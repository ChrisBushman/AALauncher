#include <QApplication>
#include <QCommandLineParser>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("AALauncher");
    app.setApplicationVersion("1.00");

    QCommandLineParser parser;
    parser.setApplicationDescription("Amulets & Armor macOS Launcher");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption aaPathOpt("aa-path",
        "Full path to the AA game binary (default: AA alongside launcher).",
        "path");
    QCommandLineOption serverPathOpt("server-path",
        "Full path to the AAServer binary (default: AAServer alongside launcher).",
        "path");
    QCommandLineOption portOpt("port",
        "IPX UDP port for server and client (default: 213).",
        "port");

    parser.addOption(aaPathOpt);
    parser.addOption(serverPathOpt);
    parser.addOption(portOpt);
    parser.process(app);

    MainWindow w(parser.value(aaPathOpt), parser.value(serverPathOpt), parser.value(portOpt));
    w.show();

    return app.exec();
}
