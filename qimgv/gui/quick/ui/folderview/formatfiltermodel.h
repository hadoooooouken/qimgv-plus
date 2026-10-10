#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

#include "utils/formatgroups.h"

// The format filter of the folder view (FormatFilterComboBox in the widget
// UI): "All formats", or any set of formats picked one by one or by their
// category. Rows are the categories (roles label, checkState - a
// Qt::CheckState, formats - a list of {label, checked, index} for the format
// check boxes); the field shows displayText. The rules:
// - picking a format while all formats are shown starts a new set with it;
// - a category checks or unchecks all its formats;
// - an empty set means all formats again.
// Each change publishes the checked extensions (empty for all formats).
// GUI thread only.
class FormatFilterModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by FolderViewController.formatFilter")
    Q_PROPERTY(bool allFormats READ allFormats NOTIFY selectionChanged FINAL)
    Q_PROPERTY(QString displayText READ displayText NOTIFY selectionChanged FINAL)
    Q_PROPERTY(QString allFormatsLabel READ allFormatsLabel CONSTANT FINAL)
    // The widest text the field can show, for its width.
    Q_PROPERTY(QStringList displayTexts READ displayTexts CONSTANT FINAL)

public:
    enum Role {
        LabelRole = Qt::UserRole + 1,
        CheckStateRole,
        FormatsRole,
    };
    Q_ENUM(Role)

    explicit FormatFilterModel(QObject *parent = nullptr);
    // categories: the format categories in display order.
    FormatFilterModel(QList<FormatCategory> categories, QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] bool allFormats() const;
    [[nodiscard]] QString displayText() const;
    [[nodiscard]] static QString allFormatsLabel();
    [[nodiscard]] QStringList displayTexts() const;
    // Empty for all formats.
    [[nodiscard]] QStringList checkedExtensions() const;

    // Shows the stored filter; extensions matching no format show all
    // formats. Publishes nothing.
    void setCheckedExtensions(const QStringList &extensions);

    Q_INVOKABLE void selectAllFormats();
    Q_INVOKABLE void setFormatChecked(int formatIndex, bool checked);
    Q_INVOKABLE void setCategoryChecked(int categoryIndex, bool checked);

signals:
    void selectionChanged();
    // The user changed the filter.
    void filterSelected(const QStringList &extensions);

private:
    struct Format {
        QString label;
        QStringList extensions;
        int category = 0;
    };

    [[nodiscard]] bool anyFormatChecked() const;
    [[nodiscard]] Qt::CheckState categoryState(int categoryIndex) const;
    void resetToAllFormats();
    void publish();

    QList<FormatCategory> mCategories;
    QList<Format> mFormats;
    QList<bool> mChecked;
};
