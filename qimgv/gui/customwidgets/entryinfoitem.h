#ifndef ENTRYINFOITEM_H
#define ENTRYINFOITEM_H

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStyleOption>
#include <QPainter>
#include <QDebug>

class EntryInfoItem : public QWidget
{
    Q_OBJECT
public:
    explicit EntryInfoItem(QWidget *parent = nullptr);

    // stacked = false (default): name and value sit side by side, value
    //   width fixed to match the overlay's narrow column layout (142px).
    // stacked = true: name on its own line, value wraps below it at the
    //   full panel width instead. Use this for long free-form fields like
    //   the generation prompt.
    //   NOTE: this width is still a *fixed* value (STACKED_VALUE_WIDTH,
    //   matching ImageInfoOverlay's fixed 280px panel width minus margins),
    //   not an unconstrained/auto width. ImageInfoOverlay's layouts use
    //   QLayout::SetFixedSize, which resizes the whole overlay to its
    //   sizeHint() on every change - if valueLabel's width were left
    //   unconstrained, QLabel's word-wrap sizeHint heuristic would pick a
    //   different "optimal" width for every different prompt, and the
    //   overlay window would visibly resize each time setInfo() is called.
    void setInfo(QString _name, QString _value, bool stacked = false);

protected:
    void paintEvent(QPaintEvent *event);

private:
    QString name;
    QString value;

    QVBoxLayout outerLayout;
    QHBoxLayout headerLayout;
    QLabel nameLabel, valueLabel;

    bool isStacked = false;
    void applyLayoutMode(bool stacked);

    // ImageInfoOverlay panel is fixed at 280px (see imageinfooverlay.cpp
    // setupUi: setMinimumSize/setMaximumSize(280, ...)). Available width for
    // this label in stacked mode = 280
    //   - verticalLayout margins (0 left + 0 right)
    //   - entryLayout margins     (1 left + 1 right)
    //   - this widget's outerLayout margins (9 left + 9 right)
    // = 280 - 20 = 260. Keep in sync if any of those margins change.
    static constexpr int STACKED_VALUE_WIDTH = 260;
};

#endif // ENTRYINFOITEM_H
