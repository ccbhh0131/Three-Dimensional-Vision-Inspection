#include "widgets/VisualGaugeReadingDialog.h"
#include "widgets/ProductDialogStyle.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QImageReader>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>

namespace vision3d {
namespace {

class GaugeImageCanvas final : public QWidget
{
public:
    explicit GaugeImageCanvas(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("visualImageCanvas"));
        setMinimumSize(520, 360);
        setMouseTracking(true);
        setAutoFillBackground(true);
    }

    void setImage(const QImage& image)
    {
        m_image = image;
        m_overlay = {};
        m_roi = {};
        update();
    }

    void setOverlay(const QImage& overlay)
    {
        m_overlay = overlay;
        update();
    }

    void clearOverlay()
    {
        m_overlay = {};
        update();
    }

    void setWholeImage(bool wholeImage)
    {
        m_wholeImage = wholeImage;
        if (m_wholeImage && !m_image.isNull()) {
            m_roi = QRect(QPoint(0, 0), m_image.size());
        }
        if (!m_wholeImage) {
            m_roi = {};
        }
        update();
    }

    QRect roi() const
    {
        return m_roi.normalized().intersected(QRect(QPoint(0, 0), m_image.size()));
    }

    void resetRoi()
    {
        m_wholeImage = false;
        m_roi = {};
        clearOverlay();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(32, 32, 32));
        if (m_image.isNull()) {
            painter.setPen(Qt::white);
            painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("请选择仪表图片"));
            return;
        }

        const QRect target = fittedImageRect();
        painter.drawImage(target, m_overlay.isNull() ? m_image : m_overlay);
        painter.setPen(QPen(QColor(255, 200, 0), 2));
        const QRect displayRoi = imageToWidget(m_roi.normalized());
        if (displayRoi.isValid() && displayRoi.width() > 0 && displayRoi.height() > 0) {
            painter.drawRect(displayRoi);
        }
        if (m_dragging) {
            painter.setPen(QPen(QColor(0, 220, 255), 2, Qt::DashLine));
            painter.drawRect(imageToWidget(QRect(m_dragStart, m_dragCurrent).normalized()));
        }
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() != Qt::LeftButton || m_image.isNull() || m_wholeImage) {
            return;
        }
        const QPoint imagePoint = widgetToImage(event->position().toPoint());
        if (!imagePoint.isNull()) {
            m_dragging = true;
            m_dragStart = imagePoint;
            m_dragCurrent = imagePoint;
            m_roi = QRect(m_dragStart, m_dragCurrent);
            update();
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (!m_dragging) {
            return;
        }
        m_dragCurrent = widgetToImage(event->position().toPoint());
        m_roi = QRect(m_dragStart, m_dragCurrent).normalized();
        update();
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() != Qt::LeftButton || !m_dragging) {
            return;
        }
        m_dragging = false;
        m_dragCurrent = widgetToImage(event->position().toPoint());
        m_roi = QRect(m_dragStart, m_dragCurrent).normalized();
        update();
    }

private:
    QRect fittedImageRect() const
    {
        if (m_image.isNull()) {
            return {};
        }
        const QSize fitted = m_image.size().scaled(size(), Qt::KeepAspectRatio);
        return QRect((width() - fitted.width()) / 2,
                     (height() - fitted.height()) / 2,
                     fitted.width(),
                     fitted.height());
    }

    QPoint widgetToImage(const QPoint& point) const
    {
        const QRect target = fittedImageRect();
        if (!target.contains(point) || target.width() <= 0 || target.height() <= 0) {
            return {};
        }
        const int x = std::clamp(
            static_cast<int>((point.x() - target.x()) * m_image.width() / target.width()),
            0,
            m_image.width() - 1);
        const int y = std::clamp(
            static_cast<int>((point.y() - target.y()) * m_image.height() / target.height()),
            0,
            m_image.height() - 1);
        return QPoint(x, y);
    }

    QRect imageToWidget(const QRect& imageRect) const
    {
        const QRect target = fittedImageRect();
        if (imageRect.isNull() || target.isNull() || m_image.width() <= 0
            || m_image.height() <= 0) {
            return {};
        }
        return QRect(target.x() + imageRect.x() * target.width() / m_image.width(),
                     target.y() + imageRect.y() * target.height() / m_image.height(),
                     std::max(1, imageRect.width() * target.width() / m_image.width()),
                     std::max(1, imageRect.height() * target.height() / m_image.height()));
    }

    QImage m_image;
    QImage m_overlay;
    QRect m_roi;
    QPoint m_dragStart;
    QPoint m_dragCurrent;
    bool m_dragging = false;
    bool m_wholeImage = false;
};

QString imageFilter()
{
    QStringList suffixes;
    for (const QByteArray& format : QImageReader::supportedImageFormats()) {
        suffixes.append(QStringLiteral("*.") + QString::fromLatin1(format).toLower());
    }
    suffixes.removeDuplicates();
    return suffixes.isEmpty()
        ? QStringLiteral("图像文件")
        : QStringLiteral("图像文件 (%1)").arg(suffixes.join(QLatin1Char(' ')));
}

} // namespace

