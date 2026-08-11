#include "Updater.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

namespace {

// The combined-bundle release feed the launcher updates itself from, and the
// per-platform asset the macOS launcher pulls. (See AmuletsArmor-Bundle repo.)
const char *kReleasesApi =
    "https://api.github.com/repos/ChrisBushman/AmuletsArmor-Bundle/releases/latest";
const char *kMacAsset = "AmuletsArmor-Bundle-macOS.dmg";

// A user-created single-player/network save "server" directory: literally 'S'
// followed by all digits (e.g. S0000000). Deliberately excludes the many
// game-data files that also start with 'S' (S41.SRP, SETUP.INI, SOUNDS.RES),
// which must NOT be treated as user data -- restoring those over a new release
// would clobber the update.
const QRegularExpression &saveDirPattern()
{
    static const QRegularExpression re("^S[0-9]+$");
    return re;
}

// Single writable config files that live loose in Contents/MacOS. Matched
// case-insensitively (HFS+ is case-insensitive; the game ships PLAYER.CFG
// uppercase, config.ini/resolution.ini lowercase).
bool isUserDataFile(const QString &name)
{
    static const QStringList files = {"config.ini", "resolution.ini", "PLAYER.CFG"};
    for (const QString &f : files)
        if (name.compare(f, Qt::CaseInsensitive) == 0)
            return true;
    return false;
}

} // namespace

Updater::Updater(QObject *parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
}

Updater::~Updater() = default;

// --- static install-layout helpers ---------------------------------------

QString Updater::appBundlePath()
{
    // applicationDirPath() is ".../Amulets & Armor.app/Contents/MacOS".
    // The .app is two directories up. Only report it when the path really
    // has that shape (flat dev binary -> not in a bundle -> empty).
    QDir macos(QCoreApplication::applicationDirPath());
    if (macos.dirName() != "MacOS")
        return {};
    QDir contents = macos;
    if (!contents.cdUp() || contents.dirName() != "Contents")
        return {};
    QDir app = contents;
    if (!app.cdUp() || !app.dirName().endsWith(".app"))
        return {};
    return app.absolutePath();
}

bool Updater::isInstalledBundle()
{
    return !appBundlePath().isEmpty();
}

QString Updater::macosDir()
{
    return QCoreApplication::applicationDirPath();
}

QString Updater::installedVersion()
{
    const QString app = appBundlePath();
    if (app.isEmpty())
        return {};
    const QString plist = app + "/Contents/Info.plist";

    // Preferred: let Qt read the plist natively (handles XML or binary).
    QSettings s(plist, QSettings::NativeFormat);
    QString v = s.value("CFBundleShortVersionString").toString();
    if (!v.isEmpty())
        return v;

    // Fallback: our pipeline always emits XML plists, so a direct scan works
    // even if QSettings' native reader declines this particular file.
    QFile f(plist);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString text = QString::fromUtf8(f.readAll());
        static const QRegularExpression re(
            "<key>CFBundleShortVersionString</key>\\s*<string>([^<]*)</string>");
        auto m = re.match(text);
        if (m.hasMatch())
            return m.captured(1).trimmed();
    }
    return {};
}

int Updater::compareVersions(const QString &a, const QString &b)
{
    const QStringList pa = a.split('.');
    const QStringList pb = b.split('.');
    const int n = qMax(pa.size(), pb.size());
    for (int i = 0; i < n; ++i) {
        const int va = i < pa.size() ? pa[i].toInt() : 0;
        const int vb = i < pb.size() ? pb[i].toInt() : 0;
        if (va != vb)
            return va < vb ? -1 : 1;
    }
    return 0;
}

QStringList Updater::userDataEntries()
{
    QStringList out;
    QDir dir(macosDir());
    const auto entries = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::Files |
                                           QDir::Dirs | QDir::Hidden);
    for (const QFileInfo &fi : entries) {
        if (fi.isDir() && saveDirPattern().match(fi.fileName()).hasMatch())
            out << fi.fileName();
        else if (fi.isFile() && isUserDataFile(fi.fileName()))
            out << fi.fileName();
    }
    return out;
}

// --- check for updates ----------------------------------------------------

void Updater::checkForUpdates()
{
    QNetworkRequest req{QUrl(QString::fromLatin1(kReleasesApi))};
    // GitHub's API rejects requests without a User-Agent, and wants this
    // Accept header for the stable v3 JSON.
    req.setHeader(QNetworkRequest::UserAgentHeader, "AALauncher-macOS");
    req.setRawHeader("Accept", "application/vnd.github+json");
    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, &Updater::onCheckReply);
}

