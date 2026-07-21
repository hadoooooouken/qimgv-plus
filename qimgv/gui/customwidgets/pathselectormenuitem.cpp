#include "pathselectormenuitem.h"

PathSelectorMenuItem::PathSelectorMenuItem(QWidget *parent)
    : MenuItem(parent),
      mDirectory(""),
      mPath("")
{
    setFocusPolicy(Qt::NoFocus);
    setIcon(FluentIcon::Folder20, IconWidget::kMenuItemIconSizePx);
    mIconWidget.setAttribute(Qt::WA_TransparentForMouseEvents, false);
    connect(&mIconWidget, &IconButton::clicked, [this]() {
        QFileDialog dialog;
        dialog.setDirectory(mDirectory);
        dialog.setWindowTitle("Select directory");
        dialog.setWindowModality(Qt::ApplicationModal);
        dialog.setFileMode(QFileDialog::Directory);
        dialog.setOption(QFileDialog::ShowDirsOnly);
        dialog.setOption(QFileDialog::DontResolveSymlinks);
        connect(&dialog, &QFileDialog::fileSelected, this, &PathSelectorMenuItem::setDirectory);
        dialog.exec();
    });
}

QString PathSelectorMenuItem::directory() {
    return mDirectory;
}

QString PathSelectorMenuItem::path() {
    return mPath;
}

void PathSelectorMenuItem::setDirectory(QString path) {
    mDirectory = path;
    mPath = path;
    QString stripped;
    stripped = mDirectory.split("/", Qt::SkipEmptyParts).last();
    this->mTextLabel.setText(stripped);
}

void PathSelectorMenuItem::onPress() {
    if(!mDirectory.isEmpty())
        emit directorySelected(mDirectory);
}
