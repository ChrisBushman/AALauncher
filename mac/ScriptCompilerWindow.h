#pragma once

#include <QWidget>

class QLineEdit;
class QPlainTextEdit;
class QProcess;
class QPushButton;

// Standalone (non-modal) window wrapping AAScriptCompiler's "SC <script>
// <output>" CLI -- lets a content author compile a .SRC/.SRP without
// leaving the launcher. Opened as a second top-level window from
// MainWindow, not a dialog, so the main launcher window stays usable
// alongside it.
class ScriptCompilerWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ScriptCompilerWindow(const QString &compilerPath, QWidget *parent = nullptr);

private slots:
    void onBrowseScript();
    void onBrowseOutput();
    void onCompile();
    void onProcessOutput();
    void onProcessFinished(int exitCode, int exitStatus);

private:
    QString m_compilerPath;
    QLineEdit *m_scriptEdit = nullptr;
    QLineEdit *m_outputEdit = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QPushButton *m_compileBtn = nullptr;
    QProcess *m_process = nullptr;
};
