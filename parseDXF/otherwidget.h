#ifndef OTHERWIDGET_H
#define OTHERWIDGET_H

#include<QComboBox>
#include<QListWidget>
#include<QHBoxLayout>
#include<QProgressBar>
#include<QLineEdit>
#include<QCheckBox>
#include<QMessageBox>
#include<QCloseEvent>
#include<QApplication>

//图层选择
class QMutiCheckBox : public QComboBox
{
    Q_OBJECT
public:
    QMutiCheckBox(QWidget *parent = nullptr);
    ~QMutiCheckBox();
    void appendItem(const QString &text, bool isChecked = false);
    void clearItems();
    void triggerSendSignal();

signals:
    void checkedBoxTexts(QString &);

private:
    QListWidget *listWidget;
    QLineEdit *lineEdit;
    QString strSelectedData;

private slots:
    void stateChangedSlot(int state);
};

//进度条
class progressShow : public QWidget
{
    Q_OBJECT

public:
    progressShow(QWidget *parent = Q_NULLPTR);
    ~progressShow();
    void updateProgressValue(const int value);
    void initProgressBar(const int maxValue, const QString &title);

protected:
    void closeEvent(QCloseEvent* event);
private:
    QHBoxLayout *HLay = nullptr;
    QProgressBar *progressBar = nullptr;

};

#endif // OTHERWIDGET_H
