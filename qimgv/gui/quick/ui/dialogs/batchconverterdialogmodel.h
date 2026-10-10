#pragma once

#include <QList>
#include <QPointer>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <optional>

#include "batchconversionservice.h"
#include "batchqueuemodel.h"
#include "components/batchconverter/batchjobrules.h"
#include "dialogsession.h"
#include "gui/ports/dialogport.h"

// What the batch converter dialog starts from.
struct BatchConverterDialogInput {
    QStringList filePaths;
    // Preferred output folder (the folder view's); falls back to the folder
    // of the first file.
    QString defaultOutputDirectory;
    // Size the resize fields start from (the first file's); invalid uses
    // BatchJobRules::fallbackOriginalSize().
    QSize originalSize;
    BatchJobRules::SaveQualityDefaults saveQualities;
    // Upscayl models found next to the application; none disables Upscayl.
    QStringList upscaylModels;
    // Stored preferences: "Use Upscayl" (shared with the resize dialog) and
    // the batch model.
    bool useUpscayl = false;
    QString upscaylModel;
    // Device pixels per logical pixel of the thumbnails.
    qreal devicePixelRatio = 1.0;
};

// Preferences to store once a batch started.
struct BatchConverterPreferences {
    bool useUpscayl = false;
    // Empty when no models exist (the stored model is kept then).
    QString upscaylModel;
};

// The batch converter (BatchConverterDialog.qml), with the rules of the
// widget BatchConverterDialog (shared through BatchJobRules): output format
// and quality, resize by percent or to a bounding size with the aspect fit
// mode, Upscayl, filter, rotation and flips, colour adjustments, output
// folder, file name pattern; the file queue (BatchQueueModel) with progress.
//
// The conversion runs through the BatchConversionService the request comes
// with. Closing the dialog while a batch runs stops it, as closing the widget
// dialog did. The result reports whether a batch was started.
//
// Warnings and the completion message are shown in the dialog's message
// window (messageOpen) instead of message boxes.
//
// Owned by DialogCoordinator. GUI thread only.
class BatchConverterDialogModel final : public DialogSession {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by DialogCoordinator")
    Q_PROPERTY(BatchQueueModel *queue READ queue CONSTANT FINAL)
    Q_PROPERTY(QStringList formats READ formats CONSTANT FINAL)
    Q_PROPERTY(QStringList filters READ filters CONSTANT FINAL)
    Q_PROPERTY(QStringList commonSizes READ commonSizes CONSTANT FINAL)
    Q_PROPERTY(QStringList rotations READ rotations CONSTANT FINAL)
    Q_PROPERTY(QVariantList colorSliders READ colorSliders CONSTANT FINAL)
    Q_PROPERTY(double minimumPercent READ minimumPercent CONSTANT FINAL)
    Q_PROPERTY(double maximumPercent READ maximumPercent CONSTANT FINAL)
    Q_PROPERTY(int percentDecimals READ percentDecimals CONSTANT FINAL)
    Q_PROPERTY(int minimumSide READ minimumSide CONSTANT FINAL)
    Q_PROPERTY(int maximumSide READ maximumSide CONSTANT FINAL)
    Q_PROPERTY(QString patternHelp READ patternHelp CONSTANT FINAL)
    Q_PROPERTY(int thumbnailSize READ thumbnailSize CONSTANT FINAL)