void Updater::onCheckReply()
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit checkFailed(reply->errorString());
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isObject()) {
        emit checkFailed("Unexpected response from the update server.");
        return;
    }
    const QJsonObject root = doc.object();

    UpdateInfo info;
    info.latestTag = root.value("tag_name").toString();
    info.latestVersion = info.latestTag;
    if (info.latestVersion.startsWith('v') || info.latestVersion.startsWith('V'))
        info.latestVersion = info.latestVersion.mid(1);
    info.releaseUrl = root.value("html_url").toString();

    for (const QJsonValue &v : root.value("assets").toArray()) {
        const QJsonObject a = v.toObject();
        if (a.value("name").toString() == QString::fromLatin1(kMacAsset)) {
            info.assetName = a.value("name").toString();
            info.assetUrl = a.value("browser_download_url").toString();
            info.assetSize = a.value("size").toVariant().toLongLong();
            break;
        }
    }

    if (info.latestVersion.isEmpty() || info.assetUrl.isEmpty()) {
        emit checkFailed("The latest release has no macOS bundle to download.");
        return;
    }

    info.currentVersion = installedVersion();
    info.valid = true;
    // Unknown current version (e.g. an older bundle whose Info.plist predates
    // the version-stamp fix) -> offer the update rather than hide it; the flow
    // always backs up user data first, so re-installing is harmless.
    info.updateAvailable = info.currentVersion.isEmpty() ||
        compareVersions(info.currentVersion, info.latestVersion) < 0;

    emit checkFinished(info);
}

// --- perform the update ---------------------------------------------------

void Updater::fail(const QString &error)
{
    cleanupPartialDownload();
    emit updateFailed(error);
}

