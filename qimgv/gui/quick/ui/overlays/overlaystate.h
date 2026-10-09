#pragma once

#include <QObject>
#include <QPointF>
#include <QtQml/qqmlregistration.h>

// Visibility of one overlay of the Qt Quick UI. `created` turns true the
// first time the overlay opens and stays true, so QML creates the overlay on
// first use (Loader) and keeps it for its fade-out; nothing is created
// before that. OverlayCoordinator decides when overlays open and close; QML
// closes them through close() (close buttons, cancel).
//
// GUI thread only.
class OverlayState final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by OverlayCoordinator")
    Q_PROPERTY(bool open READ isOpen NOTIFY openChanged FINAL)
    Q_PROPERTY(bool created READ isCreated NOTIFY createdChanged FINAL)
    Q_PROPERTY(bool takesKeyboardFocus READ takesKeyboardFocus CONSTANT FINAL)
    Q_PROPERTY(QPointF anchor READ anchor NOTIFY anchorChanged FINAL)

public:
    // takesKeyboardFocus: the open overlay receives the keyboard input
    // (FloatingWidget::acceptKeyboardFocus()).
    explicit OverlayState(bool takesKeyboardFocus, QObject *parent = nullptr);

    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] bool isCreated() const;
    [[nodiscard]] bool takesKeyboardFocus() const;
    // Window position the overlay opens at, for overlays placed at the
    // pointer; QML keeps the overlay inside the window.
    [[nodiscard]] QPointF anchor() const;

    void setOpen(bool open);
    void setAnchor(QPointF anchor);
    void toggle();
    Q_INVOKABLE void close();

signals:
    void openChanged();
    void createdChanged();
    void anchorChanged();

private:
    const bool mTakesKeyboardFocus;
    bool mOpen = false;
    bool mCreated = false;
    QPointF mAnchor;
};
