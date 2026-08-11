#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;
class QFile;

// Result of a "check for updates" query against the AmuletsArmor-Bundle
// GitHub releases feed.
struct UpdateInfo {
    bool    valid          = false;  // a well-formed release was parsed
    bool    updateAvailable = false; // latestVersion is newer than installed
    QString currentVersion;          // from this .app's Info.plist (e.g. "1.2.3")
    QString latestVersion;           // release tag minus leading 'v' (e.g. "1.2.4")
    QString latestTag;               // raw tag (e.g. "v1.2.4")
    QString assetName;               // AmuletsArmor-Bundle-macOS.dmg
    QString assetUrl;                // browser_download_url for the macOS dmg
    qint64  assetSize      = 0;      // bytes, for the progress bar / sanity check
    QString releaseUrl;              // html_url of the release (for "release notes")
};

// Drives the macOS self-update flow for the launcher's own bundle:
//   check -> (offer) -> backup user data -> download dmg -> stage new app ->
//   restore user data -> hand off the in-place swap to a detached helper.
//
// The launcher runs from inside the very "Amulets & Armor.app" it must
// replace, so the final swap can't happen in-process -- performUpdate()
// stages everything, then emits readyToRelaunch() so the caller can quit;
// a detached /bin/sh helper waits for us to exit, swaps the bundle, and
// relaunches it.
class Updater : public QObject
{
    Q_OBJECT

public:
    explicit Updater(QObject *parent = nullptr);
    ~Updater() override;

    // True when running from inside a .app bundle (i.e. a real install, not
    // the flat dev binary). Update actions are only meaningful when true.
    static bool isInstalledBundle();

    // Absolute path to the enclosing "*.app" (empty if not in a bundle).
    static QString appBundlePath();

    // The Contents/MacOS dir holding AA + the writable user data.
    static QString macosDir();

    // CFBundleShortVersionString from the enclosing .app's Info.plist
    // (empty if unavailable). This is the "installed version".
    static QString installedVersion();

    // Returns <0, 0, >0 comparing dotted numeric versions ("1.2.10" > "1.2.9").
    static int compareVersions(const QString &a, const QString &b);

    // The user-data entries under macosDir() that must survive an update:
    // config.ini, resolution.ini, PLAYER.CFG, and every S<digits> save dir.
    static QStringList userDataEntries();

    void checkForUpdates();               // async; emits checkFinished/checkFailed
    void performUpdate(const UpdateInfo &info); // async; drives the full flow

public slots:
    void cancel();                        // abort an in-flight download

signals:
    void checkFinished(const UpdateInfo &info);
    void checkFailed(const QString &error);

    void statusChanged(const QString &message);        // human-readable step
    void downloadProgress(qint64 received, qint64 total);
    void updateFailed(const QString &error);
    void readyToRelaunch();               // staging done; caller should quit

private slots:
    void onCheckReply();
    void onDownloadReadyRead();
    void onDownloadFinished();

private:
    bool backupUserData(const QString &backupDir, QString *error);
    bool restoreUserDataInto(const QString &backupDir, const QString &destMacosDir,
                             QString *error);
    void installFromDmg();                // post-download: mount/stage/swap
    QString mountDmg(const QString &dmgPath, QString *error);
    static bool runProc(const QString &program, const QStringList &args,
                        QString *error, QString *stdOut = nullptr);
    void fail(const QString &error);
    void cleanupPartialDownload();

    QNetworkAccessManager *m_net = nullptr;
    QNetworkReply         *m_reply = nullptr;
    QFile                 *m_downloadFile = nullptr;
    QString                m_downloadPath;
    UpdateInfo             m_info;
    bool                   m_canceled = false;
    QString                m_backupDir;      // where user data was snapshotted
};
