#ifndef PARSEDXF_H
#define PARSEDXF_H

#include <QMainWindow>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QWheelEvent>
#include <QMouseEvent>
#include <qmath.h>
#include <qfile.h>
#include <qtextstream.h>
#include <QVector>
#include <qmap.h>
#include <QDebug>
#include <QFileDialog>
#include <QGraphicsPathItem>
#include <QtGlobal>
#include <QThread>
#include <QListWidgetItem>
#include <QMenu>
#include "sorttask.h"
#include "otherwidget.h"

#ifdef Q_OS_WIN
    #pragma execution_character_set("utf-8")
#endif

namespace Ui {
class parseDXF;
}

class parseDXF : public QMainWindow
{
    Q_OBJECT

public:
    explicit parseDXF(QWidget *parent = nullptr);
    ~parseDXF();

protected:
    void wheelEvent(QWheelEvent* event);
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void mouseDoubleClickEvent(QMouseEvent *event);

private slots:
    void openDXF();
    void drawSortPath();

    void on_listWidget_clicked(const QModelIndex &index);
    void on_listWidget_customContextMenuRequested(const QPoint &pos);
    void turnDirShape();
    void clearListWidget();
    void deleteShape();
    void moveUpShape();
    void moveDownShape();

    void refreshView();
    void showSortPath();

public slots:
    void updateProgress(const int value, const bool iscompleted);

private:
    double ucsX, ucsY;

    double point_dis;
    double axisLength;

    bool drag_flag = false;
    bool canOutput = false;

    void initAxisItem();
    void initComBox();
    void draw_scene();
    void drawCenterAxis();
    void drawArrow(const int sortShapePos);
    void ParseInsert(QSharedPointer<QVector<QMap<int, QString> > > insertVectorMap , QSharedPointer<QVector<QMap<int, QString> > > blockVectorMap);
    void changeItemColor(QGraphicsItem *item, const uchar itemColor);
    void listWidgetAddShape(const int shapePos);
    void sortShape(const sortShapeTask::sortType type);
    void deleteExtraItem(bool clearSortedShapeMsg=true);

private:
    Ui::parseDXF *ui;
    progressShow *progressshow = nullptr;
    QGraphicsScene *scene = nullptr;
    QGraphicsLineItem *axisXLineItem = nullptr;
    QGraphicsLineItem *axisYLineItem = nullptr;
    QThread *sortThread = nullptr;
    sortShapeTask *sortTask = nullptr;
    QMutiCheckBox *layShapeCombo = nullptr;
    QPen pen;

    QMultiMap<QString, int> layToShapeMutiMap;
    QVector<QGraphicsLineItem*> lineItemsVector;
    QMap<int, QGraphicsItem*> findScenItemMap;
    QVector<int> listClickedVector;

    const double Cpoint_dis = 5, CaxisLength = 50;

};

#endif // PARSEDXF_H
