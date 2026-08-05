#include "DisplaySettingsDialog.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QLabel>
#include <QTextStream>
#include <QVBoxLayout>

DisplaySettingsDialog::DisplaySettingsDialog(const QString &aaBinaryPath, QWidget *parent)
    : QDialog(parent)
    , m_aaDir(QFileInfo(aaBinaryPath).absolutePath())
{
    setWindowTitle("Display Settings");
    setModal(true);

    QVBoxLayout *vLayout = new QVBoxLayout(this);

    QGroupBox *displayGroup = new QGroupBox("Display", this);
    QVBoxLayout *displayLayout = new QVBoxLayout(displayGroup);
    m_windowedRadio = new QRadioButton("Windowed", displayGroup);
    m_fullscreenRadio = new QRadioButton("Fullscreen", displayGroup);
    displayLayout->addWidget(m_windowedRadio);
    displayLayout->addWidget(m_fullscreenRadio);
    QButtonGroup *displayButtons = new QButtonGroup(this);
    displayButtons->addButton(m_windowedRadio);
    displayButtons->addButton(m_fullscreenRadio);
    vLayout->addWidget(displayGroup);

    QGroupBox *bppGroup = new QGroupBox("Color Depth", this);
    QVBoxLayout *bppLayout = new QVBoxLayout(bppGroup);
    m_bppAutoRadio = new QRadioButton("Auto (recommended)", bppGroup);
    m_bpp8Radio = new QRadioButton("8-bit", bppGroup);
    m_bpp16Radio = new QRadioButton("16-bit", bppGroup);
    m_bpp24Radio = new QRadioButton("24-bit", bppGroup);
    m_bpp32Radio = new QRadioButton("32-bit", bppGroup);
    bppLayout->addWidget(m_bppAutoRadio);
    bppLayout->addWidget(m_bpp8Radio);
    bppLayout->addWidget(m_bpp16Radio);
    bppLayout->addWidget(m_bpp24Radio);
    bppLayout->addWidget(m_bpp32Radio);
    QButtonGroup *bppButtons = new QButtonGroup(this);
    bppButtons->addButton(m_bppAutoRadio);
    bppButtons->addButton(m_bpp8Radio);
    bppButtons->addButton(m_bpp16Radio);
    bppButtons->addButton(m_bpp24Radio);
    bppButtons->addButton(m_bpp32Radio);
    vLayout->addWidget(bppGroup);

    QGroupBox *detailGroup = new QGroupBox("Level of Detail (also sets window size)", this);
    QVBoxLayout *detailLayout = new QVBoxLayout(detailGroup);
    static const char *kDetailLabels[6] = {
        "1 (fastest, 640x480)",
        "2 (1024x768)",
        "3 (1280x960)",
        "4 (1280x960)",
        "5 (1280x960)",
        "6 (sharpest, 1280x960)",
    };
    QButtonGroup *detailButtons = new QButtonGroup(this);
    for (int i = 0; i < 6; i++) {
        m_detailRadios[i] = new QRadioButton(kDetailLabels[i], detailGroup);
        detailLayout->addWidget(m_detailRadios[i]);
        detailButtons->addButton(m_detailRadios[i]);
    }
    vLayout->addWidget(detailGroup);

    m_vsyncCheck = new QCheckBox(
        "Vertical sync (reduces tearing; caps FPS to the refresh rate)", this);
    vLayout->addWidget(m_vsyncCheck);

    QLabel *note = new QLabel(
        "Applied the next time you start a single-player or network game.",
        this);
    note->setWordWrap(true);
    vLayout->addWidget(note);

    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &DisplaySettingsDialog::onOk);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    vLayout->addWidget(buttons);

    loadFromIni();
}

QString DisplaySettingsDialog::iniPath() const
{
    return m_aaDir + QDir::separator() + "resolution.ini";
}

