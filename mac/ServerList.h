#pragma once

#include <QList>
#include <QString>

struct ServerEntry
{
    QString name;
    QString host;
    QString port;
    bool discovered = false;  // true for a live LAN-discovered entry, not a saved favorite
};

// Reads servers.txt (name\thost\tport per line, '#' comments, blank lines
// ignored) from alongside the launcher binary. Missing file -> empty list.
QList<ServerEntry> LoadSavedServers();

// Appends one entry to servers.txt (creating it if needed).
void AppendSavedServer(const ServerEntry &entry);
