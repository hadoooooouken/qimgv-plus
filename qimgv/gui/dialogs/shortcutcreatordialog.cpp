#include "shortcutcreatordialog.h"
#include "components/settingseditor/shortcuteditormodel.h"
#include "gui/customwidgets/keysequenceedit.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRadioButton>
#include <QComboBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>

ShortcutCreatorDialog::ShortcutCreatorDialog(ShortcutEditorModel &model, QWidget *parent) :
    QDialog(parent),
    mModel(model)
{
    setupUi();
    setWindowTitle(mModel.title());

    actionsComboBox->addItems(mModel.actions());
    actionsComboBox->setCurrentIndex(mModel.actionIndex());
    scriptsComboBox->addItems(mModel.scripts());
    scriptsComboBox->setCurrentIndex(mModel.scriptIndex());
    if(mModel.isScriptSelected())
        scriptsRadioButton->setChecked(true);
    else
        actionsRadioButton->setChecked(true);
    showShortcut();

    connect(scriptsRadioButton, &QRadioButton::toggled, &mModel, &ShortcutEditorModel::setScriptSelected);
    connect(actionsComboBox, &QComboBox::currentIndexChanged, &mModel, &ShortcutEditorModel::setActionIndex);
    connect(scriptsComboBox, &QComboBox::currentIndexChanged, &mModel, &ShortcutEditorModel::setScriptIndex);
    connect(sequenceEdit, &KeySequenceEdit::edited, this, [this]() {
        mModel.setShortcut(sequenceEdit->sequence());
    });
    connect(&mModel, &ShortcutEditorModel::shortcutChanged, this, &ShortcutCreatorDialog::showShortcut);
}

ShortcutCreatorDialog::~ShortcutCreatorDialog() = default;

void ShortcutCreatorDialog::done(int result) {
    if(result == QDialog::Accepted)
        mModel.accept();
    else
        mModel.reject();
    QDialog::done(result);
}

void ShortcutCreatorDialog::showShortcut() {
    sequenceEdit->setText(mModel.shortcutText());
    warningLabel->setText(mModel.warning());
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(mModel.canAccept());
}

void ShortcutCreatorDialog::setupUi()
{
    resize(340, 237);

    QVBoxLayout *verticalLayout = new QVBoxLayout(this);
    verticalLayout->setSpacing(6);

    QHBoxLayout *actionLayout = new QHBoxLayout();
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionsRadioButton = new QRadioButton(tr("Action:"), this);
    QSizePolicy spRadio(QSizePolicy::Minimum, QSizePolicy::Fixed);
    spRadio.setHorizontalStretch(1);
    actionsRadioButton->setSizePolicy(spRadio);
    actionLayout->addWidget(actionsRadioButton);

    actionsComboBox = new QComboBox(this);
    QSizePolicy spCombo(QSizePolicy::Preferred, QSizePolicy::Fixed);
    spCombo.setHorizontalStretch(2);
    actionsComboBox->setSizePolicy(spCombo);
    actionLayout->addWidget(actionsComboBox);
    verticalLayout->addLayout(actionLayout);

    QHBoxLayout *scriptLayout = new QHBoxLayout();
    scriptLayout->setContentsMargins(0, 0, 0, 0);
    scriptsRadioButton = new QRadioButton(tr("Script:"), this);
    scriptsRadioButton->setSizePolicy(spRadio);
    scriptLayout->addWidget(scriptsRadioButton);

    scriptsComboBox = new QComboBox(this);
    scriptsComboBox->setEnabled(false);
    scriptsComboBox->setSizePolicy(spCombo);
    scriptLayout->addWidget(scriptsComboBox);
    verticalLayout->addLayout(scriptLayout);

    verticalLayout->addSpacing(10); // replaces verticalSpacer_2

    QLabel *label_2 = new QLabel(tr("Shortcut:"), this);
    verticalLayout->addWidget(label_2, 0, Qt::AlignHCenter);

    sequenceEdit = new KeySequenceEdit(this);
    verticalLayout->addWidget(sequenceEdit);

    verticalLayout->addStretch(1); // replaces verticalSpacer

    warningLabel = new QLabel(this);
    warningLabel->setMinimumSize(0, 36);
    warningLabel->setWordWrap(true);
    verticalLayout->addWidget(warningLabel);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, this);
    verticalLayout->addWidget(buttonBox);

    // Connections
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(actionsRadioButton, &QRadioButton::toggled, actionsComboBox, &QComboBox::setEnabled);
    connect(actionsRadioButton, &QRadioButton::toggled, scriptsComboBox, &QComboBox::setDisabled);
    connect(scriptsRadioButton, &QRadioButton::toggled, actionsComboBox, &QComboBox::setDisabled);
    connect(scriptsRadioButton, &QRadioButton::toggled, scriptsComboBox, &QComboBox::setEnabled);
}
