#include "MainWindow.h"
#include "DisplaySettingsDialog.h"
#include "NetworkIPDialog.h"
#include "ScriptCompilerWindow.h"
#include "Updater.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QProgressDialog>
#include <QPushButton>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

static QString defaultBinary(const QString &name)
{
    return QCoreApplication::applicationDirPath() + QDir::separator() + name;
}

MainWindow::MainWindow(const QString &aaPath, const QString &serverPath,
                       const QString &scriptCompilerPath, const QString &port, QWidget *parent)
    : QMainWindow(parent)
    , m_aaBinary(aaPath.isEmpty() ? defaultBinary("AA") : aaPath)
    , m_serverBinary(serverPath.isEmpty() ? defaultBinary("AAServer") : serverPath)
    , m_scriptCompilerBinary(scriptCompilerPath.isEmpty() ? defaultBinary("AAScriptCompiler") : scriptCompilerPath)
{
    setupUi();
    if (!port.isEmpty())
        m_portEdit->setText(port);
}

void MainWindow::setupUi()
{
    setWindowTitle("Amulets & Armor macOS Launcher v1.00");
    // Match the Windows client's web-view width; extra height below for the
    // 2-row button bar (original 3 buttons, plus port/script-compiler/exit).
    setFixedSize(1049, 622);

    QWidget *central = new QWidget(this);
    setCentralWidget(central);

    QVBoxLayout *vLayout = new QVBoxLayout(central);
    vLayout->setContentsMargins(0, 0, 0, 0);
    vLayout->setSpacing(0);

    // Web view fills all space above the button bar
    m_webView = new QWebEngineView(this);
    m_webView->setUrl(QUrl("http://www.amuletsandarmor.com/index.htm?launcher=1&classic=1"));
    vLayout->addWidget(m_webView, 1);

    // Bottom button bar (160px): original 3-button row on top, a second row
    // below with the server port field, script compiler, and exit.
    QWidget *buttonBar = new QWidget(this);
    buttonBar->setFixedHeight(160);
    vLayout->addWidget(buttonBar, 0);

    QFont btnFont;
    btnFont.setBold(true);

    // Positions from Windows InitializeComponent: x=148, x=406, x=665; size 234x58; top=21
    m_btnServer = new QPushButton("Start A&&A Server", buttonBar);
    m_btnServer->setFont(btnFont);
    m_btnServer->setGeometry(148, 21, 234, 58);

    m_btnNetwork = new QPushButton("Play Network Game", buttonBar);
    m_btnNetwork->setFont(btnFont);
    m_btnNetwork->setGeometry(406, 21, 234, 58);

    m_btnSingle = new QPushButton("Play Single Player", buttonBar);
    m_btnSingle->setFont(btnFont);
    m_btnSingle->setGeometry(665, 21, 234, 58);

    QLabel *portLabel = new QLabel("Server Port:", buttonBar);
    portLabel->setGeometry(148, 100, 90, 24);

    m_portEdit = new QLineEdit(buttonBar);
    m_portEdit->setText("21300");
    m_portEdit->setGeometry(240, 97, 70, 28);

    m_btnScriptCompiler = new QPushButton("Script Compiler", buttonBar);
    m_btnScriptCompiler->setGeometry(406, 92, 234, 40);

    m_btnExit = new QPushButton("Exit", buttonBar);
    m_btnExit->setGeometry(665, 92, 234, 40);

    connect(m_btnServer,  &QPushButton::clicked, this, &MainWindow::onStartServer);
    connect(m_btnNetwork, &QPushButton::clicked, this, &MainWindow::onPlayNetwork);
    connect(m_btnSingle,  &QPushButton::clicked, this, &MainWindow::onPlaySinglePlayer);
    connect(m_btnScriptCompiler, &QPushButton::clicked, this, &MainWindow::onScriptCompiler);
    connect(m_btnExit, &QPushButton::clicked, this, &QWidget::close);

    QMenu *optionsMenu = menuBar()->addMenu("Options");
    QAction *displaySettingsAction = optionsMenu->addAction("Display Settings...");
    connect(displaySettingsAction, &QAction::triggered, this, &MainWindow::onDisplaySettings);
    QAction *checkUpdatesAction = optionsMenu->addAction("Check for Updates...");
    connect(checkUpdatesAction, &QAction::triggered, this, &MainWindow::onCheckForUpdates);

    // A quiet check once at startup: never interrupts on failure or when
    // already current, and honors a per-version "skip" choice.
    QTimer::singleShot(0, this, [this] { startUpdateCheck(/*silent=*/true); });
}

