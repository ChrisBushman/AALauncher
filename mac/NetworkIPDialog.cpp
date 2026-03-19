#include "NetworkIPDialog.h"

#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>

NetworkIPDialog::NetworkIPDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("AALauncher Network Connection");
    setFixedSize(390, 100);
    setModal(true);

    QVBoxLayout *vLayout = new QVBoxLayout(this);

    QLabel *label = new QLabel("Enter Network IP of Server:", this);
    vLayout->addWidget(label);

    QHBoxLayout *inputRow = new QHBoxLayout();

    m_ipEdit = new QLineEdit(this);
    m_ipEdit->setPlaceholderText("e.g. 192.168.1.100");
    inputRow->addWidget(m_ipEdit);

    m_connectBtn = new QPushButton("Connect", this);
    m_connectBtn->setEnabled(false);
    m_connectBtn->setDefault(true);
    inputRow->addWidget(m_connectBtn);

    vLayout->addLayout(inputRow);

    connect(m_ipEdit,     &QLineEdit::textChanged, this, &NetworkIPDialog::onTextChanged);
    connect(m_connectBtn, &QPushButton::clicked,   this, &QDialog::accept);
}

QString NetworkIPDialog::ipAddress() const
{
    return m_ipEdit ? m_ipEdit->text().trimmed() : QString();
}

void NetworkIPDialog::onTextChanged(const QString &text)
{
    m_connectBtn->setEnabled(!text.trimmed().isEmpty());
}
