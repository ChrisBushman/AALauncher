#pragma once

#include <QObject>
#include <QString>

class QUdpSocket;
class QTimer;

// Broadcasts a LAN "is anybody there" probe (see AAServer's
// DiscoveryServerLoop / DEFAULT_DISCOVERY_PORT) and reports each reply as
// it arrives. Fire-and-forget: call start(), connect to serverFound()/
// finished(), and let it self-destruct via deleteLater() after finished().
class ServerDiscovery : public QObject
{
    Q_OBJECT

public:
    explicit ServerDiscovery(QObject *parent = nullptr);

    void start(int timeoutMs = 1500);

signals:
    void serverFound(const QString &host, quint16 port, const QString &name);
    void finished();

private slots:
    void onReadyRead();
    void onTimeout();

private:
    QUdpSocket *m_socket = nullptr;
    QTimer *m_timer = nullptr;
};
