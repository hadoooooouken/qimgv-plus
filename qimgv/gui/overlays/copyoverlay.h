#pragma once

#include <QGridLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QKeyEvent>
#include "gui/customwidgets/overlaywidget.h"
#include "gui/customwidgets/pathselectormenuitem.h"
#include "components/actionmanager/actionmanager.h"

class IconWidget;
class IconButton;
class QVBoxLayout;

enum CopyOverlayMode {
    OVERLAY_COPY,
    OVERLAY_MOVE
};

class CopyOverlay : public OverlayWidget {
    Q_OBJECT
public:
    CopyOverlay(FloatingWidgetContainer *parent);
    ~CopyOverlay();
    void saveSettings();    
    void setDialogMode(CopyOverlayMode _mode);
    CopyOverlayMode operationMode();

public slots:
    void show();
    void hide();

signals:
    void copyRequested(QString);
    void moveRequested(QString);

protected:
    void keyPressEvent(QKeyEvent *event);

    bool focusNextPrevChild(bool);
private slots:
    void requestFileOperation(QString path);
    void readSettings();

private:
    void setupUi();
    void createDefaultPaths();
    void createPathWidgets();
    void createShortcuts();

    IconWidget *headerIcon = nullptr;
    QLabel *headerLabel = nullptr;
    IconButton *closeButton = nullptr;
    QVBoxLayout *pathSelectorsLayout = nullptr;

    QList<PathSelectorMenuItem*> pathWidgets;
    const int maxPathCount = 9;
    QStringList paths;
    QMap<QString, int> shortcuts;
    CopyOverlayMode mode;
    void removePathWidgets();

};
