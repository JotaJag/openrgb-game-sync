QT += core gui widgets network
TEMPLATE = lib
CONFIG += plugin c++17
TARGET = OpenRGBGameSyncPlugin
INCLUDEPATH += ../openrgb-sequence-fix/OpenRGB ../openrgb-sequence-fix/OpenRGB/RGBController ../openrgb-sequence-fix/OpenRGB/dependencies/json
SOURCES += GameReceiver.cpp GameSyncPlugin.cpp
HEADERS += GameReceiver.h GameSyncPlugin.h PriorityLease.h protocol.h
