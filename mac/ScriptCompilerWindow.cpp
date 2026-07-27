#include "ScriptCompilerWindow.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QVBoxLayout>

ScriptCompilerWindow::ScriptCompilerWindow(const QString &compilerPath, QWidget *parent)
    : QWidget(parent)
    , m_compilerPath(compilerPath)
{
    setWindowTitle("Amulets & Armor Script Compiler");
    resize(640, 480);
    // A closed/open script compiler window shouldn't affect the launcher's
    // own quit behavior -- only MainWindow closing should end the app.
    setAttribute(Qt::WA_QuitOnClose, false);

    QVBoxLayout *vLayout = new QVBoxLayout(this);

    QHBoxLayout *scriptRow = new QHBoxLayout();
    scriptRow->addWidget(new QLabel("Script file:", this));
    m_scriptEdit = new QLineEdit(this);
    scriptRow->addWidget(m_scriptEdit, 1);
    QPushButton *scriptBrowse = new QPushButton("Browse...", this);
    scriptRow->addWidget(scriptBrowse);
    vLayout->addLayout(scriptRow);

    QHBoxLayout *outputRow = new QHBoxLayout();
    outputRow->addWidget(new QLabel("Output file:", this));
    m_outputEdit = new QLineEdit(this);
    outputRow->addWidget(m_outputEdit, 1);
    QPushButton *outputBrowse = new QPushButton("Browse...", this);
    outputRow->addWidget(outputBrowse);
    vLayout->addLayout(outputRow);

    m_compileBtn = new QPushButton("Compile", this);
    vLayout->addWidget(m_compileBtn);

    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setPlaceholderText("Compiler output will appear here...");
    vLayout->addWidget(m_log, 1);

    connect(scriptBrowse, &QPushButton::clicked, this, &ScriptCompilerWindow::onBrowseScript);
    connect(outputBrowse, &QPushButton::clicked, this, &ScriptCompilerWindow::onBrowseOutput);
    connect(m_compileBtn, &QPushButton::clicked, this, &ScriptCompilerWindow::onCompile);
}

void ScriptCompilerWindow::onBrowseScript()
{
    QString path = QFileDialog::getOpenFileName(this, "Select script file", QString(),
                                                  "Script files (*.SRC *.SRP *.src *.srp);;All files (*)");
    if (path.isEmpty())
        return;
    m_scriptEdit->setText(path);

    if (m_outputEdit->text().isEmpty()) {
        QFileInfo info(path);
        m_outputEdit->setText(info.absolutePath() + "/" + info.completeBaseName() + ".OUT");
    }
}

void ScriptCompilerWindow::onBrowseOutput()
{
    QString path = QFileDialog::getSaveFileName(this, "Select output file", m_outputEdit->text());
    if (path.isEmpty())
        return;
    m_outputEdit->setText(path);
}

void ScriptCompilerWindow::onCompile()
{
    QString scriptPath = m_scriptEdit->text().trimmed();
    QString outputPath = m_outputEdit->text().trimmed();
    if (scriptPath.isEmpty() || outputPath.isEmpty()) {
        QMessageBox::warning(this, "Script Compiler", "Please choose both a script file and an output file.");
        return;
    }
    if (!QFileInfo::exists(m_compilerPath)) {
        QMessageBox::critical(this, "Script Compiler",
            QString("Compiler not found: %1").arg(m_compilerPath));
        return;
    }

    m_log->clear();
    m_compileBtn->setEnabled(false);

    m_process = new QProcess(this);
    m_process->setProgram(m_compilerPath);
    m_process->setArguments({scriptPath, outputPath});
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_process, &QProcess::readyReadStandardOutput, this, &ScriptCompilerWindow::onProcessOutput);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ScriptCompilerWindow::onProcessFinished);

    m_process->start();
}

void ScriptCompilerWindow::onProcessOutput()
{
    m_log->appendPlainText(QString::fromLocal8Bit(m_process->readAllStandardOutput()));
}

void ScriptCompilerWindow::onProcessFinished(int exitCode, int exitStatus)
{
    Q_UNUSED(exitStatus);
    m_compileBtn->setEnabled(true);
    m_log->appendPlainText(exitCode == 0 ? "\n-- Compile succeeded. --" : "\n-- Compile failed. --");
    m_process->deleteLater();
    m_process = nullptr;
}
