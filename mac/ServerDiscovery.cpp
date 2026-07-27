#include "ServerDiscovery.h"

#include <QByteArray>
#include <QHostAddress>
#include <QStringList>
#include <QTimer>
#include <QUdpSocket>
#include <cstring>

/* Protocol matches AAServer's DiscoveryServerLoop (AAServer.cpp) and
   ipxserver.h's DEFAULT_DISCOVERY_PORT/DISCOVERY_REQUEST_MAGIC/
   DISCOVERY_REPLY_PREFIX -- kept as literals here rather than a shared
   header since the launcher and server are separate repos/build systems. */
static const quint16 kDiscoveryPort = 21399;
static const char *kRequestMagic = "AASERVER_DISCOVER";
static const char *kReplyPrefix = "AASERVER_HERE:";

ServerDiscovery::ServerDiscovery(QObject *parent)
    : QObject(parent)
{
}

void ServerDiscovery::start(int timeoutMs)
{
    m_socket = new QUdpSocket(this);
    m_socket->bind(QHostAddress::AnyIPv4, 0, QUdpSocket::ShareAddress);
    connect(m_socket, &QUdpSocket::readyRead, this, &ServerDiscovery::onReadyRead);

    m_socket->writeDatagram(kRequestMagic, strlen(kRequestMagic),
                             QHostAddress::Broadcast, kDiscoveryPort);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &ServerDiscovery::onTimeout);
    m_timer->start(timeoutMs);
}

void ServerDiscovery::onReadyRead()
{
    while (m_socket->hasPendingDatagrams()) {
        QByteArray buf;
        buf.resize((int)m_socket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort = 0;
        m_socket->readDatagram(buf.data(), buf.size(), &sender, &senderPort);

        QString text = QString::fromLatin1(buf);
        if (!text.startsWith(kReplyPrefix))
            continue;

        QString rest = text.mid((int)strlen(kReplyPrefix));
        QStringList parts = rest.split(':');
        if (parts.size() < 2)
            continue;
        bool ok = false;
        quint16 port = (quint16)parts[0].toUInt(&ok);
        if (!ok)
            continue;
        QString name = parts[1];

        emit serverFound(sender.toString(), port, name);
    }
}

void ServerDiscovery::onTimeout()
{
    m_socket->close();
    emit finished();
    deleteLater();
}
