#pragma once

#include <QMainWindow>
#include <QWebEngineView>
#include <QLineEdit>
#include <QPushButton>

class ScriptCompilerWindow;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString &aaPath = {},
                        const QString &serverPath = {},
                        const QString &scriptCompilerPath = {},
                        const QString &port = {},
                        QWidget *parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onStartServer();
    void onPlayNetwork();
    void onPlaySinglePlayer();
    void onScriptCompiler();
    void onDisplaySettings();

private:
    void setupUi();
    bool launchProcess(const QString &binaryPath, const QStringList &args);

    QString         m_aaBinary;
    QString         m_serverBinary;
    QString         m_scriptCompilerBinary;
    QWebEngineView *m_webView    = nullptr;
    QPushButton    *m_btnServer  = nullptr;
    QPushButton    *m_btnNetwork = nullptr;
    QPushButton    *m_btnSingle  = nullptr;
    QPushButton    *m_btnScriptCompiler = nullptr;
    QPushButton    *m_btnExit    = nullptr;
    QLineEdit      *m_portEdit   = nullptr;
    ScriptCompilerWindow *m_scriptCompilerWindow = nullptr;
};
