#include "JobProcessor.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QLockFile>
#include <QTextStream>
#include <QTimer>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("Vision3DLocalAI");
    QCoreApplication::setApplicationVersion("0.1.0");
    QCommandLineParser parser;
    parser.setApplicationDescription("Serial file-copy worker; Qt Core only. No implicit runtime directories.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"inbox", "Existing client_to_ai directory (absolute path).", "directory"});
    parser.addOption({"outbox", "Separate ai_to_client directory (absolute path).", "directory"});
    parser.addOption({"once", "Scan once; process at most one ready task. Exit 2 if none is ready."});
    parser.addOption({"serve", "Poll every second until stopped."});
    // process()/showVersion()/showHelp() can open a Windows message box when
    // stdout is redirected. This background worker must always remain headless.
    if (!parser.parse(app.arguments())) {
        QTextStream(stderr) << "Invalid command line; use --help." << Qt::endl;
        return 1;
    }
    if (parser.isSet("help") || parser.isSet("help-all")) {
        QTextStream(stdout) << parser.helpText() << Qt::endl;
        return 0;
    }
    if (parser.isSet("version")) {
        QTextStream(stdout) << "Vision3DLocalAI " << app.applicationVersion() << Qt::endl;
        return 0;
    }

    auto error = [](const char *message) {
        QTextStream(stderr) << message << Qt::endl;
        return 1;
    };
    if (parser.isSet("once") == parser.isSet("serve")
        || !parser.isSet("inbox") || !parser.isSet("outbox"))
        return error("Specify --inbox, --outbox and exactly one of --once / --serve.");
    const QString inboxArg = QDir::fromNativeSeparators(parser.value("inbox"));
    const QString outboxArg = QDir::fromNativeSeparators(parser.value("outbox"));
    if (!QDir::isAbsolutePath(inboxArg) || !QDir::isAbsolutePath(outboxArg)
        || !QFileInfo(inboxArg).isDir())
        return error("Use absolute paths; inbox must already exist.");
    // Resolve existing ancestors before creating anything in the outbox.
    QString outbox = QDir::cleanPath(outboxArg);
    QString ancestor = outbox;
    QString suffix;
    while (!QFileInfo::exists(ancestor)) {
        suffix.prepend('/' + QFileInfo(ancestor).fileName());
        const QString parent = QFileInfo(ancestor).absolutePath();
        if (parent == ancestor)
            return error("Outbox has no existing parent directory.");
        ancestor = parent;
    }
    if (!QFileInfo(ancestor).isDir())
        return error("Outbox parent must be a directory.");
    outbox = QDir::cleanPath(QFileInfo(ancestor).canonicalFilePath() + suffix);
    const QString inbox = QFileInfo(inboxArg).canonicalFilePath();
    if (inbox.compare(outbox, Qt::CaseInsensitive) == 0
        || outbox.startsWith(inbox + '/', Qt::CaseInsensitive)
        || inbox.startsWith(outbox + '/', Qt::CaseInsensitive))
        return error("Inbox and outbox must be disjoint directory trees.");
    if (!QDir().mkpath(outbox))
        return error("Cannot create outbox.");
    QLockFile lock(QDir(outbox).filePath(".worker.lock"));
    lock.setStaleLockTime(0);
    if (!lock.tryLock())
        return error("Outbox is locked by another worker.");
    QTextStream(stdout) << QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)
                        << " startup mode=" << (parser.isSet("once") ? "once" : "serve") << Qt::endl;
    JobProcessor processor(inbox, outbox);
    if (parser.isSet("once")) {
        const auto outcome = processor.scanOnce();
        return outcome == JobProcessor::Outcome::Completed ? 0
            : outcome == JobProcessor::Outcome::Failed ? 1 : 2;
    }
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, &app, [&processor] { processor.scanOnce(); });
    timer.start(1000);
    QTimer::singleShot(0, &app, [&processor] { processor.scanOnce(); });
    return app.exec();
}
