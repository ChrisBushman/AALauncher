#pragma once

#include <QMainWindow>
#include <QWebEngineView>
#include <QLineEdit>
#include <QPushButton>

class ScriptCompilerWindow;
class Updater;
struct UpdateInfo;

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
    void onCheckForUpdates();

private:
    void setupUi();
    bool launchProcess(const QString &binaryPath, const QStringList &args);

    // Update flow. checkForUpdates(silent): on startup, silent=true so a
    // failed check or an up-to-date result says nothing; the menu item passes
    // silent=false so the user always gets a reply. offerUpdate() prompts and,
    // on accept, runs the download/install with a progress dialog.
    void startUpdateCheck(bool silent);
    void offerUpdate(const UpdateInfo &info);

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
    Updater        *m_updater  = nullptr;
};