bool MainWindow::launchProcess(const QString &binaryPath, const QStringList &args)
{
    QString workDir = QFileInfo(binaryPath).absolutePath();

    /* AA/AAServer link against bundled SDL dylibs by absolute build-time
       path (see their own run.sh wrappers); dyld only finds them via
       DYLD_LIBRARY_PATH pointed at the lib/ folder shipped alongside each
       binary, since we're exec'ing them directly rather than through a
       shell wrapper. */
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString libDir = workDir + QDir::separator() + "lib";
    QString existing = env.value("DYLD_LIBRARY_PATH");
    env.insert("DYLD_LIBRARY_PATH", existing.isEmpty() ? libDir : libDir + ":" + existing);

    QProcess *process = new QProcess(this);
    process->setProgram(binaryPath);
    process->setArguments(args);
    process->setWorkingDirectory(workDir);
    process->setProcessEnvironment(env);
    bool ok = process->startDetached();
    if (!ok) {
        QMessageBox::critical(this, "Launch Error",
            QString("Failed to launch: %1").arg(binaryPath));
    }
    process->deleteLater();
    return ok;
}

void MainWindow::onStartServer()
{
    /* --console: AAServer is spawned here via QProcess::startDetached(),
       which leaves it with no controlling terminal at all -- with no
       flag, its output goes nowhere and it shows up as an anonymous
       background process with no visible window. The flag tells AAServer
       to relaunch itself into a real Terminal.app window; harmless no-op
       on platforms/builds where a console already exists. */
    QStringList args = {"--console"};
    QString port = m_portEdit->text().trimmed();
    if (!port.isEmpty())
        args << port;
    launchProcess(m_serverBinary, args);
}

void MainWindow::onPlayNetwork()
{
    NetworkIPDialog dlg(m_portEdit->text().trimmed(), this);
    if (dlg.exec() == QDialog::Accepted) {
        QString ip = dlg.ipAddress();
        if (ip.isEmpty())
            return;
        QStringList args = {ip};
        QString port = dlg.port();
        if (!port.isEmpty())
            args << port;
        if (launchProcess(m_aaBinary, args))
            close();
    }
}

void MainWindow::onPlaySinglePlayer()
{
    if (launchProcess(m_aaBinary, {}))
        close();
}

void MainWindow::onScriptCompiler()
{
    if (!m_scriptCompilerWindow)
        m_scriptCompilerWindow = new ScriptCompilerWindow(m_scriptCompilerBinary);
    m_scriptCompilerWindow->show();
    m_scriptCompilerWindow->raise();
    m_scriptCompilerWindow->activateWindow();
}

void MainWindow::onDisplaySettings()
{
    DisplaySettingsDialog dlg(m_aaBinary, this);
    dlg.exec();
}

void MainWindow::onCheckForUpdates()
{
    startUpdateCheck(/*silent=*/false);
}

