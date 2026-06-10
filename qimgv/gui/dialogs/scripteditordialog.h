#pragma once

#include <QFileDialog>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include "components/scriptmanager/scriptmanager.h"
#include "utils/script.h"

class QLineEdit;
class QCheckBox;
class QLabel;
class QPushButton;

class ScriptEditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ScriptEditorDialog(QWidget *parent = nullptr);
    explicit ScriptEditorDialog(QString name, Script script, QWidget *parent = nullptr);
    ~ScriptEditorDialog();
    QString scriptName();
    Script script();

private slots:
    void onNameChanged(QString name);
    void selectScriptPath();

private:
    void setupUi();

    QLineEdit *nameLineEdit = nullptr;
    QLineEdit *pathLineEdit = nullptr;
    QLabel *label_3 = nullptr;
    QLabel *keywordsLabel = nullptr;
    QCheckBox *blockingCheckBox = nullptr;
    QLabel *messageLabel = nullptr;
    QPushButton *acceptButton = nullptr;
    QPushButton *cancelButton = nullptr;
    QPushButton *fileSelectButton = nullptr;

    bool editMode;
    QString editTarget;
};