VisualGaugeReadingDialog::VisualGaugeReadingDialog(
    const GaugeProfile& profile,
    const QList<VisualGaugeImageChoice>& projectImages,
    QWidget* parent)
    : QDialog(parent)
    , m_profile(profile)
    , m_projectImages(projectImages)
    , m_projectImageCombo(new QComboBox(this))
    , m_externalButton(new QPushButton(QStringLiteral("选择外部图片"), this))
    , m_wholeImageCheckBox(new QCheckBox(QStringLiteral("使用整图作为 ROI"), this))
    , m_canvas(new GaugeImageCanvas(this))
    , m_imagePathLabel(new QLabel(QStringLiteral("未选择图片"), this))
    , m_resultLabel(new QLabel(QStringLiteral("尚未分析。请先选择图片并框选 ROI。"), this))
    , m_resetRoiButton(new QPushButton(QStringLiteral("重新选择 ROI"), this))
    , m_analyzeButton(new QPushButton(QStringLiteral("分析"), this))
    , m_confirmButton(new QPushButton(QStringLiteral("确认读数"), this))
    , m_cancelButton(new QPushButton(QStringLiteral("取消"), this))
{
    setWindowTitle(QStringLiteral("视觉读数 — %1").arg(profile.name));
    resize(900, 720);
    setModal(true);

    m_projectImageCombo->setObjectName(QStringLiteral("visualProjectImageCombo"));
    m_externalButton->setObjectName(QStringLiteral("visualChooseExternalButton"));
    m_wholeImageCheckBox->setObjectName(QStringLiteral("visualWholeImageCheckBox"));
    m_imagePathLabel->setObjectName(QStringLiteral("visualImagePathLabel"));
    m_resultLabel->setObjectName(QStringLiteral("visualReadingResultLabel"));
    m_resetRoiButton->setObjectName(QStringLiteral("visualResetRoiButton"));
    m_analyzeButton->setObjectName(QStringLiteral("visualAnalyzeButton"));
    m_confirmButton->setObjectName(QStringLiteral("visualConfirmButton"));
    m_cancelButton->setObjectName(QStringLiteral("visualCancelButton"));
    m_resultLabel->setWordWrap(true);
    m_imagePathLabel->setWordWrap(true);

    if (m_projectImages.isEmpty()) {
        m_projectImageCombo->addItem(QStringLiteral("没有可用项目图像资产"));
        m_projectImageCombo->setEnabled(false);
    } else {
        for (const VisualGaugeImageChoice& choice : m_projectImages) {
            m_projectImageCombo->addItem(choice.label, choice.path);
            m_projectImageCombo->setItemData(
                m_projectImageCombo->count() - 1, choice.assetId, Qt::UserRole + 1);
        }
    }

    auto* imageButtons = new QHBoxLayout;
    imageButtons->addWidget(m_projectImageCombo, 1);
    imageButtons->addWidget(m_externalButton);
    imageButtons->addWidget(m_wholeImageCheckBox);

    auto* resultGroup = new QGroupBox(QStringLiteral("分析结果"), this);
    auto* resultLayout = new QVBoxLayout(resultGroup);
    resultLayout->addWidget(m_resultLabel);

    auto* bottomButtons = new QHBoxLayout;
    bottomButtons->addWidget(m_resetRoiButton);
    bottomButtons->addWidget(m_analyzeButton);
    bottomButtons->addStretch();
    bottomButtons->addWidget(m_confirmButton);
    bottomButtons->addWidget(m_cancelButton);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(QStringLiteral("项目图片 / 外部图片:"), this));
    layout->addLayout(imageButtons);
    layout->addWidget(m_imagePathLabel);
    layout->addWidget(m_canvas, 1);
    layout->addWidget(resultGroup);
    layout->addLayout(bottomButtons);

    connect(m_projectImageCombo,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            &VisualGaugeReadingDialog::chooseProjectImage);
    connect(m_externalButton,
            &QPushButton::clicked,
            this,
            &VisualGaugeReadingDialog::chooseExternalImage);
    connect(m_wholeImageCheckBox,
            &QCheckBox::toggled,
            this,
            [this](bool checked) {
                static_cast<GaugeImageCanvas*>(m_canvas)->setWholeImage(checked);
                updateControls();
            });
    connect(m_resetRoiButton,
            &QPushButton::clicked,
            this,
            &VisualGaugeReadingDialog::resetRoi);
    connect(m_analyzeButton, &QPushButton::clicked, this, &VisualGaugeReadingDialog::analyze);
    connect(m_confirmButton,
            &QPushButton::clicked,
            this,
            &VisualGaugeReadingDialog::confirmResult);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    m_confirmButton->setDefault(true);
    applyProductDialogStyle(this);
    styleProductButton(m_externalButton, ProductButtonRole::Secondary);
    styleProductButton(m_resetRoiButton, ProductButtonRole::Secondary);
    styleProductButton(m_analyzeButton, ProductButtonRole::Primary);
    styleProductButton(m_confirmButton, ProductButtonRole::Primary);
    styleProductButton(m_cancelButton, ProductButtonRole::Secondary);
    updateControls();
    if (!m_projectImages.isEmpty()) {
        chooseProjectImage(0);
    }
}

