#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPluginLoader>
#include <iostream>
int main(int argc,char** argv)
{
    QCoreApplication app(argc,argv);
    if(argc!=3)return 2;
    QFile file(argv[2]);if(!file.open(QIODevice::ReadOnly))return 3;
    const auto expected=QJsonDocument::fromJson(file.readAll()).object();
    QPluginLoader loader(argv[1]);const auto actual=loader.metaData().value("MetaData").toObject();
    if(actual.value("VersionStr")!=expected.value("VersionStr")||actual.value("Id")!=expected.value("Id")){
        std::cerr<<"Plugin metadata does not match source; rebuild the plugin.\n";return 1;
    }
}
