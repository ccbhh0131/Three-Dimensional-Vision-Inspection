#pragma once

#include "core/device/GaugeProfile.h"
#include "core/vision/VisualGaugeReader.h"

#include <QDialog>
#include <QList>
#include <QString>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QWidget;

namespace vision3d {

struct VisualGaugeImageChoice
{
    QString label;
    QString path;
    QString assetId;
};

class VisualGaugeReadingDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit VisualGaugeReadingDialog(const GaugeProfile& profile,
                                      const QList<VisualGaugeImageChoice>& projectImages,
                                      QWidget* parent = nullptr);

    VisualGaugeReadingResult result() const;
    QString selectedImagePath() const;
    QString selectedImageAssetId() const;

private slots:
    void chooseProjectImage(int index);
    void chooseExternalImage();
    void resetRoi();
    void analyze();
    void confirmResult();

private:
    void loadImage(const QString& path, const QString& assetId = QString());
    QRect effectiveRoi() const;
    void updateControls();
    void showResult(const VisualGaugeReadingResult& result);

    GaugeProfile m_profile;
    QList<VisualGaugeImageChoice> m_projectImages;
    QComboBox* m_projectImageCombo;
    QPushButton* m_externalButton;
    QCheckBox* m_wholeImageCheckBox;
    QWidget* m_canvas;
    QLabel* m_imagePathLabel;
    QLabel* m_resultLabel;
    QPushButton* m_resetRoiButton;
    QPushButton* m_analyzeButton;
    QPushButton* m_confirmButton;
    QPushButton* m_cancelButton;
    QImage m_image;
    QString m_selectedImagePath;
    QString m_selectedImageAssetId;
    VisualGaugeReadingResult m_result;
    bool m_hasSuccessfulResult = false;
};

} // namespace vision3d
