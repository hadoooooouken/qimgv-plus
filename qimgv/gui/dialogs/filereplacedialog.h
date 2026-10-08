#pragma once
#include <QDialog>

#include "gui/ports/dialogport.h"

class QLabel;
class QCheckBox;
class QPushButton;

class FileReplaceDialog : public QDialog {
    Q_OBJECT

public:
    explicit FileReplaceDialog(QWidget *parent = nullptr);
    ~FileReplaceDialog();

    void setMode(FileReplaceMode mode);
    void setMulti(bool);
    FileReplaceDecision getResult();

    void setSource(QString src);
    void setDestination(QString dst);
private slots:
    void onYesClicked();
    void onNoClicked();
    void onCancelClicked();

private:
    void setupUi();

    QLabel *titleLabel = nullptr;
    QLabel *srcLabel = nullptr;
    QLabel *dstLabel = nullptr;
    QCheckBox *applyAllCheckBox = nullptr;
    QPushButton *yesButton = nullptr;
    QPushButton *noButton = nullptr;
    QPushButton *cancelButton = nullptr;

    bool multi;
    FileReplaceDecision result;
};