void Updater::cleanupPartialDownload()
{
    if (m_reply) {
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if (m_downloadFile) {
        m_downloadFile->close();
        m_downloadFile->remove();
        delete m_downloadFile;
        m_downloadFile = nullptr;
    }
}

void Updater::performUpdate(const UpdateInfo &info)
{
    m_info = info;
    m_canceled = false;

    if (!isInstalledBundle()) {
        fail("The launcher isn't running from an installed app bundle, so it "
             "can't update itself in place.");
        return;
    }

    const QString appPath = appBundlePath();
    const QFileInfo appInfo(appPath);
    const QString parentDir = appInfo.absolutePath();
    QFileInfo parentInfo(parentDir);
    if (!parentInfo.isWritable()) {
        fail(QString("Can't write to \"%1\".\n\nMove Amulets & Armor to a "
                     "location you own (for example your Applications or "
                     "Downloads folder) and try again.").arg(parentDir));
        return;
    }

    // Snapshot user data next to the app, tagged with the outgoing version.
    const QString ver = m_info.currentVersion.isEmpty() ? "unknown" : m_info.currentVersion;
    m_backupDir = parentDir + "/AA-backup-" + ver + "-" +
                  QString::number(QDateTime::currentSecsSinceEpoch());
    emit statusChanged("Backing up your characters and settings...");
    QString err;
    if (!backupUserData(m_backupDir, &err)) {
        fail("Backup failed: " + err);
        return;
    }

    // Download the new bundle to a temp file we can stream to disk.
    const QString tmp = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    m_downloadPath = tmp + "/" + kMacAsset;
    m_downloadFile = new QFile(m_downloadPath);
    if (!m_downloadFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        delete m_downloadFile;
        m_downloadFile = nullptr;
        fail("Couldn't create a temporary download file.");
        return;
    }

    emit statusChanged("Downloading " + m_info.latestTag + "...");
    QNetworkRequest req{QUrl(m_info.assetUrl)};
    req.setHeader(QNetworkRequest::UserAgentHeader, "AALauncher-macOS");
    // browser_download_url redirects to the release CDN; follow it.
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_net->get(req);
    connect(m_reply, &QNetworkReply::readyRead, this, &Updater::onDownloadReadyRead);
    connect(m_reply, &QNetworkReply::downloadProgress, this, &Updater::downloadProgress);
    connect(m_reply, &QNetworkReply::finished, this, &Updater::onDownloadFinished);
}

void Updater::cancel()
{
    m_canceled = true;
    if (m_reply)
        m_reply->abort();
}

void Updater::onDownloadReadyRead()
{
    if (m_reply && m_downloadFile)
        m_downloadFile->write(m_reply->readAll());
}

void Updater::onDownloadFinished()
{
    if (!m_reply)
        return;
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;

    const bool aborted = m_canceled;
    const QNetworkReply::NetworkError netErr = reply->error();
    const QString netErrStr = reply->errorString();
    if (m_downloadFile)
        m_downloadFile->write(reply->readAll());
    reply->deleteLater();

    if (aborted) {
        cleanupPartialDownload();
        emit updateFailed(QString());   // empty == user-canceled, caller stays quiet
        return;
    }
    if (netErr != QNetworkReply::NoError) {
        fail("Download failed: " + netErrStr);
        return;
    }

    if (m_downloadFile) {
        m_downloadFile->close();
        const qint64 got = m_downloadFile->size();
        delete m_downloadFile;
        m_downloadFile = nullptr;
        if (m_info.assetSize > 0 && got != m_info.assetSize) {
            fail(QString("Downloaded file is incomplete (%1 of %2 bytes).")
                     .arg(got).arg(m_info.assetSize));
            return;
        }
    }

    installFromDmg();
}

// --- backup / restore -----------------------------------------------------

bool Updater::backupUserData(const QString &backupDir, QString *error)
{
    if (!QDir().mkpath(backupDir)) {
        *error = "couldn't create backup folder";
        return false;
    }
    const QString src = macosDir();
    const QStringList entries = userDataEntries();
    for (const QString &name : entries) {
        // ditto copies files and directory trees alike, preserving contents.
        if (!runProc("ditto", {src + "/" + name, backupDir + "/" + name}, error))
            return false;
    }
    return true;
}

bool Updater::restoreUserDataInto(const QString &backupDir, const QString &destMacosDir,
                                  QString *error)
{
    QDir bdir(backupDir);
    const auto entries = bdir.entryInfoList(QDir::NoDotAndDotDot | QDir::Files |
                                            QDir::Dirs | QDir::Hidden);
    for (const QFileInfo &fi : entries) {
        // Overwrite the fresh release's defaults with the user's own copy.
        const QString dst = destMacosDir + "/" + fi.fileName();
        if (fi.isDir())
            QDir(dst).removeRecursively();
        else
            QFile::remove(dst);
        if (!runProc("ditto", {fi.absoluteFilePath(), dst}, error))
            return false;
    }
    return true;
}

// --- dmg mount / stage / swap handoff ------------------------------------

QString Updater::mountDmg(const QString &dmgPath, QString *error)
{
    QString out;
    if (!runProc("hdiutil",
                 {"attach", "-nobrowse", "-readonly", "-noverify", dmgPath},
                 error, &out)) {
        return {};
    }
    // A mount line looks like "/dev/disk4s1 \t Apple_HFS \t /Volumes/Name".
    // Volume names contain spaces, so take everything from "/Volumes/" to the
    // end of the last such line rather than splitting on whitespace.
    QString mountPoint;
    for (const QString &line : out.split('\n')) {
        const int idx = line.indexOf("/Volumes/");
        if (idx >= 0)
            mountPoint = line.mid(idx).trimmed();
    }
    if (mountPoint.isEmpty())
        *error = "couldn't determine where the disk image mounted";
    return mountPoint;
}

void Updater::installFromDmg()
{
    emit statusChanged("Preparing the new version...");
    QString err;

    const QString mount = mountDmg(m_downloadPath, &err);
    if (mount.isEmpty()) {
        fail("Couldn't open the downloaded disk image: " + err);
        return;
    }

    // Locate the "*.app" inside the mounted image.
    QString srcApp;
    for (const QFileInfo &fi : QDir(mount).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (fi.fileName().endsWith(".app")) {
            srcApp = fi.absoluteFilePath();
            break;
        }
    }
    if (srcApp.isEmpty()) {
        runProc("hdiutil", {"detach", mount, "-quiet"}, &err);
        fail("The downloaded disk image doesn't contain an app to install.");
        return;
    }

    const QString appPath = appBundlePath();
    const QString parentDir = QFileInfo(appPath).absolutePath();
    // Stage on the same volume as the destination so the final move is a fast,
    // atomic rename rather than a cross-device copy.
    const QString stagingDir = parentDir + "/.AA-update-" + m_info.latestVersion;
    QDir(stagingDir).removeRecursively();
    if (!QDir().mkpath(stagingDir)) {
        runProc("hdiutil", {"detach", mount, "-quiet"}, &err);
        fail("Couldn't create a staging folder for the update.");
        return;
    }
    const QString stagedApp = stagingDir + "/" + QFileInfo(appPath).fileName();

    // Copy the new app out of the (read-only) image into staging.
    if (!runProc("ditto", {srcApp, stagedApp}, &err)) {
        runProc("hdiutil", {"detach", mount, "-quiet"}, nullptr);
        QDir(stagingDir).removeRecursively();
        fail("Couldn't copy the new version: " + err);
        return;
    }

    // Done with the image; the swap works entirely from the staged copy.
    runProc("hdiutil", {"detach", mount, "-quiet"}, nullptr);
    QFile::remove(m_downloadPath);

    // Restore the user's characters/settings on top of the fresh install.
    emit statusChanged("Restoring your characters and settings...");
    const QString stagedMacos = stagedApp + "/Contents/MacOS";
    if (!restoreUserDataInto(m_backupDir, stagedMacos, &err)) {
        QDir(stagingDir).removeRecursively();
        fail("Couldn't restore your saved data: " + err +
             "\n\nYour backup is safe at:\n" + m_backupDir);
        return;
    }

    // The launcher lives inside the app being replaced, so a detached helper
    // does the swap after we exit. It waits for our PID, moves the old app
    // aside, moves the staged app into place (rolling back on failure), clears
    // quarantine, cleans up, and relaunches.
    const QString helperPath = stagingDir + "/swap.sh";
    QFile helper(helperPath);
    if (!helper.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QDir(stagingDir).removeRecursively();
        fail("Couldn't write the update helper script.");
        return;
    }
    const QString script = QStringLiteral(
        "#!/bin/sh\n"
        "LAUNCHER_PID=\"$1\"\n"
        "OLD_APP=\"$2\"\n"
        "STAGED_APP=\"$3\"\n"
        "STAGING=\"$4\"\n"
        "i=0\n"
        "while kill -0 \"$LAUNCHER_PID\" 2>/dev/null; do\n"
        "  sleep 0.2; i=$((i+1)); [ $i -gt 300 ] && break\n"
        "done\n"
        "OLD_MOVED=\"$OLD_APP.updating-old\"\n"
        "rm -rf \"$OLD_MOVED\"\n"
        "if [ -d \"$OLD_APP\" ]; then mv \"$OLD_APP\" \"$OLD_MOVED\" || exit 1; fi\n"
        "if mv \"$STAGED_APP\" \"$OLD_APP\"; then\n"
        "  rm -rf \"$OLD_MOVED\"\n"
        "else\n"
        "  rm -rf \"$OLD_APP\"; [ -d \"$OLD_MOVED\" ] && mv \"$OLD_MOVED\" \"$OLD_APP\"\n"
        "fi\n"
        "xattr -dr com.apple.quarantine \"$OLD_APP\" 2>/dev/null\n"
        "rm -rf \"$STAGING\"\n"
        "open \"$OLD_APP\"\n");
    helper.write(script.toUtf8());
    helper.close();

    const qint64 pid = QCoreApplication::applicationPid();
    const QStringList args = {helperPath, QString::number(pid), appPath, stagedApp, stagingDir};
    if (!QProcess::startDetached("/bin/sh", args)) {
        QDir(stagingDir).removeRecursively();
        fail("Couldn't launch the update helper.");
        return;
    }

    emit statusChanged("Finishing up. Amulets & Armor will relaunch...");
    emit readyToRelaunch();
}

// --- process helper -------------------------------------------------------

bool Updater::runProc(const QString &program, const QStringList &args,
                      QString *error, QString *stdOut)
{
    QProcess p;
    p.start(program, args);
    if (!p.waitForStarted(10000)) {
        if (error)
            *error = program + " could not be started";
        return false;
    }
    p.waitForFinished(-1);
    if (stdOut)
        *stdOut = QString::fromUtf8(p.readAllStandardOutput());
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        if (error) {
            const QString errOut = QString::fromUtf8(p.readAllStandardError()).trimmed();
            *error = errOut.isEmpty()
                ? QString("%1 failed (exit %2)").arg(program).arg(p.exitCode())
                : errOut;
        }
        return false;
    }
    return true;
}
