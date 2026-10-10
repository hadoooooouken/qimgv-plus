#pragma once

#include <QSize>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

#include <optional>

#include "dialogsession.h"
#include "gui/ports/dialogport.h"

// What the resize dialog starts from.
struct ResizeDialogInput {
    QSize originalSize;
    // Target of "Fit to desktop" / "Fill desktop" (the primary screen).
    QSize desktopSize;
    // Upscayl models found next to the application; none disables Upscayl.
    QStringList upscaylModels;
    // Stored preferences: "Use Upscayl" and the model it uses.
    bool useUpscayl = false;
    QString upscaylModel;
};

// Preferences to store after an accepted resize.
struct ResizePreferences {
    bool useUpscayl = false;
    QString upscaylModel;
};

// Image resize settings (ResizeDialog.qml), the rules of the widget
// ResizeDialog: a size by percent of the original or an absolute size, the
// other side following the edited one while the aspect ratio is kept (always
// in percent mode), common screen sizes, fit / fill the desktop, reset to the
// original; Upscayl applies only to upscales, and only when models exist.
// Editing the percent or a side selects its mode (the widget dialog started
// with every field enabled and switched only through its radio buttons).
// Sides stay within 1 - 65535 pixels.
//
// Accepting reports the request when the size changed (nothing otherwise)
// and the Upscayl preferences to store; rejecting and abandoning report
// nothing.
//
// Owned by DialogCoordinator. GUI thread only.
class ResizeDialogModel final : public DialogSession {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by DialogCoordinator")
    Q_PROPERTY(int originalWidth READ originalWidth NOTIFY stateChanged FINAL)
    Q_PROPERTY(int originalHeight READ originalHeight NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool byPercent READ byPercent NOTIFY stateChanged FINAL)
    Q_PROPERTY(double percent READ percent NOTIFY stateChanged FINAL)
    Q_PROPERTY(double minimumPercent READ minimumPercent CONSTANT FINAL)
    Q_PROPERTY(double maximumPercent READ maximumPercent CONSTANT FINAL)
    Q_PROPERTY(int targetWidth READ targetWidth NOTIFY stateChanged FINAL)
    Q_PROPERTY(int targetHeight READ targetHeight NOTIFY stateChanged FINAL)
    Q_PROPERTY(int minimumSide READ minimumSide CONSTANT FINAL)
    Q_PROPERTY(int maximumSide READ maximumSide CONSTANT FINAL)
    Q_PROPERTY(bool keepAspectRatio READ keepAspectRatio NOTIFY stateChanged FINAL)
    Q_PROPERTY(int filterIndex READ filterIndex NOTIFY stateChanged FINAL)
    Q_PROPERTY(QStringList commonSizes READ commonSizes CONSTANT FINAL)
    Q_PROPERTY(int commonSizeIndex READ commonSizeIndex NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool upscaylAvailable READ upscaylAvailable NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool upscaylApplies READ upscaylApplies NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool useUpscayl READ useUpscayl NOTIFY stateChanged FINAL)
    Q_PROPERTY(QStringList upscaylModels READ upscaylModels NOTIFY stateChanged FINAL)
    Q_PROPERTY(int upscaylModelIndex READ upscaylModelIndex NOTIFY stateChanged FINAL)

public:
    // Order of the filter list; the default is Magic Kernel Sharp 2021.
    enum class Filter { Nearest, Bilinear, SmartSharpen, MagicKernelSharp2021 };
    Q_ENUM(Filter)

    explicit ResizeDialogModel(QObject *parent = nullptr);

    [[nodiscard]] int originalWidth() const;
    [[nodiscard]] int originalHeight() const;
    [[nodiscard]] bool byPercent() const;
    [[nodiscard]] double percent() const;
    [[nodiscard]] double minimumPercent() const;
    [[nodiscard]] double maximumPercent() const;
    [[nodiscard]] int targetWidth() const;
    [[nodiscard]] int targetHeight() const;
    [[nodiscard]] QSize targetSize() const;
    [[nodiscard]] int minimumSide() const;
    [[nodiscard]] int maximumSide() const;
    [[nodiscard]] bool keepAspectRatio() const;
    // A Filter value.
    [[nodiscard]] int filterIndex() const;
    // Labels of the common sizes ("1920 x 1080 (FullHD)", ...).
    [[nodiscard]] QStringList commonSizes() const;
    // The selected common size, -1 for none.
    [[nodiscard]] int commonSizeIndex() const;
    [[nodiscard]] bool upscaylAvailable() const;
    // The target is larger than the original on a side.
    [[nodiscard]] bool upscaylApplies() const;
    [[nodiscard]] bool useUpscayl() const;
    [[nodiscard]] QStringList upscaylModels() const;
    [[nodiscard]] int upscaylModelIndex() const;

    [[nodiscard]] std::optional<ResizeRequest> result() const;
    [[nodiscard]] std::optional<ResizePreferences> preferencesToStore() const;

    // Opens the dialog for input; false while another request is open.
    [[nodiscard]] bool start(const ResizeDialogInput &input);

    Q_INVOKABLE void setByPercent(bool byPercent);
    Q_INVOKABLE void setPercent(double percent);
    Q_INVOKABLE void setTargetWidth(int width);
    Q_INVOKABLE void setTargetHeight(int height);
    Q_INVOKABLE void setKeepAspectRatio(bool keep);
    // index into commonSizes(); -1 goes back to the original size.
    Q_INVOKABLE void selectCommonSize(int index);
    Q_INVOKABLE void fitDesktop();
    Q_INVOKABLE void fillDesktop();
    Q_INVOKABLE void reset();
    Q_INVOKABLE void setFilterIndex(int index);
    Q_INVOKABLE void setUseUpscayl(bool use);
    Q_INVOKABLE void setUpscaylModelIndex(int index);
    Q_INVOKABLE void accept();
    Q_INVOKABLE void reject();

signals:
    void stateChanged();

private:
    enum class Side { Width, Height };

    void applyPercent();
    void applySide(Side side, int value);
    void scaleOriginalTo(QSize bounds, Qt::AspectRatioMode mode);
    void setTarget(QSize size);
    [[nodiscard]] bool effectiveUseUpscayl() const;
    [[nodiscard]] QString selectedUpscaylModel() const;

    ResizeDialogInput mInput;
    bool mByPercent = true;
    double mPercent = 0;
    QSize mTarget;
    bool mKeepAspectRatio = true;
    Side mLastEdited = Side::Width;
    int mFilterIndex = 0;
    int mCommonSizeIndex = -1;
    bool mUseUpscayl = false;
    int mUpscaylModelIndex = -1;
    std::optional<ResizeRequest> mResult;
    std::optional<ResizePreferences> mPreferences;
};
