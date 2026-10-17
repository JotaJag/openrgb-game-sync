#include "GameReceiver.h"
#include <QNetworkDatagram>
#include <cstring>

GameReceiver::GameReceiver(QObject* parent) : QObject(parent)
{
    clock.start();
    connect(&socket, &QUdpSocket::readyRead, this, &GameReceiver::readPackets);
    timeout.setInterval(100);
    connect(&timeout, &QTimer::timeout, this, [this] {
        if (active() && clock.elapsed() - lastFrame > 1500) release();
    });
    timeout.start();
}

bool GameReceiver::listen(quint16 port)
{
    return socket.bind(QHostAddress::LocalHost, port, QUdpSocket::DontShareAddress);
}

void GameReceiver::release()
{
    bool hadOwner = active();
    owner.clear();
    ownerSource = 0;
    application.clear();
    frames.clear();
    if (hadOwner && released) released();
    if (status) status(enabled ? "En espera · tu efecto habitual sigue activo" : "Desactivado · tu efecto habitual sigue activo");
}

void GameReceiver::setEnabled(bool value)
{
    enabled = value;
    release();
}

void GameReceiver::setSourceEnabled(unsigned int source, bool value)
{
    if (source < 1 || source > 3) return;
    sources[source] = value;
    if (!value && ownerSource == source) release();
}

void GameReceiver::releaseSource(unsigned int source)
{
    if (active() && ownerSource == source) release();
}

void GameReceiver::readPackets()
{
    while (socket.hasPendingDatagrams())
    {
        auto datagram = socket.receiveDatagram(GS_HEADER_SIZE + GS_MAX_COLORS * 4 + 1);
        const auto data = datagram.data();
        if (!enabled || datagram.senderAddress() != QHostAddress::LocalHost || data.size() < GS_HEADER_SIZE) continue;
        processPacket(data);
    }
}

void GameReceiver::processPacket(const QByteArray& data)
{
    if (!enabled || data.size() < GS_HEADER_SIZE) return;
    GameSyncPacket p{};
    std::memcpy(&p, data.constData(), std::min<size_t>(sizeof(p), data.size()));
    if (p.magic != GS_MAGIC || (p.version != 1 && p.version != 2)) return;
    QByteArray session(reinterpret_cast<const char*>(p.session), 16);
    if (session == QByteArray(16, '\0')) return;
    if (p.kind == 2)
    {
        if (data.size() == GS_HEADER_SIZE && session == owner) release();
        return;
    }
    const unsigned source = p.version == 1 ? 1 : (p.device >> 8);
    if (source < 1 || source > 3 || !sources[source]) return;
    p.device &= 0xff;
    if (p.kind == 3)
    {
        if (p.version != 2 || data.size() != GS_HEADER_SIZE || p.count || p.rows || p.cols ||
            p.device < 1 || p.device > 6 || session != owner || source != ownerSource) return;
        if (!frames.remove(p.device)) return;
        if (frames.isEmpty()) release();
        else if (changed) changed();
        return;
    }
    if (p.kind != 1 || p.device < 1 || p.device > 6 || p.count == 0 || p.count > GS_MAX_COLORS ||
        p.rows == 0 || p.cols == 0 || unsigned(p.rows) * p.cols != p.count ||
        data.size() != GS_HEADER_SIZE + p.count * 4) return;
    // Keep one application in control until it releases or times out.
    if (active() && owner != session) return;
    if (!active())
    {
        owner = session;
        ownerSource = source;
        application = QString::fromUtf8(p.app, int(strnlen(p.app, 64))).left(64);
        if (application.isEmpty()) application = "Juego compatible";
        const QString names[] = {"", "Chroma", "iCUE", "Lightsync"};
        if (status) status(names[source] + " · " + application);
    }
    lastFrame = clock.elapsed();
    ++receivedFrames;
    GameFrame frame{p.device, p.rows, p.cols, {}};
    for (unsigned i = 0; i < p.count; ++i) frame.colors.push_back(p.colors[i] & 0xffffff);
    const auto previous = frames.constFind(p.device);
    if (previous != frames.cend() && previous->rows == frame.rows && previous->cols == frame.cols && previous->colors == frame.colors)
    {
        ++duplicateFrames;
        return; // A heartbeat renews ownership without another hardware write.
    }
    frames[p.device] = frame;
    if (changed) changed();
}