void MainWindow::startUpdateCheck(bool silent)
{
    if (!silent && !Updater::isInstalledBundle()) {
        QMessageBox::information(this, "Check for Updates",
            "Updating in place only works when running the installed "
            "Amulets & Armor app. This looks like a development build.");
        return;
    }
    if (!Updater::isInstalledBundle())
        return;  // silent startup check from a dev build: stay quiet

    if (!m_updater)
        m_updater = new Updater(this);

    connect(m_updater, &Updater::checkFinished, this,
            [this, silent](const UpdateInfo &info) {
        if (info.updateAvailable) {
            // Respect a prior "Skip This Version" -- but only for the silent
            // startup check; an explicit menu check always shows the offer.
            if (silent) {
                QSettings settings;
                if (settings.value("updates/skipVersion").toString() == info.latestVersion)
                    return;
            }
            offerUpdate(info);
        } else if (!silent) {
            QMessageBox::information(this, "Check for Updates",
                QString("You're up to date.\n\nAmulets & Armor %1 is the latest version.")
                    .arg(info.currentVersion.isEmpty() ? info.latestVersion : info.currentVersion));
        }
    }, Qt::SingleShotConnection);

    connect(m_updater, &Updater::checkFailed, this, [this, silent](const QString &err) {
        if (!silent)
            QMessageBox::warning(this, "Check for Updates",
                "Couldn't check for updates:\n\n" + err);
    }, Qt::SingleShotConnection);

    m_updater->checkForUpdates();
}

void MainWindow::offerUpdate(const UpdateInfo &info)
{
    const QString current = info.currentVersion.isEmpty() ? "unknown" : info.currentVersion;
    QMessageBox box(this);
    box.setWindowTitle("Update Available");
    box.setIcon(QMessageBox::Question);
    box.setText(QString("Amulets & Armor %1 is available.").arg(info.latestVersion));
    box.setInformativeText(
        QString("You have version %1.\n\nYour characters, saved games, and "
                "settings are backed up and carried over automatically.")
            .arg(current));
    QPushButton *updateBtn = box.addButton("Update Now", QMessageBox::AcceptRole);
    QPushButton *notesBtn  = box.addButton("Release Notes", QMessageBox::ActionRole);
    QPushButton *skipBtn   = box.addButton("Skip This Version", QMessageBox::DestructiveRole);
    box.addButton("Later", QMessageBox::RejectRole);
    box.setDefaultButton(updateBtn);

    box.exec();

    if (box.clickedButton() == notesBtn) {
        if (!info.releaseUrl.isEmpty())
            QDesktopServices::openUrl(QUrl(info.releaseUrl));
        // Re-offer after they've looked at the notes.
        offerUpdate(info);
        return;
    }
    if (box.clickedButton() == skipBtn) {
        QSettings settings;
        settings.setValue("updates/skipVersion", info.latestVersion);
        return;
    }
    if (box.clickedButton() != updateBtn)
        return;  // Later / closed

    // Run the download+install behind a progress dialog.
    QProgressDialog *progress = new QProgressDialog(
        "Preparing...", "Cancel", 0, 0, this);
    progress->setWindowTitle("Updating Amulets & Armor");
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->setAutoClose(false);
    progress->setAutoReset(false);

    connect(m_updater, &Updater::statusChanged, progress, &QProgressDialog::setLabelText);
    connect(m_updater, &Updater::downloadProgress, progress,
            [progress](qint64 received, qint64 total) {
        if (total > 0) {
            progress->setMaximum(static_cast<int>(total / 1024));
            progress->setValue(static_cast<int>(received / 1024));
        }
    });
    connect(progress, &QProgressDialog::canceled, m_updater, &Updater::cancel);

    connect(m_updater, &Updater::updateFailed, this, [this, progress](const QString &err) {
        progress->close();
        progress->deleteLater();
        if (!err.isEmpty())  // empty == user canceled
            QMessageBox::critical(this, "Update Failed", err);
    }, Qt::SingleShotConnection);

    connect(m_updater, &Updater::readyToRelaunch, this, [progress] {
        progress->close();
        progress->deleteLater();
        // Quit so the detached helper can replace the running bundle and
        // relaunch it. The helper is already spawned and waiting on our PID.
        QCoreApplication::quit();
    }, Qt::SingleShotConnection);

    m_updater->performUpdate(info);
}
