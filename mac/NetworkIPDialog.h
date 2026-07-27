#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>

// "Join Network Game" dialog: pick a saved or LAN-discovered server from a
// list, or type a host/IP + port directly. See ServerList.h (saved
// favorites, servers.txt) and ServerDiscovery.h (LAN broadcast probe).
class NetworkIPDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NetworkIPDialog(const QString &defaultPort, QWidget *parent = nullptr);

    QString ipAddress() const;
    QString port() const;

private slots:
    void onTextChanged();
    void onListItemClicked(QListWidgetItem *item);
    void onListItemDoubleClicked(QListWidgetItem *item);
    void onSaveClicked();
    void onServerDiscovered(const QString &host, quint16 port, const QString &name);

private:
    void addListRow(const QString &label, const QString &host, const QString &port);

    QListWidget *m_list = nullptr;
    QLineEdit   *m_hostEdit = nullptr;
    QLineEdit   *m_portEdit = nullptr;
    QPushButton *m_connectBtn = nullptr;
    QPushButton *m_saveBtn = nullptr;
};
