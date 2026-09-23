#include "widgets/ImagePreviewWidget.h"

#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QLabel>
#include <QPixmap>
#include <QResizeEvent>
#include <QScrollArea>
#include <QVBoxLayout>

namespace vision3d {

ImagePreviewWidget::ImagePreviewWidget(QWidget* parent)
    : QWidget(parent)
    , m_scrollArea(new QScrollArea(this))
    , m_imageLabel(new QLabel(this))
{
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setMinimumSize(QSize(1, 1));
    m_imageLabel->setText(QStringLiteral("选择图像后显示预览。"));
    m_imageLabel->setWordWrap(true);

    m_scrollArea->setWidget(m_imageLabel);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setAlignment(Qt::AlignCenter);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_scrollArea);
}

void ImagePreviewWidget::clearPreview()
{
    m_sourceImage = {};
    m_currentAssetId.clear();
    showMessage(QStringLiteral("选择图像后显示预览。"));
}

void ImagePreviewWidget::showAsset(const AssetRecord& asset,
                                   const QString& projectDirectory)
{
    m_currentAssetId = asset.id;
    QString validationError;
    if (!asset.isValid(&validationError)) {
        m_sourceImage = {};
        showMessage(QStringLiteral("图像资产无效: %1").arg(validationError));
        return;
    }

    const QString imagePath = QDir(projectDirectory).filePath(
        QDir::fromNativeSeparators(QDir::cleanPath(asset.relativePath)));
    if (!QFileInfo(imagePath).isFile()) {
        m_sourceImage = {};
        showMessage(QStringLiteral("Missing\n%1").arg(asset.originalFileName));
        return;
    }

    QImageReader reader(imagePath);
    reader.setAutoTransform(true);
    m_sourceImage = reader.read();
    if (m_sourceImage.isNull()) {
        showMessage(QStringLiteral("无法读取图像预览: %1").arg(reader.errorString()));
        return;
    }
    updateDisplay();
}

QString ImagePreviewWidget::currentAssetId() const { return m_currentAssetId; }

bool ImagePreviewWidget::hasLoadedImage() const { return !m_sourceImage.isNull(); }

void ImagePreviewWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (!m_sourceImage.isNull()) {
        updateDisplay();
    }
}

void ImagePreviewWidget::updateDisplay()
{
    if (m_sourceImage.isNull()) {
        return;
    }

    const QSize available = m_scrollArea->viewport()->size();
    if (available.width() <= 1 || available.height() <= 1) {
        m_imageLabel->setPixmap(QPixmap::fromImage(m_sourceImage));
        return;
    }

    const QImage displayed = m_sourceImage.scaled(available,
                                                  Qt::KeepAspectRatio,
                                                  Qt::SmoothTransformation);
    m_imageLabel->setText(QString());
    m_imageLabel->setPixmap(QPixmap::fromImage(displayed));
    m_imageLabel->resize(displayed.size());
}

void ImagePreviewWidget::showMessage(const QString& message)
{
    m_imageLabel->setPixmap(QPixmap());
    m_imageLabel->setText(message);
    m_imageLabel->resize(m_scrollArea->viewport()->size());
}

} // namespace vision3d
