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

void GameReceiver::readPackets()
{
    while (socket.hasPendingDatagrams())
    {
        auto datagram = socket.receiveDatagram(GS_HEADER_SIZE + GS_MAX_COLORS * 4 + 1);
        const auto data = datagram.data();
        if (!enabled || datagram.senderAddress() != QHostAddress::LocalHost || data.size() < GS_HEADER_SIZE) continue;
        GameSyncPacket p{};
        std::memcpy(&p, data.constData(), std::min<size_t>(sizeof(p), data.size()));
        if (p.magic != GS_MAGIC || (p.version != 1 && p.version != 2)) continue;
        QByteArray session(reinterpret_cast<const char*>(p.session), 16);
        if (session == QByteArray(16, '\0')) continue;
        if (p.kind == 2)
        {
            if (data.size() == GS_HEADER_SIZE && session == owner) release();
            continue;
        }
        const unsigned source = p.version == 1 ? 1 : (p.device >> 8);
        if (source < 1 || source > 3 || !sources[source]) continue;
        p.device &= 0xff;
        if (p.kind != 1 || p.device < 1 || p.device > 6 || p.count == 0 || p.count > GS_MAX_COLORS ||
            p.rows == 0 || p.cols == 0 || unsigned(p.rows) * p.cols != p.count ||
            data.size() != GS_HEADER_SIZE + p.count * 4) continue;
        // Keep one application in control until it releases or times out.
        if (active() && owner != session) continue;
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
        GameFrame frame{p.device, p.rows, p.cols, {}};
        for (unsigned i = 0; i < p.count; ++i) frame.colors.push_back(p.colors[i] & 0xffffff);
        frames[p.device] = frame;
        if (changed) changed();
    }
}
