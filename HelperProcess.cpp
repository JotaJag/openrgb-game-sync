#include "HelperProcess.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <algorithm>

HelperProcess::HelperProcess(Config value, Policy limits, QObject* parent)
    : QObject(parent), config(std::move(value)), policy(limits)
{
    clock.start();
    monitor.setInterval(std::max(1, policy.pollMs));
    retry.setSingleShot(true);
    killDeadline.setSingleShot(true);
    process.setStandardOutputFile(QProcess::nullDevice());
    if (config.errorLog.isEmpty()) process.setStandardErrorFile(QProcess::nullDevice());
    else {
        QDir().mkpath(QFileInfo(config.errorLog).absolutePath());
        process.setStandardErrorFile(config.errorLog, QIODevice::Append);
    }
    connect(&monitor, &QTimer::timeout, this, &HelperProcess::poll);
    connect(&retry, &QTimer::timeout, this, [this] { if (wanted) start(); });
    connect(&killDeadline, &QTimer::timeout, this, [this] {
        if (process.state() != QProcess::NotRunning) process.kill();
    });
    connect(&process, &QProcess::started, this, [this] {
        if (wanted && config.statusFile.isEmpty()) {
            readyAt = lastHealthy = clock.elapsed();
            state(State::Ready);
        }
    });
    connect(&process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (wanted && error == QProcess::FailedToStart) fail(process.errorString());
    });
    connect(&process, qOverload<int,QProcess::ExitStatus>(&QProcess::finished), this, [this](int code, QProcess::ExitStatus exit) {
        killDeadline.stop();
        if (!wanted) { state(State::Stopped); return; }
        if (!failing) {
            current.error = exit == QProcess::CrashExit ? QString("Proceso interrumpido (%1)").arg(code)
                                                       : QString("El proceso terminó (%1)").arg(code);
            current.health = {};
            if (unavailable) unavailable();
        }
        scheduleRetry();
    });
}

HelperProcess::~HelperProcess()
{
    changed = {}; unavailable = {};
    setEnabled(false);
}

void HelperProcess::state(State value)
{
    if (current.state == value) return;
    current.state = value;
    if (changed) changed();
}

HelperProcess::Status HelperProcess::status() const
{
    auto result = current;
    result.pid = process.processId();
    result.retryInMs = current.state == State::WaitingRetry ? std::max<qint64>(0, retryAt - clock.elapsed()) : 0;
    return result;
}

void HelperProcess::setEnabled(bool enabled)
{
    if (wanted == enabled) return;
    wanted = enabled;
    if (enabled) {
        current = {};
        monitor.start();
        start();
        return;
    }
    retry.stop(); monitor.stop(); killDeadline.stop();
    failing = false;
    if (process.state() != QProcess::NotRunning) {
        process.terminate();
        if (!process.waitForFinished(policy.stopMs)) {
            process.kill(); process.waitForFinished(1000);
        }
    }
    current.health = {};
    state(State::Stopped);
}

void HelperProcess::start()
{
    if (!wanted || process.state() != QProcess::NotRunning) return;
    failing = stableReset = false;
    attemptAt = lastHealthy = clock.elapsed();
    attemptWallMs = QDateTime::currentMSecsSinceEpoch();
    current.health = {};
    state(State::Starting);
    process.start(config.program, config.arguments);
}

bool HelperProcess::readHealth(QJsonObject& value, qint64& ageMs) const
{
    ageMs = 0;
    if (config.statusFile.isEmpty()) return process.state() == QProcess::Running;
    QFile file(config.statusFile);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 65536) return false;
    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) return false;
    value = document.object();
    const auto modified = QFileInfo(file).lastModified().toMSecsSinceEpoch();
    const auto now = QDateTime::currentMSecsSinceEpoch();
    const auto updated = qint64(value.value("updated_at").toDouble()) * 1000;
    ageMs = std::max<qint64>(0, now - modified);
    // A stale file from the previous process must never count as startup readiness.
    return value.value("running").toBool() && modified >= attemptWallMs && modified <= now + 2000 &&
           now - modified <= policy.heartbeatMs && updated >= attemptWallMs - 1000 && updated <= now + 2000;
}

void HelperProcess::poll()
{
    if (!wanted || failing || process.state() != QProcess::Running) return;
    QJsonObject health;
    qint64 ageMs = 0;
    const auto now = clock.elapsed();
    if (readHealth(health, ageMs)) {
        lastHealthy = now - ageMs;
        current.health = health;
        if (current.state != State::Ready) {
            readyAt = now;
            state(State::Ready);
        }
        if (!stableReset && now - readyAt >= policy.stableMs) {
            current.retries = 0;
            current.error.clear();
            stableReset = true;
        }
    }
    if (current.state == State::Starting && now - attemptAt >= policy.startupMs)
        fail("El servicio no confirmó su arranque a tiempo");
    else if (current.state == State::Ready && now - lastHealthy >= policy.heartbeatMs)
        fail("El servicio dejó de actualizar su estado");
}

void HelperProcess::fail(const QString& reason)
{
    if (!wanted || failing) return;
    failing = true;
    current.error = reason;
    current.health = {};
    state(State::Recovering);
    if (unavailable) unavailable();
    if (process.state() == QProcess::NotRunning) scheduleRetry();
    else {
        process.terminate();
        killDeadline.start(policy.stopMs);
    }
}

void HelperProcess::scheduleRetry()
{
    failing = false;
    current.health = {};
    if (!wanted) { state(State::Stopped); return; }
    if (current.retries >= policy.retries) {
        monitor.stop();
        state(State::Exhausted);
        return;
    }
    qint64 delay = std::max(1, policy.firstRetryMs);
    for (int i = 0; i < current.retries; ++i) delay = std::min<qint64>(policy.maxRetryMs, delay * 2);
    delay = std::min<qint64>(std::max(1, policy.maxRetryMs), delay);
    ++current.retries;
    retryAt = clock.elapsed() + delay;
    state(State::WaitingRetry);
    retry.start(int(delay));
}