    Q_PROPERTY(int formatIndex READ formatIndex NOTIFY settingsChanged FINAL)
    Q_PROPERTY(bool qualityAvailable READ qualityAvailable NOTIFY settingsChanged FINAL)
    Q_PROPERTY(int qualityMinimum READ qualityMinimum NOTIFY settingsChanged FINAL)
    Q_PROPERTY(int qualityMaximum READ qualityMaximum NOTIFY settingsChanged FINAL)
    Q_PROPERTY(int quality READ quality NOTIFY settingsChanged FINAL)
    Q_PROPERTY(QString qualityToolTip READ qualityToolTip NOTIFY settingsChanged FINAL)
    Q_PROPERTY(bool resizeEnabled READ resizeEnabled NOTIFY settingsChanged FINAL)
    Q_PROPERTY(bool byPercent READ byPercent NOTIFY settingsChanged FINAL)
    Q_PROPERTY(double percent READ percent NOTIFY settingsChanged FINAL)
    Q_PROPERTY(int targetWidth READ targetWidth NOTIFY settingsChanged FINAL)
    Q_PROPERTY(int targetHeight READ targetHeight NOTIFY settingsChanged FINAL)
    Q_PROPERTY(int originalWidth READ originalWidth NOTIFY settingsChanged FINAL)
    Q_PROPERTY(int originalHeight READ originalHeight NOTIFY settingsChanged FINAL)
    Q_PROPERTY(bool keepAspectRatio READ keepAspectRatio NOTIFY settingsChanged FINAL)
    Q_PROPERTY(int aspectFitMode READ aspectFitMode NOTIFY settingsChanged FINAL)
    Q_PROPERTY(int commonSizeIndex READ commonSizeIndex NOTIFY settingsChanged FINAL)
    Q_PROPERTY(int filterIndex READ filterIndex NOTIFY settingsChanged FINAL)
    Q_PROPERTY(bool upscaylAvailable READ upscaylAvailable NOTIFY settingsChanged FINAL)
    Q_PROPERTY(bool useUpscayl READ useUpscayl NOTIFY settingsChanged FINAL)
    Q_PROPERTY(QStringList upscaylModels READ upscaylModels NOTIFY settingsChanged FINAL)
    Q_PROPERTY(int upscaylModelIndex READ upscaylModelIndex NOTIFY settingsChanged FINAL)
    Q_PROPERTY(int rotationIndex READ rotationIndex NOTIFY settingsChanged FINAL)
    Q_PROPERTY(bool flipHorizontal READ flipHorizontal NOTIFY settingsChanged FINAL)
    Q_PROPERTY(bool flipVertical READ flipVertical NOTIFY settingsChanged FINAL)
    Q_PROPERTY(bool colorEnabled READ colorEnabled NOTIFY settingsChanged FINAL)
    Q_PROPERTY(QList<double> colorValues READ colorValues NOTIFY settingsChanged FINAL)
    Q_PROPERTY(QString outputDirectory READ outputDirectory NOTIFY settingsChanged FINAL)
    Q_PROPERTY(QUrl outputDirectoryUrl READ outputDirectoryUrl NOTIFY settingsChanged FINAL)
    Q_PROPERTY(bool createSubfolder READ createSubfolder NOTIFY settingsChanged FINAL)
    Q_PROPERTY(QString pattern READ pattern NOTIFY settingsChanged FINAL)
    Q_PROPERTY(bool overwrite READ overwrite NOTIFY settingsChanged FINAL)

    Q_PROPERTY(bool running READ isRunning NOTIFY runChanged FINAL)
    Q_PROPERTY(bool cancelling READ isCancelling NOTIFY runChanged FINAL)
    Q_PROPERTY(int progressValue READ progressValue NOTIFY runChanged FINAL)
    Q_PROPERTY(int progressMaximum READ progressMaximum NOTIFY runChanged FINAL)
    Q_PROPERTY(QString statusText READ statusText NOTIFY runChanged FINAL)

    Q_PROPERTY(bool messageOpen READ messageOpen NOTIFY messageChanged FINAL)
    Q_PROPERTY(bool messageIsWarning READ messageIsWarning NOTIFY messageChanged FINAL)
    Q_PROPERTY(QString messageTitle READ messageTitle NOTIFY messageChanged FINAL)
    Q_PROPERTY(QString messageText READ messageText NOTIFY messageChanged FINAL)

public:
    explicit BatchConverterDialogModel(QObject *parent = nullptr);
    ~BatchConverterDialogModel() override;

    [[nodiscard]] BatchQueueModel *queue();
    [[nodiscard]] QStringList formats() const;
    [[nodiscard]] QStringList filters() const;
    // "Original size" first.
    [[nodiscard]] QStringList commonSizes() const;
    [[nodiscard]] QStringList rotations() const;
    // One map per BatchJobRules::ColorSlider: label, minimum, maximum,
    // defaultValue, step, decimals, suffix.
    [[nodiscard]] QVariantList colorSliders() const;
    [[nodiscard]] double minimumPercent() const;
    [[nodiscard]] double maximumPercent() const;
    [[nodiscard]] int percentDecimals() const;
    [[nodiscard]] int minimumSide() const;
    [[nodiscard]] int maximumSide() const;
    [[nodiscard]] QString patternHelp() const;
    // Side of the square thumbnails, in logical pixels.
    [[nodiscard]] int thumbnailSize() const;

