QT += core gui widgets network
TEMPLATE = lib
CONFIG += plugin c++17
TARGET = OpenRGBGameSyncPlugin
INCLUDEPATH += ../openrgb-sequence-fix/OpenRGB ../openrgb-sequence-fix/OpenRGB/RGBController ../openrgb-sequence-fix/OpenRGB/dependencies/json
SOURCES += GameReceiver.cpp GameSyncPlugin.cpp HelperProcess.cpp
HEADERS += GameReceiver.h GameSyncPlugin.h HelperProcess.h PriorityLease.h protocol.h
DISTFILES += plugin.json
# qmake does not otherwise track the JSON read by Q_PLUGIN_METADATA reliably.
metadata_dependency.target = moc_GameSyncPlugin.cpp
metadata_dependency.depends = $$PWD/plugin.json
QMAKE_EXTRA_TARGETS += metadata_dependency
