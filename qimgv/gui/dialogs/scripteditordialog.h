#pragma once

#include <QDialog>

class QLineEdit;
class QCheckBox;
class QLabel;
class QPushButton;
class ScriptEditorModel;

// Widget view of the script editor (ScriptEditorModel): shows the model's
// open request; the answer goes to the model when the dialog finishes.
class ScriptEditorDialog : public QDialog
{
    Q_OBJECT

public:
    // model must outlive the dialog.
    explicit ScriptEditorDialog(ScriptEditorModel &model, QWidget *parent = nullptr);
    ~ScriptEditorDialog() override;

    void done(int result) override;

private slots:
    void onNameChanged();
    void selectScriptPath();

private:
    void setupUi();

    ScriptEditorModel &mModel;
    QLineEdit *nameLineEdit = nullptr;
    QLineEdit *pathLineEdit = nullptr;
    QLabel *keywordsLabel = nullptr;
    QCheckBox *blockingCheckBox = nullptr;
    QLabel *messageLabel = nullptr;
    QPushButton *acceptButton = nullptr;
    QPushButton *cancelButton = nullptr;
    QPushButton *fileSelectButton = nullptr;
};
