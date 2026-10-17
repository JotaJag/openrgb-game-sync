#pragma once
#include <QObject>
#include <QUdpSocket>
#include <QElapsedTimer>
#include <QTimer>
#include <QMap>
#include <QVector>
#include <functional>
#include "protocol.h"

struct GameFrame
{
    unsigned int device, rows, cols;
    QVector<unsigned int> colors;
};

class GameReceiver : public QObject
{
    Q_OBJECT
    QUdpSocket socket;
    QTimer timeout;
    QElapsedTimer clock;
    QByteArray owner;
    QString application;
    qint64 lastFrame = 0;
    bool enabled = true;
    bool sources[4] = {false, true, true, true};
    unsigned int ownerSource = 0;
    void readPackets();
public:
    QMap<unsigned int, GameFrame> frames;
    std::function<void()> changed;
    std::function<void()> released;
    std::function<void(QString)> status;
    explicit GameReceiver(QObject* parent = nullptr);
    bool listen(quint16 port = GS_PORT);
    void setEnabled(bool value);
    void setSourceEnabled(unsigned int source, bool value);
    void release();
    bool active() const { return !owner.isEmpty(); }
    QString appName() const { return application; }
    quint16 port() const { return socket.localPort(); }
};
