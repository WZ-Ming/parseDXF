#include "otherwidget.h"

QMutiCheckBox::QMutiCheckBox(QWidget *parent) : QComboBox(parent)
{
    listWidget = new QListWidget(this);
    lineEdit = new QLineEdit(this);
    this->setModel(listWidget->model());
    this->setView(listWidget);
    this->setLineEdit(lineEdit);
    lineEdit->setText("图层选择");
    lineEdit->setReadOnly(true);
}

QMutiCheckBox::~QMutiCheckBox()
{
    delete listWidget;
    delete lineEdit;
}

void QMutiCheckBox::appendItem(const QString &text, bool isChecked)
{
    QListWidgetItem *listItem = new QListWidgetItem(listWidget);
    listWidget->addItem(listItem);
    QCheckBox *checkBox = new QCheckBox(this);
    checkBox->setText(text);
    listWidget->setItemWidget(listItem, checkBox);
    connect(checkBox, SIGNAL(stateChanged(int)), this, SLOT(stateChangedSlot(int)));
    checkBox->setChecked(isChecked);
}

void QMutiCheckBox::clearItems()
{
    listWidget->clear();
}

void QMutiCheckBox::stateChangedSlot(int state)
{
    lineEdit->setText("图层选择");
    strSelectedData.clear();
    int nCount = listWidget->count();
    for (int i = 0; i < nCount; ++i){
        QListWidgetItem *pItem = listWidget->item(i);
        QWidget *pWidget = listWidget->itemWidget(pItem);
        if (!pWidget) continue;
        QCheckBox *pCheckBox = qobject_cast<QCheckBox *>(pWidget);
        if (pCheckBox->isChecked())
            strSelectedData.append(pCheckBox->text()).append(";");
    }
    emit checkedBoxTexts(strSelectedData);
}

void QMutiCheckBox::triggerSendSignal()
{
    emit checkedBoxTexts(strSelectedData);
}

progressShow::progressShow(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(QSize(200, 20));
    setWindowModality(Qt::WindowModal);
    setWindowFlag(Qt::Window);
    setWindowFlags(windowFlags() &~Qt::WindowMinimizeButtonHint &~Qt::WindowMaximizeButtonHint &~Qt::WindowCloseButtonHint);
    HLay = new QHBoxLayout;
    this->setLayout(HLay);
    progressBar = new QProgressBar;
    progressBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    HLay->addWidget(progressBar);
    progressBar->setAlignment(Qt::AlignHCenter);
    progressBar->setMaximum(100);
    progressBar->setValue(0);
    this->setWindowTitle("正在打开DXF...");
    this->show();
}

void progressShow::closeEvent(QCloseEvent* event)
{
    QMessageBox::information(this, "提示", "无法强制关闭!");
    event->ignore();
    return;
}

progressShow::~progressShow()
{
    delete HLay;
    delete progressBar;
}

void progressShow::initProgressBar(const int maxValue, const QString &title)
{
    progressBar->setMaximum(maxValue);
    this->setWindowTitle(title);
}

void progressShow::updateProgressValue(const int value)
{
    progressBar->setValue(value);
    QApplication::processEvents();
}
