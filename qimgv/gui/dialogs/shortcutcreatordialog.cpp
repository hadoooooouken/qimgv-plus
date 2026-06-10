#include "shortcutcreatordialog.h"
#include "gui/customwidgets/keysequenceedit.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRadioButton>
#include <QComboBox>
#include <QLabel>
#include <QDialogButtonBox>

ShortcutCreatorDialog::ShortcutCreatorDialog(QWidget *parent) :
    QDialog(parent)
{
    setupUi();
    setWindowTitle(tr("Add shortcut"));
    actionList = appActions->getList();
    scriptList = scriptManager->scriptNames();

    actionsComboBox->addItems(actionList);
    actionsComboBox->setCurrentIndex(0);

    scriptsComboBox->addItems(scriptList);
    scriptsComboBox->setCurrentIndex(0);
}

ShortcutCreatorDialog::~ShortcutCreatorDialog() = default;

void ShortcutCreatorDialog::setupUi()
{
    setWindowTitle(tr("New shortcut"));
    resize(340, 237);

    QVBoxLayout *verticalLayout = new QVBoxLayout(this);
    verticalLayout->setSpacing(6);

    QHBoxLayout *actionLayout = new QHBoxLayout();
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionsRadioButton = new QRadioButton(tr("Action:"), this);
    actionsRadioButton->setChecked(true);
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
    sequenceEdit->setText(tr("[Enter shortcut]"));
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
    connect(sequenceEdit, &KeySequenceEdit::edited, this, &ShortcutCreatorDialog::onShortcutEdited);
}

QString ShortcutCreatorDialog::selectedAction() {
    if(actionsRadioButton->isChecked())
        return actionsComboBox->currentText();
    else
        return "s:"+scriptsComboBox->currentText();
}

QString ShortcutCreatorDialog::selectedShortcut() {
    return sequenceEdit->sequence();
}

void ShortcutCreatorDialog::onShortcutEdited() {
    QString action = actionManager->actionForShortcut(sequenceEdit->sequence());
    if(!action.isEmpty())
        warningLabel->setText(tr("This shortcut is used for action: ") + action + tr(". Replace?"));
    else
        warningLabel->setText("");
}

void ShortcutCreatorDialog::setAction(QString action) {
    auto cbox = actionsComboBox;
    if(action.startsWith("s:")) {
        action = action.remove(0,2);
        cbox = scriptsComboBox;
        scriptsRadioButton->setChecked(true);
    }
    int index = cbox->findText(action);
    if(index != -1)
       cbox->setCurrentIndex(index);
}

void ShortcutCreatorDialog::setShortcut(QString shortcut) {
    sequenceEdit->setText(shortcut);
}