void DisplaySettingsDialog::loadFromIni()
{
    // Defaults match RESSCALE.C's own compiled-in defaults (fullscreen=1,
    // bpp=0/auto, detail=2) so a launcher run before AA.exe has ever
    // created resolution.ini still shows the same choice AA.exe itself
    // would default to.
    int fullscreen = 1;
    int bpp = 0;
    int detail = 2;
    int vsync = 0;      // matches RESSCALE.C's default (off)

    QFile f(iniPath());
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&f);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            int eq = line.indexOf('=');
            if (eq <= 0)
                continue;
            QString key = line.left(eq);
            QString value = line.mid(eq + 1);
            bool ok = false;
            int intValue = value.toInt(&ok);
            if (!ok)
                continue;
            if (key == "fullscreen")
                fullscreen = intValue;
            else if (key == "bpp")
                bpp = intValue;
            else if (key == "detail")
                detail = intValue;
            else if (key == "vsync")
                vsync = intValue;
        }
    }

    (fullscreen ? m_fullscreenRadio : m_windowedRadio)->setChecked(true);

    QRadioButton *bppRadio = m_bppAutoRadio;
    switch (bpp) {
        case 8:  bppRadio = m_bpp8Radio;  break;
        case 16: bppRadio = m_bpp16Radio; break;
        case 24: bppRadio = m_bpp24Radio; break;
        case 32: bppRadio = m_bpp32Radio; break;
        default: bppRadio = m_bppAutoRadio; break;
    }
    bppRadio->setChecked(true);

    if (detail < 1) detail = 1;
    if (detail > 6) detail = 6;
    m_detailRadios[detail - 1]->setChecked(true);

    m_vsyncCheck->setChecked(vsync != 0);
}

void DisplaySettingsDialog::onOk()
{
    saveToIni();
    accept();
}

void DisplaySettingsDialog::saveToIni()
{
    // Rewrites resolution.ini with the current selections, preserving
    // every other line (comments, scale=/fit=/aspect=/hotkeys=) exactly
    // as found -- same merge-preserve approach as the Win9x launcher's
    // WriteResolutionIni (win9x/main.c), for the same reason: an
    // explicit width=/height= otherwise pins the window regardless of
    // detail level (RESSCALE.C's IComputeWindowSize gives explicit
    // width=/height= priority over scale=), so window size needs to be
    // rewritten alongside detail level, not left stale.
    int fullscreen = m_fullscreenRadio->isChecked() ? 1 : 0;

    int bpp = 0;
    if (m_bpp8Radio->isChecked()) bpp = 8;
    else if (m_bpp16Radio->isChecked()) bpp = 16;
    else if (m_bpp24Radio->isChecked()) bpp = 24;
    else if (m_bpp32Radio->isChecked()) bpp = 32;

    int detail = 2;
    for (int i = 0; i < 6; i++) {
        if (m_detailRadios[i]->isChecked()) {
            detail = i + 1;
            break;
        }
    }

    int vsync = m_vsyncCheck->isChecked() ? 1 : 0;

    // Window size tier per detail level -- capped at the level-3 size for
    // levels 4-6 rather than continuing to grow the window: beyond that
    // point higher detail is about render sharpness, not needing an even
    // bigger window. All 4:3, matching resolution.ini's own default
    // aspect.
    static const int kWidths[6]  = { 640, 1024, 1280, 1280, 1280, 1280 };
    static const int kHeights[6] = { 480,  768,  960,  960,  960,  960 };
    int width = kWidths[detail - 1];
    int height = kHeights[detail - 1];

    QStringList keptLines;
    QFile inFile(iniPath());
    if (inFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&inFile);
        while (!in.atEnd()) {
            QString line = in.readLine();
            QString trimmed = line.trimmed();
            bool isManaged =
                trimmed.startsWith("fullscreen=") ||
                trimmed.startsWith("bpp=") ||
                trimmed.startsWith("detail=") ||
                trimmed.startsWith("width=") ||
                trimmed.startsWith("height=") ||
                trimmed.startsWith("vsync=");
            if (!isManaged)
                keptLines << line;
        }
    }

    QFile outFile(iniPath());
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return;
    QTextStream out(&outFile);
    for (const QString &line : keptLines)
        out << line << "\n";
    out << "fullscreen=" << fullscreen << "\n";
    out << "bpp=" << bpp << "\n";
    out << "detail=" << detail << "\n";
    out << "width=" << width << "\n";
    out << "height=" << height << "\n";
    out << "vsync=" << vsync << "\n";
}
