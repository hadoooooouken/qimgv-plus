#pragma once

#include <QDialog>

class QRadioButton;
class QComboBox;
class QLabel;
class KeySequenceEdit;
class QDialogButtonBox;
class ShortcutEditorModel;

// Widget view of the shortcut creator (ShortcutEditorModel): shows the
// model's open request; the answer goes to the model when the dialog
// finishes.
class ShortcutCreatorDialog : public QDialog
{
    Q_OBJECT

public:
    // model must outlive the dialog.
    explicit ShortcutCreatorDialog(ShortcutEditorModel &model, QWidget *parent = nullptr);
    ~ShortcutCreatorDialog() override;

    void done(int result) override;

private:
    void setupUi();
    void showShortcut();

    ShortcutEditorModel &mModel;
    QRadioButton *actionsRadioButton = nullptr;
    QComboBox *actionsComboBox = nullptr;
    QRadioButton *scriptsRadioButton = nullptr;
    QComboBox *scriptsComboBox = nullptr;
    KeySequenceEdit *sequenceEdit = nullptr;
    QLabel *warningLabel = nullptr;
    QDialogButtonBox *buttonBox = nullptr;
};
