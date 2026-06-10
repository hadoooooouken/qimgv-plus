#include "scripteditordialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QFrame>

ScriptEditorDialog::ScriptEditorDialog(QWidget *parent) :
    QDialog(parent),
    editMode(false)
{
    setupUi();
    this->setWindowTitle(tr("New application/script"));
    keywordsLabel->setText(tr("Keywords:") + " %file%");
#if defined(_WIN32) || defined(Q_OS_WIN) || defined(Q_OS_WIN32)
    label_3->hide();
#endif
    connect(nameLineEdit, &QLineEdit::textChanged, this, &ScriptEditorDialog::onNameChanged);
    this->onNameChanged(nameLineEdit->text());
}

ScriptEditorDialog::ScriptEditorDialog(QString name, Script script, QWidget *parent)
    : QDialog(parent),
      editMode(true)
{
    setupUi();
    this->setWindowTitle(tr("Edit"));
    editTarget = name;
#if defined(_WIN32) || defined(Q_OS_WIN) || defined(Q_OS_WIN32)
    label_3->hide();
#endif
    connect(nameLineEdit, &QLineEdit::textChanged, this, &ScriptEditorDialog::onNameChanged);
    nameLineEdit->setText(name);
    pathLineEdit->setText(script.command);
    blockingCheckBox->setChecked(script.blocking);
    this->onNameChanged(nameLineEdit->text());
}

ScriptEditorDialog::~ScriptEditorDialog() = default;

void ScriptEditorDialog::setupUi()
{
    resize(470, 200);

    QVBoxLayout *verticalLayout = new QVBoxLayout(this);
    verticalLayout->setSpacing(6);

    QGridLayout *gridLayout = new QGridLayout();

    QLabel *label = new QLabel(tr("Name:"), this);
    gridLayout->addWidget(label, 0, 0);

    QHBoxLayout *horizontalLayout = new QHBoxLayout();
    horizontalLayout->setContentsMargins(0, 0, 0, 0);
    horizontalLayout->addSpacing(20); // replaces horizontalSpacer
    nameLineEdit = new QLineEdit(this);
    horizontalLayout->addWidget(nameLineEdit);
    gridLayout->addLayout(horizontalLayout, 0, 1);

    QLabel *label_2 = new QLabel(tr("Command:"), this);
    gridLayout->addWidget(label_2, 1, 0);

    QHBoxLayout *horizontalLayout_2 = new QHBoxLayout();
    horizontalLayout_2->setContentsMargins(0, 0, 0, 0);
    horizontalLayout_2->addSpacing(20); // replaces horizontalSpacer_2
    pathLineEdit = new QLineEdit(this);
    horizontalLayout_2->addWidget(pathLineEdit);
    gridLayout->addLayout(horizontalLayout_2, 1, 1);

    fileSelectButton = new QPushButton(this);
    fileSelectButton->setMaximumWidth(60);
    fileSelectButton->setText("...");
    gridLayout->addWidget(fileSelectButton, 1, 2);

    keywordsLabel = new QLabel(this);
    QFont smallFont;
    smallFont.setPointSize(10);
    keywordsLabel->setFont(smallFont);
    keywordsLabel->setMargin(4);
    keywordsLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::TextSelectableByMouse);
    gridLayout->addWidget(keywordsLabel, 2, 0, 1, 3);

    label_3 = new QLabel(tr("NOTE: make sure your .sh script has execute flag."), this);
    QFont italicFont = smallFont;
    italicFont.setItalic(true);
    label_3->setFont(italicFont);
    gridLayout->addWidget(label_3, 3, 0, 1, 3);

    verticalLayout->addLayout(gridLayout);

    QHBoxLayout *horizontalLayout_3 = new QHBoxLayout();
    horizontalLayout_3->setContentsMargins(0, 0, 0, 0);
    blockingCheckBox = new QCheckBox(tr("Wait to finish"), this);
    horizontalLayout_3->addWidget(blockingCheckBox);
    horizontalLayout_3->addStretch(1);
    verticalLayout->addLayout(horizontalLayout_3);

    verticalLayout->addSpacing(6);

    messageLabel = new QLabel(this);
    verticalLayout->addWidget(messageLabel);

    QHBoxLayout *horizontalLayout_4 = new QHBoxLayout();
    horizontalLayout_4->setContentsMargins(0, 0, 0, 0);
    horizontalLayout_4->addStretch(1);

    acceptButton = new QPushButton(tr("Create"), this);
    horizontalLayout_4->addWidget(acceptButton);

    cancelButton = new QPushButton(tr("Cancel"), this);
    horizontalLayout_4->addWidget(cancelButton);

    verticalLayout->addLayout(horizontalLayout_4);

    // Connections
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(acceptButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(fileSelectButton, &QPushButton::clicked, this, &ScriptEditorDialog::selectScriptPath);
}

QString ScriptEditorDialog::scriptName() {
    return nameLineEdit->text();
}

Script ScriptEditorDialog::script() {
    return Script(pathLineEdit->text(), blockingCheckBox->isChecked());
}

void ScriptEditorDialog::onNameChanged(QString name) {
    if(name.isEmpty()) {
        messageLabel->setText(tr("Enter script name"));
        acceptButton->setEnabled(false);
        return;
    } else {
        acceptButton->setEnabled(true);
    }

    QString okBtnText;
    messageLabel->clear();

    if(editMode) {
        if(name != editTarget && scriptManager->scriptExists(name)) {
            messageLabel->setText(tr("A script with this same name exists"));
            okBtnText = tr("Replace");
        } else {
            okBtnText = tr("Save");
        }
    } else {
        if(scriptManager->scriptExists(name)) {
            messageLabel->setText(tr("A script with this same name exists"));
            okBtnText = tr("Replace");
        } else {
            okBtnText = tr("Create");
        }
    }
    acceptButton->setText(okBtnText);
}

void ScriptEditorDialog::selectScriptPath() {
    QFileDialog dialog;
    QString file;
#ifdef _WIN32
    file = dialog.getOpenFileName(this, tr("Select an executable/script"), "", "Executable/script (*.exe *.bat)");
#else
    file = dialog.getOpenFileName(this, tr("Select a script file"), "", "Shell script (*.sh)");
#endif
    if(!file.isEmpty()) {
        pathLineEdit->setText("\"" + file + "\"" + " %file%");
    }
}
