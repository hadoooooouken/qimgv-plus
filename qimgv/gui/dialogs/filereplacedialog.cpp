#include "filereplacedialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>

FileReplaceDialog::FileReplaceDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
    multi = false;
    connect(yesButton, &QPushButton::clicked, this, &FileReplaceDialog::onYesClicked);
    connect(noButton, &QPushButton::clicked, this, &FileReplaceDialog::onNoClicked);
    connect(cancelButton, &QPushButton::clicked, this, &FileReplaceDialog::onCancelClicked);
}

FileReplaceDialog::~FileReplaceDialog() = default;

void FileReplaceDialog::setupUi()
{
    setWindowTitle(tr("Dialog"));
    resize(380, 169);
    setMinimumWidth(380);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    titleLabel = new QLabel(tr("title"), this);
    mainLayout->addWidget(titleLabel);

    QLabel *srcTitleLabel = new QLabel(tr("Source:"), this);
    QFont boldFont;
    boldFont.setBold(true);
    srcTitleLabel->setFont(boldFont);
    mainLayout->addWidget(srcTitleLabel);

    srcLabel = new QLabel(tr("src"), this);
    srcLabel->setWordWrap(false);
    mainLayout->addWidget(srcLabel);

    QLabel *arrowLabel = new QLabel(tr(">>>"), this);
    mainLayout->addWidget(arrowLabel);

    QLabel *dstTitleLabel = new QLabel(tr("Destination:"), this);
    dstTitleLabel->setFont(boldFont);
    mainLayout->addWidget(dstTitleLabel);

    dstLabel = new QLabel(tr("dst"), this);
    dstLabel->setWordWrap(true);
    mainLayout->addWidget(dstLabel);

    mainLayout->addStretch(1);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(6);

    buttonLayout->addStretch(1);

    applyAllCheckBox = new QCheckBox(tr("Apply to all"), this);
    buttonLayout->addWidget(applyAllCheckBox);

    yesButton = new QPushButton(tr("Yes"), this);
    buttonLayout->addWidget(yesButton);

    noButton = new QPushButton(tr("No"), this);
    buttonLayout->addWidget(noButton);

    cancelButton = new QPushButton(tr("Cancel"), this);
    buttonLayout->addWidget(cancelButton);

    mainLayout->addLayout(buttonLayout);
}

void FileReplaceDialog::setSource(QString src) {
    srcLabel->setText(src);
}

void FileReplaceDialog::setDestination(QString dst) {
    dstLabel->setText(dst);
}

void FileReplaceDialog::setMode(FileReplaceMode mode) {
    if (mode == FILE_TO_FILE) {
        setWindowTitle(tr("File already exists"));
        titleLabel->setText(tr("Replace destination file?"));
    } else if (mode == DIR_TO_DIR) {
        setWindowTitle(tr("Directory already exists"));
        titleLabel->setText(tr("Merge directories?"));
    } else if (mode == DIR_TO_FILE) {
        setWindowTitle(tr("Destination already exists"));
        titleLabel->setText(tr("There is a file with that name. Replace?"));
    } else { // FILE_TO_DIR
        setWindowTitle(tr("Destination already exists"));
        titleLabel->setText(tr("There is a folder with that name. Replace?"));
    }
}

void FileReplaceDialog::setMulti(bool _multi) {
    multi = _multi;
    applyAllCheckBox->setVisible(multi);
}

DialogResult FileReplaceDialog::getResult() {
    return result;
}

void FileReplaceDialog::onYesClicked() {
    result.yes = true;
    result.all = applyAllCheckBox->isChecked();
    result.cancel = false;
    this->close();
}

void FileReplaceDialog::onNoClicked() {
    result.yes = false;
    result.all = applyAllCheckBox->isChecked();
    result.cancel = false;
    this->close();
}

void FileReplaceDialog::onCancelClicked() {
    result.cancel = true;
    this->close();
}
