#include "copyoverlay.h"
#include "gui/customwidgets/iconwidget.h"
#include "gui/customwidgets/iconbutton.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

CopyOverlay::CopyOverlay(FloatingWidgetContainer *parent) :
    OverlayWidget(parent)
{
    setupUi();
    hide();
    setFadeEnabled(true);

    closeButton->setIconPath(":/res/icons/common/overlay/close-dim16.png");
    headerIcon->setIconPath(":/res/icons/common/overlay/copy16.png");
    headerLabel->setText(tr("Copy to..."));
    mode = OVERLAY_COPY;

    createShortcuts();

    paths = settings->savedPaths();
    if(paths.count() < maxPathCount)
        createDefaultPaths();
    createPathWidgets();

    setAcceptKeyboardFocus(true);

    if(parent)
        setContainerSize(parent->size());

    readSettings();
    connect(settings, &Settings::settingsChanged, this, &CopyOverlay::readSettings);
}

CopyOverlay::~CopyOverlay() = default;

void CopyOverlay::setupUi() {
    this->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    this->setMinimumSize(200, 0);
    this->setMaximumSize(240, 16777215);
    this->setFocusPolicy(Qt::StrongFocus);

    QVBoxLayout *verticalLayout = new QVBoxLayout(this);
    verticalLayout->setSpacing(8);
    verticalLayout->setContentsMargins(0, 0, 0, 4);

    // --- header ---
    QWidget *headerWidget = new QWidget(this);
    headerWidget->setAccessibleName("OverlayHeaderWidget");

    QHBoxLayout *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setSpacing(0);
    headerLayout->setContentsMargins(0, 0, 0, 0);

    headerIcon = new IconWidget(headerWidget);
    headerIcon->setAccessibleName("OverlayHeaderIcon");
    headerLayout->addWidget(headerIcon);

    headerLabel = new QLabel(headerWidget);
    QSizePolicy labelPolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    labelPolicy.setHorizontalStretch(1);
    headerLabel->setSizePolicy(labelPolicy);
    headerLabel->setAccessibleName("OverlayHeaderLabel");
    headerLabel->setContentsMargins(0, 0, 0, 0);
    headerLayout->addWidget(headerLabel);

    closeButton = new IconButton(headerWidget);
    closeButton->setAccessibleName("OverlayHeaderButton");
    headerLayout->addWidget(closeButton);
    connect(closeButton, &IconButton::clicked, this, &CopyOverlay::hide);

    verticalLayout->addWidget(headerWidget);

    // --- path selectors layout ---
    pathSelectorsLayout = new QVBoxLayout();
    pathSelectorsLayout->setSpacing(0);
    pathSelectorsLayout->setContentsMargins(0, 0, 0, 0);
    verticalLayout->addLayout(pathSelectorsLayout);
}

void CopyOverlay::show() {
    OverlayWidget::show();
    setFocus();
}

void CopyOverlay::hide() {
    OverlayWidget::hide();
}

void CopyOverlay::setDialogMode(CopyOverlayMode _mode) {
    mode = _mode;
    if(mode == OVERLAY_COPY) {
        headerIcon->setIconPath(":/res/icons/common/overlay/copy16.png");
        headerLabel->setText(tr("Copy to..."));
    } else {
        headerIcon->setIconPath(":/res/icons/common/overlay/move16.png");
        headerLabel->setText(tr("Move to..."));
    }
}

CopyOverlayMode CopyOverlay::operationMode() {
    return mode;
}

void CopyOverlay::removePathWidgets() {
    for(int i = 0; i < pathWidgets.count(); i++) {
        QWidget *tmp = pathWidgets.at(i);
        pathSelectorsLayout->removeWidget(tmp);
        delete tmp;
    }
    pathWidgets.clear();
}

void CopyOverlay::createPathWidgets() {
    removePathWidgets();
    int count = (paths.length() > maxPathCount) ? maxPathCount : paths.length();
    for(int i = 0; i < count; i++) {
        PathSelectorMenuItem *item = new PathSelectorMenuItem(this);
        item->setDirectory(paths.at(i));
        item->setShortcutText(shortcuts.key(i));
        connect(item, &PathSelectorMenuItem::directorySelected, this, &CopyOverlay::requestFileOperation);
        pathWidgets.append(item);
        pathSelectorsLayout->addWidget(item);
    }
}

void CopyOverlay::createShortcuts() {
    for(int i = 0; i < maxPathCount; i++)
        shortcuts.insert(QString::number(i + 1), i);
}

void CopyOverlay::requestFileOperation(QString path) {
    if(mode == OVERLAY_COPY)
        emit copyRequested(path);
    else
        emit moveRequested(path);
}

void CopyOverlay::readSettings() {
    // don't interfere with the main panel
    if(settings->panelEnabled() && settings->panelPosition() == PanelPosition::PANEL_BOTTOM) {
        setPosition(FloatingWidgetPosition::TOPLEFT);
    } else {
        setPosition(FloatingWidgetPosition::BOTTOMLEFT);
    }
    update();
}

// for some reason, duplicate folders may appear in the configuration
// we remove duplicate directories
void CopyOverlay::saveSettings() {
    paths.clear();
    QStringList temp;
    for(int i = 0; i< pathWidgets.count(); i++) {
        QString path = pathWidgets.at(i)->path();
        if (!path.isEmpty()) {
            if (!temp.contains(path)) {
                temp << path;
                paths << pathWidgets.at(i)->directory();
            }
        }
    }
    settings->setSavedPaths(paths);
}

void CopyOverlay::createDefaultPaths() {
    QString home = QDir::homePath();
    if (paths.count() < 1 || paths.at(0).isEmpty() || paths.at(0).at(0) == '@') {
        paths.clear();
        paths << home;
    }
    if (paths.count() == 1 && paths.at(0) == home) {
        QDir dir(home);
        foreach(QFileInfo mfi, dir.entryInfoList()) {
            if (paths.count() >= maxPathCount) {
                break;
            }
            if(mfi.isFile()) {
                continue;
            } 
            else {
                if(mfi.fileName() == "."  
                || mfi.fileName() ==  ".."
                // hide directory
                || mfi.fileName().at(0) ==  '.' 
                // windows system directory
                || mfi.fileName() ==  "3D Objects"
                || mfi.fileName() ==  "Contacts"
                || mfi.fileName() ==  "Favorites"
                || mfi.fileName() ==  "Links"
                || mfi.fileName() ==  "Saved Games"
                || mfi.fileName() ==  "Searches"
                ) {
                    continue;
                }
                QString qpath(home + "/" + mfi.fileName());
                QFileInfo qinfo(qpath);
                if (qinfo.permission(QFile::WriteUser | QFile::ReadGroup)) {
                    paths << qpath;
                }
            }
        }
    }
}

// block native tab-switching so we can use it in shortcuts
bool CopyOverlay::focusNextPrevChild(bool mode) {
    return false;
}

void CopyOverlay::keyPressEvent(QKeyEvent *event) {
    event->accept();
    QString key = actionManager->keyForNativeScancode(event->nativeScanCode());
    if(shortcuts.contains(key))
        requestFileOperation(pathWidgets.at(shortcuts[key])->directory());
    else
        actionManager->processEvent(event);
}
