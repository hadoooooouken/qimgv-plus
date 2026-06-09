#pragma once

#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>
#include "components/actionmanager/actionmanager.h"
#include "components/scriptmanager/scriptmanager.h"

class ActionButton;
class ContextMenuItem;

class ContextMenu : public QWidget {
    Q_OBJECT

public:
    explicit ContextMenu(QWidget *parent = nullptr);
    ~ContextMenu();

    void setImageEntriesEnabled(bool mode);
    void setCasSettingsVisible(bool visible);
    QSize sizeHint() const override;

public slots:
    void showAt(QPoint pos);
    void setGeometry(QRect geom);

signals:
    void showScriptSettings();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void switchToMainPage();
    void switchToScriptsPage();

private:
    void setupUi();
    void fillOpenWithMenu();

    QStackedWidget *m_stackedWidget;

    // Main page widgets
    ActionButton    *m_fitWindow;
    ActionButton    *m_fitWidth;
    ActionButton    *m_fitWindowStretch;
    ActionButton    *m_zoomOriginal;
    ActionButton    *m_zoomIn;
    ActionButton    *m_zoomOut;

    ActionButton    *m_rotateLeft;
    ActionButton    *m_rotateRight;
    ActionButton    *m_flipV;
    ActionButton    *m_flipH;
    ActionButton    *m_crop;
    ActionButton    *m_resize;

    ContextMenuItem *m_colorAdjustments;
    ContextMenuItem *m_panoramaMode;
    ContextMenuItem *m_casSettings;
    ContextMenuItem *m_print;
    ContextMenuItem *m_copy;
    ContextMenuItem *m_move;
    ContextMenuItem *m_trash;
    ContextMenuItem *m_open;
    ContextMenuItem *m_folderView;
    ContextMenuItem *m_settings;
    ContextMenuItem *m_openWith;
    ContextMenuItem *m_showLocation;

    // Scripts page widgets
    ContextMenuItem *m_backButton;
    ContextMenuItem *m_scriptSetupButton;
    QVBoxLayout     *m_scriptsLayout;

    QPoint customGlobalPos;
    bool hasCustomPos = false;
    QPoint dragStartPosition;
    QPoint dragStartWidgetPosition;
    bool isDragging = false;
};
