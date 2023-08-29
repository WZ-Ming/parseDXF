#ifndef SORTTHREAD_H
#define SORTTHREAD_H

#include<QString>
#include<QPointF>
#include<QGraphicsItem>
#include<math.h>
#include<qmath.h>

//排序存储结构体
struct shapeMsg
{
    QString graphicType;
    QVector<QPointF> pointVector;
    bool needAirMove = true;
    int shapePos;
    bool isTurnDir = false;
    bool itemHasColor = false;
    QGraphicsLineItem* arrowItem[4]={nullptr,nullptr,nullptr,nullptr};
    bool arrow01IsShowing = false;
    bool arrow23IsShowing = false;
};

//排序线程
class sortShapeTask : public QObject
{
    Q_OBJECT

public:
    sortShapeTask();
    const double doublePrecision=pow(10,-6);
    QSharedPointer<QVector<shapeMsg>> sortedShapeMsg;
    QVector<QVector<double>> allShapeVector;
    QVector<int>curLayShapesVector;

    enum shapeType{ point = 0, line, circle, arc, ellipse };
    enum sortType{ pointSort = 0, shapeSort, handSort }sorttype;
    int startShapePos = -1;
    void addHandSortMsg(shapeMsg &shapeMsgTmp, const QPointF &lastPoint);

private:
    double calMinDis(const QVector<double> &vector, const QPointF &lastPoint, shapeMsg &shapeMsgTmp);
    double sortAlgorithm(const QPointF &startPoint, QSharedPointer<QVector<shapeMsg> > sortedShapeMsgTmp);
    void handSelectSort();

public slots:
    void beginSortShape();

signals:
    void sortShapeDone();
    void updateProgress(const int, const bool);
};

#endif // SORTTHREAD_H
