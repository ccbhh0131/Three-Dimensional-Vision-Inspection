#pragma once

#include <QPlainTextEdit>

namespace vision3d {

class LogPanel final : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit LogPanel(QWidget* parent = nullptr);

    void appendInfo(const QString& text);
    void appendBackend(const QString& text);
    void appendProcess(const QString& text);
    void appendError(const QString& text);

private:
    void appendMessage(const QString& source, const QString& text);
};

} // namespace vision3d
