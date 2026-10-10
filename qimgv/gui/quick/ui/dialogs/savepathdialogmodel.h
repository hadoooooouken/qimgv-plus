#pragma once

#include <QByteArray>
#include <QList>
#include <QStringList>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

#include "dialogsession.h"
#include "gui/ports/dialogport.h"

// "Save File as..." (SaveFileDialog.qml, the platform file dialog), the
// rules of MW::getSaveFileName(): the dialog starts at the suggested path
// with one filter per writable format, the filter of the suggested file's
// suffix selected (saveFileFiltersFor()). Accepting reports the chosen local
// path; rejecting or abandoning reports none.
//
// Owned by DialogCoordinator. GUI thread only.
class SavePathDialogModel final : public DialogSession {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by DialogCoordinator")
    Q_PROPERTY(QStringList nameFilters READ nameFilters NOTIFY requestChanged FINAL)
    Q_PROPERTY(int selectedFilterIndex READ selectedFilterIndex NOTIFY requestChanged FINAL)
    Q_PROPERTY(QUrl suggestedFile READ suggestedFile NOTIFY requestChanged FINAL)
    Q_PROPERTY(QUrl suggestedFolder READ suggestedFolder NOTIFY requestChanged FINAL)

public:
    explicit SavePathDialogModel(QObject *parent = nullptr);

    [[nodiscard]] QStringList nameFilters() const;
    [[nodiscard]] int selectedFilterIndex() const;
    [[nodiscard]] QUrl suggestedFile() const;
    [[nodiscard]] QUrl suggestedFolder() const;
    [[nodiscard]] SavePathResult result() const;

    // Opens the dialog for request with the filters of writerFormats
    // (QImageWriter::supportedImageFormats()); false while another request
    // is open.
    [[nodiscard]] bool start(const SavePathRequest &request,
                             const QList<QByteArray> &writerFormats);

    // fileUrl: the selected file; a URL that is no local file is rejected.
    Q_INVOKABLE void accept(const QUrl &fileUrl);
    Q_INVOKABLE void reject();

signals:
    void requestChanged();

private:
    QStringList mNameFilters;
    int mSelectedFilterIndex = 0;
    QString mSuggestedPath;
    SavePathResult mResult;
};
