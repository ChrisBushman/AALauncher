#pragma once

#include <QDialog>
#include <QRadioButton>

// Fullscreen/Windowed, Color Depth, and Level of Detail controls, writing
// directly to resolution.ini (read by AA.exe's RESSCALE module, see
// AmuletsArmor's Source/Win32/RESSCALE.C) in the same directory as the AA
// binary this launcher starts. Same 3 settings and same resolution.ini
// format as the Win9x launcher (win9x/main.c) -- this is the equivalent
// for macOS and Linux, which share this Qt launcher (see the top-level
// CMakeLists.txt's add_subdirectory(mac)).
class DisplaySettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DisplaySettingsDialog(const QString &aaBinaryPath, QWidget *parent = nullptr);

private slots:
    void onOk();

private:
    void loadFromIni();
    void saveToIni();
    QString iniPath() const;

    QString m_aaDir;

    QRadioButton *m_windowedRadio = nullptr;
    QRadioButton *m_fullscreenRadio = nullptr;

    QRadioButton *m_bppAutoRadio = nullptr;
    QRadioButton *m_bpp8Radio = nullptr;
    QRadioButton *m_bpp16Radio = nullptr;
    QRadioButton *m_bpp24Radio = nullptr;
    QRadioButton *m_bpp32Radio = nullptr;

    QRadioButton *m_detailRadios[6] = { nullptr };
};
