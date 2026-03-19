#pragma once

#include <QMainWindow>
#include <QWebEngineView>
#include <QPushButton>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString &aaPath = {},
                        const QString &serverPath = {},
                        const QString &port = {},
                        QWidget *parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onStartServer();
    void onPlayNetwork();
    void onPlaySinglePlayer();

private:
    void setupUi();
    bool launchProcess(const QString &binaryPath, const QStringList &args);

    QString         m_aaBinary;
    QString         m_serverBinary;
    QString         m_port;
    QWebEngineView *m_webView    = nullptr;
    QPushButton    *m_btnServer  = nullptr;
    QPushButton    *m_btnNetwork = nullptr;
    QPushButton    *m_btnSingle  = nullptr;
};
