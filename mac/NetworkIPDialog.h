#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>

class NetworkIPDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NetworkIPDialog(QWidget *parent = nullptr);
    ~NetworkIPDialog() override = default;

    // Returns the IP address entered by the user (only valid after Accepted)
    QString ipAddress() const;

private slots:
    void onTextChanged(const QString &text);

private:
    QLineEdit   *m_ipEdit     = nullptr;
    QPushButton *m_connectBtn = nullptr;
};
