#pragma once

#include <QDialog>
#include <QComboBox>
#include <QRadioButton>
#include "shortcutbuilder.h"
#include "utils/actions.h"
#include "components/actionmanager/actionmanager.h"
#include "components/scriptmanager/scriptmanager.h"

class QRadioButton;
class QComboBox;
class QLabel;
class KeySequenceEdit;
class QDialogButtonBox;

class ShortcutCreatorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ShortcutCreatorDialog(QWidget *parent = nullptr);
    ~ShortcutCreatorDialog();
    QString selectedAction();
    QString selectedShortcut();
    void setAction(QString);
    void setShortcut(QString);

private slots:
    void onShortcutEdited();

private:
    void setupUi();

    QRadioButton *actionsRadioButton = nullptr;
    QComboBox *actionsComboBox = nullptr;
    QRadioButton *scriptsRadioButton = nullptr;
    QComboBox *scriptsComboBox = nullptr;
    KeySequenceEdit *sequenceEdit = nullptr;
    QLabel *warningLabel = nullptr;
    QDialogButtonBox *buttonBox = nullptr;

    QList<QString> actionList, scriptList;
};
