#include "ServerList.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>

static QString ServersFilePath()
{
    return QCoreApplication::applicationDirPath() + QDir::separator() + "servers.txt";
}

QList<ServerEntry> LoadSavedServers()
{
    QList<ServerEntry> result;
    QFile file(ServersFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return result;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;
        QStringList parts = line.split('\t');
        if (parts.size() < 3)
            continue;
        ServerEntry entry;
        entry.name = parts[0].trimmed();
        entry.host = parts[1].trimmed();
        entry.port = parts[2].trimmed();
        entry.discovered = false;
        result.append(entry);
    }
    return result;
}

void AppendSavedServer(const ServerEntry &entry)
{
    QFile file(ServersFilePath());
    if (!file.open(QIODevice::Append | QIODevice::Text))
        return;

    QTextStream out(&file);
    out << entry.name << '\t' << entry.host << '\t' << entry.port << '\n';
}
