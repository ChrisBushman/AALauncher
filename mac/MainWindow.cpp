#include "MainWindow.h"
#include "NetworkIPDialog.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QMessageBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

static QString defaultBinary(const QString &name)
{
    return QCoreApplication::applicationDirPath() + QDir::separator() + name;
}

MainWindow::MainWindow(const QString &aaPath, const QString &serverPath, const QString &port, QWidget *parent)
    : QMainWindow(parent)
    , m_aaBinary(aaPath.isEmpty() ? defaultBinary("AA") : aaPath)
    , m_serverBinary(serverPath.isEmpty() ? defaultBinary("AAServer") : serverPath)
    , m_port(port)
{
    setupUi();
}

void MainWindow::setupUi()
{
    setWindowTitle("Amulets & Armor macOS Launcher v1.00");
    // Match the Windows client size exactly
    setFixedSize(1049, 562);

    QWidget *central = new QWidget(this);
    setCentralWidget(central);

    QVBoxLayout *vLayout = new QVBoxLayout(central);
    vLayout->setContentsMargins(0, 0, 0, 0);
    vLayout->setSpacing(0);

    // Web view fills all space above the button bar
    m_webView = new QWebEngineView(this);
    m_webView->setUrl(QUrl("http://www.amuletsandarmor.com/index.htm?launcher=1&classic=1"));
    vLayout->addWidget(m_webView, 1);

    // Bottom button bar (100px), with absolute button geometry matching the Windows source
    QWidget *buttonBar = new QWidget(this);
    buttonBar->setFixedHeight(100);
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

    connect(m_btnServer,  &QPushButton::clicked, this, &MainWindow::onStartServer);
    connect(m_btnNetwork, &QPushButton::clicked, this, &MainWindow::onPlayNetwork);
    connect(m_btnSingle,  &QPushButton::clicked, this, &MainWindow::onPlaySinglePlayer);
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
    QStringList args;
    if (!m_port.isEmpty())
        args << m_port;
    launchProcess(m_serverBinary, args);
}

void MainWindow::onPlayNetwork()
{
    NetworkIPDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QString ip = dlg.ipAddress();
        if (ip.isEmpty())
            return;
        QStringList args = {ip};
        if (!m_port.isEmpty())
            args << m_port;
        if (launchProcess(m_aaBinary, args))
            close();
    }
}

void MainWindow::onPlaySinglePlayer()
{
    if (launchProcess(m_aaBinary, {}))
        close();
}
