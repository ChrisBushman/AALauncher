#include "NetworkIPDialog.h"
#include "ServerDiscovery.h"
#include "ServerList.h"

#include <QInputDialog>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>

NetworkIPDialog::NetworkIPDialog(const QString &defaultPort, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("AALauncher Network Connection");
    setModal(true);
    resize(420, 340);

    QVBoxLayout *vLayout = new QVBoxLayout(this);

    vLayout->addWidget(new QLabel("Known servers (double-click to connect):", this));

    m_list = new QListWidget(this);
    vLayout->addWidget(m_list, 1);

    for (const ServerEntry &entry : LoadSavedServers())
        addListRow(entry.name, entry.host, entry.port);

    QLabel *manualLabel = new QLabel("Or enter a host/IP and port directly:", this);
    vLayout->addWidget(manualLabel);

    QHBoxLayout *inputRow = new QHBoxLayout();
    m_hostEdit = new QLineEdit(this);
    m_hostEdit->setPlaceholderText("e.g. 192.168.1.100 or myserver.example.com");
    inputRow->addWidget(m_hostEdit, 1);

    m_portEdit = new QLineEdit(this);
    m_portEdit->setText(defaultPort);
    m_portEdit->setPlaceholderText("Port");
    m_portEdit->setFixedWidth(70);
    inputRow->addWidget(m_portEdit);
    vLayout->addLayout(inputRow);

    QHBoxLayout *btnRow = new QHBoxLayout();
    m_saveBtn = new QPushButton("Save as Favorite", this);
    btnRow->addWidget(m_saveBtn);
    btnRow->addStretch(1);
    m_connectBtn = new QPushButton("Connect", this);
    m_connectBtn->setEnabled(false);
    m_connectBtn->setDefault(true);
    btnRow->addWidget(m_connectBtn);
    vLayout->addLayout(btnRow);

    connect(m_hostEdit, &QLineEdit::textChanged, this, &NetworkIPDialog::onTextChanged);
    connect(m_portEdit, &QLineEdit::textChanged, this, &NetworkIPDialog::onTextChanged);
    connect(m_list, &QListWidget::itemClicked, this, &NetworkIPDialog::onListItemClicked);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &NetworkIPDialog::onListItemDoubleClicked);
    connect(m_saveBtn, &QPushButton::clicked, this, &NetworkIPDialog::onSaveClicked);
    connect(m_connectBtn, &QPushButton::clicked, this, &QDialog::accept);

    ServerDiscovery *discovery = new ServerDiscovery(this);
    connect(discovery, &ServerDiscovery::serverFound, this, &NetworkIPDialog::onServerDiscovered);
    discovery->start();
}

void NetworkIPDialog::addListRow(const QString &label, const QString &host, const QString &port)
{
    QString text = label.isEmpty() ? QString("%1:%2").arg(host, port)
                                    : QString("%1 (%2:%3)").arg(label, host, port);
    QListWidgetItem *item = new QListWidgetItem(text, m_list);
    item->setData(Qt::UserRole, host);
    item->setData(Qt::UserRole + 1, port);
}

QString NetworkIPDialog::ipAddress() const
{
    return m_hostEdit ? m_hostEdit->text().trimmed() : QString();
}

QString NetworkIPDialog::port() const
{
    return m_portEdit ? m_portEdit->text().trimmed() : QString();
}

void NetworkIPDialog::onTextChanged()
{
    m_connectBtn->setEnabled(!m_hostEdit->text().trimmed().isEmpty()
                              && !m_portEdit->text().trimmed().isEmpty());
}

void NetworkIPDialog::onListItemClicked(QListWidgetItem *item)
{
    m_hostEdit->setText(item->data(Qt::UserRole).toString());
    m_portEdit->setText(item->data(Qt::UserRole + 1).toString());
}

void NetworkIPDialog::onListItemDoubleClicked(QListWidgetItem *item)
{
    onListItemClicked(item);
    accept();
}

void NetworkIPDialog::onSaveClicked()
{
    QString host = m_hostEdit->text().trimmed();
    QString port = m_portEdit->text().trimmed();
    if (host.isEmpty() || port.isEmpty())
        return;

    bool ok = false;
    QString name = QInputDialog::getText(this, "Save Server", "Name for this server:",
                                          QLineEdit::Normal, host, &ok);
    if (!ok)
        return;

    ServerEntry entry;
    entry.name = name.isEmpty() ? host : name;
    entry.host = host;
    entry.port = port;
    AppendSavedServer(entry);
    addListRow(entry.name, entry.host, entry.port);
}

void NetworkIPDialog::onServerDiscovered(const QString &host, quint16 port, const QString &name)
{
    addListRow(QString("%1 [LAN]").arg(name), host, QString::number(port));
}
