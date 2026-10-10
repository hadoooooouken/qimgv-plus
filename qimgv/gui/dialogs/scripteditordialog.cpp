#include "scripteditordialog.h"
#include "components/settingseditor/scripteditormodel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QFileDialog>
#include <QLabel>
#include <QPushButton>
#include <QFrame>

ScriptEditorDialog::ScriptEditorDialog(ScriptEditorModel &model, QWidget *parent) :
    QDialog(parent),
    mModel(model)
{
    setupUi();
    setWindowTitle(mModel.title());
    nameLineEdit->setText(mModel.name());
    pathLineEdit->setText(mModel.command());
    blockingCheckBox->setChecked(mModel.isBlocking());
    onNameChanged();

    connect(nameLineEdit, &QLineEdit::textChanged, &mModel, &ScriptEditorModel::setName);
    connect(pathLineEdit, &QLineEdit::textChanged, &mModel, &ScriptEditorModel::setCommand);
    connect(blockingCheckBox, &QCheckBox::toggled, &mModel, &ScriptEditorModel::setBlocking);
    connect(&mModel, &ScriptEditorModel::nameChanged, this, &ScriptEditorDialog::onNameChanged);
    connect(&mModel, &ScriptEditorModel::commandChanged, this, [this]() {
        if(pathLineEdit->text() != mModel.command())
            pathLineEdit->setText(mModel.command());
    });
}

ScriptEditorDialog::~ScriptEditorDialog() = default;

void ScriptEditorDialog::done(int result) {
    if(result == QDialog::Accepted)
        mModel.accept();
    else
        mModel.reject();
    QDialog::done(result);
}

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
    keywordsLabel->setText(ScriptEditorModel::keywordsText());
    gridLayout->addWidget(keywordsLabel, 2, 0, 1, 3);

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

    acceptButton = new QPushButton(this);
    horizontalLayout_4->addWidget(acceptButton);

    cancelButton = new QPushButton(tr("Cancel"), this);
    horizontalLayout_4->addWidget(cancelButton);

    verticalLayout->addLayout(horizontalLayout_4);

    // Connections
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(acceptButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(fileSelectButton, &QPushButton::clicked, this, &ScriptEditorDialog::selectScriptPath);
}

void ScriptEditorDialog::onNameChanged() {
    messageLabel->setText(mModel.message());
    acceptButton->setText(mModel.acceptText());
    acceptButton->setEnabled(mModel.canAccept());
}

void ScriptEditorDialog::selectScriptPath() {
    const QString filter = ScriptEditorModel::executableFilters().join(QStringLiteral(";;"));
    const QString file = QFileDialog::getOpenFileName(this, ScriptEditorModel::executableDialogTitle(), "", filter);
    mModel.setExecutablePath(file);
}
