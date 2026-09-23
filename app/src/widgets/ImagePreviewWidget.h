#pragma once

#include "core/assets/AssetRecord.h"

#include <QImage>
#include <QWidget>

class QLabel;
class QScrollArea;

namespace vision3d {

class ImagePreviewWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit ImagePreviewWidget(QWidget* parent = nullptr);

public slots:
    void clearPreview();
    void showAsset(const AssetRecord& asset, const QString& projectDirectory);

    QString currentAssetId() const;
    bool hasLoadedImage() const;

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void updateDisplay();
    void showMessage(const QString& message);

    QScrollArea* m_scrollArea;
    QLabel* m_imageLabel;
    QImage m_sourceImage;
    QString m_currentAssetId;
};

} // namespace vision3d
