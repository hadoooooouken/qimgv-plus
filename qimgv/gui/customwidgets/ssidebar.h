#ifndef SSIDEBAR_H
#define SSIDEBAR_H

#include <QWidget>
#include <QLabel>
#include <QPainter>
#include <QStyleOption>
#include <QBoxLayout>
#include "gui/customwidgets/iconwidget.h"

class SSideBarItem;

class SSideBar : public QWidget {
    Q_OBJECT
public:
    explicit SSideBar(QWidget *parent = nullptr);
    void addEntry(FluentIcon icon, const QString &name);
    void selectEntry(int idx);

private:
    QBoxLayout *layout;
    QList<SSideBarItem *> entries;
    Qt::Orientation orientation = Qt::Vertical;
    void selectEntryAt(QPoint pos);

signals:
    void entrySelected(int);

protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);

    void paintEvent(QPaintEvent *event);
};

class SSideBarItem : public QWidget {
    Q_OBJECT
public:
    explicit SSideBarItem(FluentIcon icon, const QString &name, QWidget *parent = nullptr);
    void setHighlighted(bool mode);
    bool highlighted();

private:
    QBoxLayout *layout;
    IconWidget iconWidget;
    QLabel textLabel;
    bool mHighlighted = false;

protected:
    void paintEvent(QPaintEvent *event);
    void changeEvent(QEvent *event) override;
};

#endif // SSIDEBAR_H