VisualGaugeReadingResult VisualGaugeReadingDialog::result() const
{
    return m_result;
}

QString VisualGaugeReadingDialog::selectedImagePath() const
{
    return m_selectedImagePath;
}

QString VisualGaugeReadingDialog::selectedImageAssetId() const
{
    return m_selectedImageAssetId;
}

void VisualGaugeReadingDialog::chooseProjectImage(int index)
{
    if (index < 0 || index >= m_projectImageCombo->count()
        || !m_projectImageCombo->itemData(index).isValid()) {
        return;
    }
    loadImage(m_projectImageCombo->itemData(index).toString(),
              m_projectImageCombo->itemData(index, Qt::UserRole + 1).toString());
}

void VisualGaugeReadingDialog::chooseExternalImage()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择仪表图片"), QDir::homePath(), imageFilter());
    if (!path.isEmpty()) {
        loadImage(path);
    }
}

void VisualGaugeReadingDialog::resetRoi()
{
    m_wholeImageCheckBox->setChecked(false);
    static_cast<GaugeImageCanvas*>(m_canvas)->resetRoi();
    m_hasSuccessfulResult = false;
    m_result = VisualGaugeReadingResult();
    m_resultLabel->setText(QStringLiteral("ROI 已清除，请在图像上拖拽矩形。"));
    updateControls();
}

void VisualGaugeReadingDialog::analyze()
{
    if (m_image.isNull()) {
        m_result = VisualGaugeReadingResult();
        m_result.failureReason = QStringLiteral("尚未选择图片。 ").trimmed();
        showResult(m_result);
        return;
    }
    m_result = VisualGaugeReader::read(m_image, m_profile, effectiveRoi());
    m_hasSuccessfulResult = m_result.success;
    showResult(m_result);
}

void VisualGaugeReadingDialog::confirmResult()
{
    if (m_hasSuccessfulResult && m_result.success) {
        accept();
    }
}

void VisualGaugeReadingDialog::loadImage(const QString& path, const QString& assetId)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        m_image = {};
        m_selectedImagePath.clear();
        m_selectedImageAssetId.clear();
        static_cast<GaugeImageCanvas*>(m_canvas)->setImage({});
        m_resultLabel->setText(QStringLiteral("FAILED: 无法读取图片：%1")
                                   .arg(reader.errorString()));
        m_hasSuccessfulResult = false;
        updateControls();
        return;
    }
    m_image = image;
    m_selectedImagePath = path;
    m_selectedImageAssetId = assetId.trimmed();
    m_result = VisualGaugeReadingResult();
    m_hasSuccessfulResult = false;
    m_imagePathLabel->setText(QStringLiteral("图片: %1 (%2x%3)")
                                  .arg(path)
                                  .arg(image.width())
                                  .arg(image.height()));
    static_cast<GaugeImageCanvas*>(m_canvas)->setImage(image);
    m_resultLabel->setText(QStringLiteral("图片已加载。请框选 ROI 或使用整图。"));
    updateControls();
}

QRect VisualGaugeReadingDialog::effectiveRoi() const
{
    const QRect selected = static_cast<GaugeImageCanvas*>(m_canvas)->roi();
    return selected.isValid() && selected.width() >= 2 && selected.height() >= 2
        ? selected
        : QRect(QPoint(0, 0), m_image.size());
}

void VisualGaugeReadingDialog::updateControls()
{
    const bool hasImage = !m_image.isNull();
    m_resetRoiButton->setEnabled(hasImage);
    m_analyzeButton->setEnabled(hasImage);
    m_confirmButton->setEnabled(m_hasSuccessfulResult);
}

void VisualGaugeReadingDialog::showResult(const VisualGaugeReadingResult& result)
{
    static_cast<GaugeImageCanvas*>(m_canvas)->setOverlay(result.diagnosticOverlay);
    if (!result.success) {
        m_resultLabel->setText(QStringLiteral("FAILED\n原因: %1\n中心来源: %2")
                                   .arg(result.failureReason)
                                   .arg(gaugeCenterSourceDisplayName(result.centerSource)));
        m_confirmButton->setEnabled(false);
        return;
    }
    m_resultLabel->setText(
        QStringLiteral("成功\n中心: (%1, %2)\n针尖: (%3, %4)\n角度: %5°\n读数: %6 %7\nheuristic confidence: %8\n中心来源: %9")
            .arg(result.center.x(), 0, 'f', 1)
            .arg(result.center.y(), 0, 'f', 1)
            .arg(result.needleTip.x(), 0, 'f', 1)
            .arg(result.needleTip.y(), 0, 'f', 1)
            .arg(result.needleAngleDegrees, 0, 'f', 1)
            .arg(result.value, 0, 'f', 3)
            .arg(m_profile.unit)
            .arg(result.confidence, 0, 'f', 2)
            .arg(gaugeCenterSourceDisplayName(result.centerSource)));
    m_confirmButton->setEnabled(true);
}

} // namespace vision3d
