#include "../HelperProcess.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>
#include <cassert>
#include <csignal>
#include <iostream>

static bool until(const std::function<bool()>& condition,int timeout=2500)
{
    QElapsedTimer timer;timer.start();
    while(timer.elapsed()<timeout){QCoreApplication::processEvents();if(condition())return true;QThread::msleep(2);}
    return condition();
}
static void wait(int ms){until([]{return false;},ms);}
static void write(const QString& path,const QByteArray& data){QFile f(path);assert(f.open(QIODevice::WriteOnly));assert(f.write(data)==data.size());}
static int launches(const QString& path){QFile f(path);if(!f.open(QIODevice::ReadOnly))return 0;return f.readAll().toInt();}

int main(int argc,char** argv)
{
    QCoreApplication app(argc,argv);QTemporaryDir temp;assert(temp.isValid());
    const auto script=temp.path()+"/helper.py";
    write(script,R"PY(import json,pathlib,sys,time
status,count,mode=map(str,sys.argv[1:])
counter=pathlib.Path(count)
n=int(counter.read_text())+1 if counter.exists() else 1
counter.write_text(str(n))
if mode=='crash' or (mode=='crash_once' and n==1): sys.exit(7)
while True:
    if mode!='silent':
        p=pathlib.Path(status);tmp=p.with_suffix('.tmp')
        tmp.write_text(json.dumps({'running':True,'updated_at':int(time.time()),'generation':n}))
        tmp.replace(p)
    if mode=='stall':
        while True: time.sleep(.1)
    time.sleep(.02)
)PY");
    const auto python=QStandardPaths::findExecutable("python3");assert(!python.isEmpty());
    HelperProcess::Policy policy;
    policy.pollMs=10;policy.retries=2;policy.firstRetryMs=40;policy.maxRetryMs=80;
    policy.startupMs=600;policy.heartbeatMs=200;policy.stableMs=300;policy.stopMs=100;
    auto config=[&](const QString& name,const QString& mode){
        return HelperProcess::Config{python,{script,temp.path()+"/"+name+".json",temp.path()+"/"+name+".count",mode},temp.path()+"/"+name+".json",{}};
    };
    using State=HelperProcess::State;
    {
        HelperProcess helper(config("recover","crash_once"),policy);int lost=0;
        helper.unavailable=[&]{++lost;};helper.setEnabled(true);
        assert(until([&]{return helper.status().state==State::Ready;}));
        assert(lost==1&&helper.status().retries==1&&launches(temp.path()+"/recover.count")==2);
        assert(helper.status().health.value("generation").toInt()==2);
        assert(until([&]{return helper.status().retries==0;}));
        assert(helper.status().error.isEmpty());
        const auto pid=helper.status().pid;assert(pid>0);::kill(pid,SIGKILL);
        assert(until([&]{return helper.status().state==State::Ready&&helper.status().pid!=pid;}));
        assert(helper.status().retries==1&&lost==2);
        helper.setEnabled(false);wait(150);assert(helper.status().state==State::Stopped&&lost==2);
        std::cout<<"PASS: crashed child recovers; healthy uptime resets its retry budget; intentional stop does not retry\n";
    }
    {
        HelperProcess helper(config("budget","crash"),policy);helper.setEnabled(true);
        assert(until([&]{return helper.status().state==State::Exhausted;}));
        assert(launches(temp.path()+"/budget.count")==3&&helper.status().retries==2);
        helper.setEnabled(true);wait(200);assert(launches(temp.path()+"/budget.count")==3);
        helper.setEnabled(false);helper.setEnabled(true);
        assert(until([&]{return helper.status().state==State::Exhausted;}));
        assert(launches(temp.path()+"/budget.count")==6);
        std::cout<<"PASS: repeated failures exhaust a bounded budget; disable/enable explicitly retries\n";
    }
    {
        auto limits=policy;limits.retries=0;limits.startupMs=250;
        const auto cfg=config("old","silent");
        write(cfg.statusFile,QJsonDocument(QJsonObject{{"running",true},{"updated_at",double(QDateTime::currentSecsSinceEpoch()-10)}}).toJson());
        QFile file(cfg.statusFile);assert(file.open(QIODevice::ReadWrite));
        assert(file.setFileTime(QDateTime::currentDateTime().addSecs(-10),QFileDevice::FileModificationTime));file.close();
        HelperProcess helper(cfg,limits);bool wasReady=false;
        helper.changed=[&]{wasReady|=helper.status().state==State::Ready;};helper.setEnabled(true);
        assert(until([&]{return helper.status().state==State::Exhausted;}));assert(!wasReady);
        std::cout<<"PASS: old running=true file cannot prove that a new process started\n";
    }
    {
        auto limits=policy;limits.retries=0;
        HelperProcess helper(config("stall","stall"),limits);int lost=0;
        helper.unavailable=[&]{++lost;};helper.setEnabled(true);
        assert(until([&]{return helper.status().state==State::Ready;}));
        assert(until([&]{return helper.status().state==State::Exhausted;}));
        assert(lost==1&&helper.status().health.isEmpty());
        std::cout<<"PASS: stopped heartbeat invalidates readiness and stops the child\n";
    }
    {
        auto limits=policy;limits.firstRetryMs=limits.maxRetryMs=400;
        HelperProcess helper(config("cancel","crash"),limits);helper.setEnabled(true);
        assert(until([&]{return helper.status().state==State::WaitingRetry;}));
        assert(helper.status().retryInMs>0);helper.setEnabled(false);wait(500);
        assert(launches(temp.path()+"/cancel.count")==1&&helper.status().state==State::Stopped);
        std::cout<<"PASS: disabling while waiting cancels the pending restart\n";
    }
    {
        HelperProcess helper({"/nonexistent/openrgb-test-helper",{},{},{}},policy);int lost=0;
        helper.unavailable=[&]{++lost;};helper.setEnabled(true);
        assert(until([&]{return helper.status().state==State::Exhausted;}));assert(lost==3);
        std::cout<<"PASS: launch failure follows the same bounded recovery path\n";
    }
    {
        auto cfg=config("rest","silent");cfg.statusFile.clear();
        HelperProcess helper(cfg,policy);helper.setEnabled(true);
        assert(until([&]{return helper.status().state==State::Ready;}));helper.setEnabled(false);
        std::cout<<"PASS: helpers without heartbeat files are monitored for process exit\n";
    }
}
