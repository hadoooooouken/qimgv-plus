#include "savepathdialogmodel.h"

#include <QDebug>
#include <QFileInfo>

#include "utils/savefilefilters.h"

SavePathDialogModel::SavePathDialogModel(QObject *parent) : DialogSession(parent) {}

QStringList SavePathDialogModel::nameFilters() const {
    return mNameFilters;
}

int SavePathDialogModel::selectedFilterIndex() const {
    return mSelectedFilterIndex;
}

QUrl SavePathDialogModel::suggestedFile() const {
    return mSuggestedPath.isEmpty() ? QUrl() : QUrl::fromLocalFile(mSuggestedPath);
}

QUrl SavePathDialogModel::suggestedFolder() const {
    return mSuggestedPath.isEmpty() ? QUrl()
                                    : QUrl::fromLocalFile(QFileInfo(mSuggestedPath).absolutePath());
}

SavePathResult SavePathDialogModel::result() const {
    return mResult;
}

bool SavePathDialogModel::start(const SavePathRequest &request,
                                const QList<QByteArray> &writerFormats) {
    if (!canStart("save as"))
        return false;
    mResult = {};
    const SaveFileFilters filters = saveFileFiltersFor(writerFormats, request.suggestedPath);
    mNameFilters = filters.filters;
    mSelectedFilterIndex = filters.selectedIndex();
    mSuggestedPath = request.suggestedPath;
    emit requestChanged();
    open();
    return true;
}

void SavePathDialogModel::accept(const QUrl &fileUrl) {
    if (!isOpen())
        return;
    if (!fileUrl.isLocalFile()) {
        qWarning() << "Qt Quick UI: the save dialog returned a location that is no local file;"
                      " nothing is saved:"
                   << fileUrl;
        reject();
        return;
    }
    mResult = {.path = fileUrl.toLocalFile()};
    conclude();
}

void SavePathDialogModel::reject() {
    if (!isOpen())
        return;
    mResult = {};
    conclude();
}
