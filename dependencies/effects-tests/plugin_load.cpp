#include <QApplication>
#include <QPluginLoader>
#include <iostream>
#include "OpenRGBPluginInterface.h"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    if (argc != 2) return 2;
    QPluginLoader loader(QString::fromLocal8Bit(argv[1]));
    auto* plugin = qobject_cast<OpenRGBPluginInterface*>(loader.instance());
    if (!plugin)
    {
        std::cerr << loader.errorString().toStdString() << '\n';
        return 1;
    }
    std::cout << "Plugin loads; API=" << plugin->GetPluginAPIVersion() << '\n';
    return plugin->GetPluginAPIVersion() == OPENRGB_PLUGIN_API_VERSION ? 0 : 1;
}