    [[nodiscard]] int formatIndex() const;
    [[nodiscard]] bool qualityAvailable() const;
    [[nodiscard]] int qualityMinimum() const;
    [[nodiscard]] int qualityMaximum() const;
    [[nodiscard]] int quality() const;
    [[nodiscard]] QString qualityToolTip() const;
    [[nodiscard]] bool resizeEnabled() const;
    [[nodiscard]] bool byPercent() const;
    [[nodiscard]] double percent() const;
    [[nodiscard]] int targetWidth() const;
    [[nodiscard]] int targetHeight() const;
    [[nodiscard]] int originalWidth() const;
    [[nodiscard]] int originalHeight() const;
    [[nodiscard]] bool keepAspectRatio() const;
    // An AspectFitMode value.
    [[nodiscard]] int aspectFitMode() const;
    // Into commonSizes(); 0 is the original size.
    [[nodiscard]] int commonSizeIndex() const;
    [[nodiscard]] int filterIndex() const;
    [[nodiscard]] bool upscaylAvailable() const;
    [[nodiscard]] bool useUpscayl() const;
    [[nodiscard]] QStringList upscaylModels() const;
    [[nodiscard]] int upscaylModelIndex() const;
    // Into rotations() (BatchJobRules::kRotations).
    [[nodiscard]] int rotationIndex() const;
    [[nodiscard]] bool flipHorizontal() const;
    [[nodiscard]] bool flipVertical() const;
    [[nodiscard]] bool colorEnabled() const;
    // In BatchJobRules::ColorSlider order.
    [[nodiscard]] QList<double> colorValues() const;
    [[nodiscard]] QString outputDirectory() const;
    [[nodiscard]] QUrl outputDirectoryUrl() const;
    [[nodiscard]] bool createSubfolder() const;
    [[nodiscard]] QString pattern() const;
    [[nodiscard]] bool overwrite() const;

    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] bool isCancelling() const;
    [[nodiscard]] int progressValue() const;
    [[nodiscard]] int progressMaximum() const;
    [[nodiscard]] QString statusText() const;

    [[nodiscard]] bool messageOpen() const;
    [[nodiscard]] bool messageIsWarning() const;
    [[nodiscard]] QString messageTitle() const;
    [[nodiscard]] QString messageText() const;

    [[nodiscard]] BatchConversionResult result() const;
    [[nodiscard]] std::optional<BatchConverterPreferences> preferencesToStore() const;
    // The draft as the next batch would start it.
    [[nodiscard]] BatchJob job() const;

    // Opens the dialog for input, converting through service, which must
    // stay alive until `finished`; false while another request is open.
    [[nodiscard]] bool start(const BatchConverterDialogInput &input,
                             BatchConversionService &service);

    Q_INVOKABLE void setFormatIndex(int index);
    Q_INVOKABLE void setQuality(int quality);
    Q_INVOKABLE void setResizeEnabled(bool enabled);
    Q_INVOKABLE void setByPercent(bool byPercent);
    Q_INVOKABLE void setPercent(double percent);
    Q_INVOKABLE void setTargetWidth(int width);
    Q_INVOKABLE void setTargetHeight(int height);
    Q_INVOKABLE void setKeepAspectRatio(bool keep);
    Q_INVOKABLE void setAspectFitMode(int mode);
    Q_INVOKABLE void selectCommonSize(int index);
    Q_INVOKABLE void resetSize();
    Q_INVOKABLE void setFilterIndex(int index);
    Q_INVOKABLE void setUseUpscayl(bool use);
    Q_INVOKABLE void setUpscaylModelIndex(int index);
    Q_INVOKABLE void setRotationIndex(int index);
    Q_INVOKABLE void setFlipHorizontal(bool flip);
    Q_INVOKABLE void setFlipVertical(bool flip);
    Q_INVOKABLE void setColorEnabled(bool enabled);
    Q_INVOKABLE void setColorValue(int slider, double value);
    Q_INVOKABLE void resetColorValues();
    Q_INVOKABLE void setOutputDirectory(const QString &directory);
    // A non-local URL is rejected with a warning.
    Q_INVOKABLE void setOutputDirectoryUrl(const QUrl &url);
    Q_INVOKABLE void setCreateSubfolder(bool create);
    Q_INVOKABLE void setPattern(const QString &pattern);
    Q_INVOKABLE void setOverwrite(bool overwrite);

    // Checks the draft (showing the first problem as a warning) and starts
    // the batch.
    Q_INVOKABLE void convert();
    // The Cancel / Stop button: stops a running batch, closes the dialog
    // otherwise.
    Q_INVOKABLE void stopOrClose();
    // Escape and the window's close button: closes the dialog, stopping a
    // running batch.
    Q_INVOKABLE void reject();
    Q_INVOKABLE void dismissMessage();

signals:
    void settingsChanged();
    void runChanged();
    void messageChanged();

private:
    void connectService(BatchConversionService &service);
    void detachService();
    void applySize(QSize target);
    void applyFormatQuality();
    void showMessage(const BatchJobRules::Message &message, bool warning);
    void closeMessage();
    void setStatusText(const QString &text);

    void onProgressUpdated(int index, BatchItemState state, const QString &details);
    void onFinished(int succeeded, int failed, int total);
    void onCancelled(int succeeded, int failed, int total);
    void onStartFailed(const QString &reason);

    BatchQueueModel mQueue;
    QPointer<BatchConversionService> mService;
    BatchConverterDialogInput mInput;
    QSize mOriginalSize;

    BatchJobRules::BatchJobDraft mDraft;
    int mFormatIndex = 0;
    BatchJobRules::QualityScale mQualityScale;
    int mCommonSizeIndex = 0;
    int mFilterIndex = 0;
    int mUpscaylModelIndex = -1;
    int mRotationIndex = 0;

    bool mRunning = false;
    bool mCancelling = false;
    bool mStarted = false;
    int mProcessed = 0;
    int mProgressMaximum = 0;
    QString mStatusText;

    bool mMessageOpen = false;
    bool mMessageIsWarning = false;
    BatchJobRules::Message mMessage;

    std::optional<BatchConverterPreferences> mPreferences;
};
