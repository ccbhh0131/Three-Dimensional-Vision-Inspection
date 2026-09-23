#include "widgets/LogPanel.h"

#include <QDateTime>

namespace vision3d {

LogPanel::LogPanel(QWidget* parent)
    : QPlainTextEdit(parent)
{
    setReadOnly(true);
    setMaximumBlockCount(5000);
    setPlaceholderText(QStringLiteral("运行日志将在这里显示。"));
}

void LogPanel::appendInfo(const QString& text) { appendMessage(QStringLiteral("INFO"), text); }

void LogPanel::appendBackend(const QString& text)
{
    appendMessage(QStringLiteral("重建引擎"), text);
}

void LogPanel::appendProcess(const QString& text) { appendMessage(QStringLiteral("PROCESS"), text); }

void LogPanel::appendError(const QString& text) { appendMessage(QStringLiteral("ERROR"), text); }

void LogPanel::appendMessage(const QString& source, const QString& text)
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        if (line.isEmpty() && lines.size() > 1) {
            continue;
        }
        appendPlainText(QStringLiteral("[%1] [%2] %3").arg(timestamp, source, line));
    }
}

} // namespace vision3d
