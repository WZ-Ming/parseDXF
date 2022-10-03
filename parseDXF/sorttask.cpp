#include "sorttask.h"

sortShapeTask::sortShapeTask():sortedShapeMsg(new QVector<shapeMsg>)
{

}

void sortShapeTask::beginSortShape()
{
    if (sorttype == pointSort){
        sortAlgorithm(QPointF(0.0, 0.0), sortedShapeMsg);
    }
    else if (sorttype == shapeSort){
        QVector<double>vector = allShapeVector[startShapePos];
        double x0=0, y0=0, x1=0, y1=0;
        double addMinDis1=0, addMinDis2=0;
        bool sortTwice = false;
        switch (static_cast<shapeType>(static_cast<int>(vector[0]))){
        case point:{
            x0 = vector[1];
            y0 = vector[2];
        }break;
        case circle:{
            x0 = vector[1];
            y0 = vector[2];
        }break;
        case line:{
            x0 = vector[1];
            y0 = vector[2];
            x1 = vector[3];
            y1 = vector[4];
            sortTwice = true;
        }break;
        case arc:{
            x0 = vector[1] + vector[3] * qCos(vector[4] * M_PI / 180);
            y0 = vector[2] + vector[3] * qSin(vector[4] * M_PI / 180);
            x1 = vector[1] + vector[3] * qCos(vector[5] * M_PI / 180);
            y1 = vector[2] + vector[3] * qSin(vector[5] * M_PI / 180);
            sortTwice = true;
        }break;
        case ellipse:{
            if (qFabs((vector[7] - vector[6]) * 180 / M_PI - 360) < doublePrecision){
                x0 = vector[1];
                y0 = vector[2];
            }
            else{
                double r = qSqrt(qPow(vector[3], 2) + qPow(vector[4], 2));
                double rotate_angle = qAtan2(vector[4], vector[3]);
                double x00 = vector[1] + r*qCos(vector[6]);
                double y00 = vector[2] + r*qSin(vector[6]);
                double x01 = vector[1] + r*qCos(vector[7]);
                double y01 = vector[2] + r*qSin(vector[7]);

                //double centerX = (x00 - vector[1])*qCos(M_PI_4) - (y00 - vector[2])*sin(M_PI_4) + vector[1];
                double centerY = (y00 - vector[2])*qCos(M_PI_4) + (x00 - vector[1])*qSin(M_PI_4) + vector[2];
                centerY = vector[5] * (centerY - vector[2]) + vector[2];

                y00 = vector[5] * (y00 - vector[2]) + vector[2];
                y01 = vector[5] * (y01 - vector[2]) + vector[2];
                x0 = (x00 - vector[1])*qCos(rotate_angle) - (y00 - vector[2])*qSin(rotate_angle) + vector[1];
                y0 = (y00 - vector[2])*qCos(rotate_angle) + (x00 - vector[1])*qSin(rotate_angle) + vector[2];
                x1 = (x01 - vector[1])*qCos(rotate_angle) - (y01 - vector[2])*qSin(rotate_angle) + vector[1];
                y1 = (y01 - vector[2])*qCos(rotate_angle) + (x01 - vector[1])*qSin(rotate_angle) + vector[2];
                sortTwice = true;
            }
        }break;}
        addMinDis1 = sortAlgorithm(QPointF(x0, y0), sortedShapeMsg);
        if (sortTwice){
            QSharedPointer<QVector<shapeMsg>> sortedShapeMsgTmp(new QVector<shapeMsg>());
            addMinDis2 = sortAlgorithm(QPointF(x1, y1), sortedShapeMsgTmp);
            if (addMinDis1 > addMinDis2)
                sortedShapeMsg = sortedShapeMsgTmp;
        }
    }
    else{
        handSelectSort();
    }
    emit sortShapeDone();
}

void sortShapeTask::handSelectSort()
{
    for (int i = 1; i < sortedShapeMsg->size(); i++)
        addHandSortMsg(*(sortedShapeMsg->begin()+i), sortedShapeMsg->at(i-1).endPoint[1]);
}

void sortShapeTask::addHandSortMsg(shapeMsg &shapeMsgTmp, const QPointF &lastPoint)
{
    for (int i = 0; i < 4; i++)
        shapeMsgTmp.arrowItem[i] = nullptr;
    shapeMsgTmp.itemHasColor = false;
    shapeMsgTmp.arrow01IsShowing = false;
    shapeMsgTmp.arrow23IsShowing = false;
    auto shapeType = static_cast<sortShapeTask::shapeType>(static_cast<int>(allShapeVector[shapeMsgTmp.shapePos][0]));
    QVector<double>vector = allShapeVector[shapeMsgTmp.shapePos];
    bool isTurnDir = shapeMsgTmp.isTurnDir;
    double minDis = 0, x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    switch (shapeType){
    case line:{
        if (!isTurnDir){
            minDis = qPow(vector[1] - lastPoint.x(), 2) + qPow(vector[2] - lastPoint.y(), 2);
            x0 = vector[1];
            y0 = vector[2];
            x1 = vector[3];
            y1 = vector[4];
        }
        else{
            minDis = qPow(vector[3] - lastPoint.x(), 2) + qPow(vector[4] - lastPoint.y(), 2);
            x0 = vector[3];
            y0 = vector[4];
            x1 = vector[1];
            y1 = vector[2];
        }
        shapeMsgTmp.endPoint[0]=QPointF(x0, y0);
        shapeMsgTmp.endPoint[1]=QPointF(x1, y1);
    }break;
    case point:{
        minDis = qPow(vector[1] - lastPoint.x(), 2) + qPow(vector[2] - lastPoint.y(), 2);
        x0 = vector[1];
        y0 = vector[2];
        shapeMsgTmp.endPoint[0]=QPointF(x0, y0);
        shapeMsgTmp.endPoint[1]=QPointF(x0, y0);
    }break;
    case circle:{
        if (qSqrt(qPow(lastPoint.x() - vector[1], 2) + qPow(lastPoint.y() - vector[2], 2)) < 0.1){
            x0 = (vector[1] + vector[3]);
            y0 = vector[2];
        }
        else{
            double t = vector[3] / qSqrt(qPow(lastPoint.x() - vector[1], 2) + qPow(lastPoint.y() - vector[2], 2));
            x0 = t*(lastPoint.x() - vector[1]) + vector[1];
            y0 = t*(lastPoint.y() - vector[2]) + vector[2];
        }
        x1 = x0;
        y1 = y0;
        minDis = qPow(x0 - lastPoint.x(), 2) + qPow(y0 - lastPoint.y(), 2);
        shapeMsgTmp.endPoint[0]=QPointF(x0, y0);
        shapeMsgTmp.endPoint[1]=QPointF(x0, y0);
    }break;
    case arc:{
        double angle_move = 0;
        if (vector[5] - vector[4] < 0)
            angle_move = vector[5] - vector[4] + 360;
        else
            angle_move = vector[5] - vector[4];
        if (!isTurnDir){
            minDis = qPow(vector[1] + vector[3] * qCos(vector[4] * M_PI / 180) - lastPoint.x(), 2) + qPow(vector[2] + vector[3] * qSin(vector[4] * M_PI / 180) - lastPoint.y(), 2);
            x0 = vector[1] + vector[3] * qCos(vector[4] * M_PI / 180);
            y0 = vector[2] + vector[3] * qSin(vector[4] * M_PI / 180);
            x1 = vector[1] + vector[3] * qCos(vector[5] * M_PI / 180);
            y1 = vector[2] + vector[3] * qSin(vector[5] * M_PI / 180);
        }
        else{
            minDis = qPow(vector[1] + vector[3] * qCos(vector[5] * M_PI / 180) - lastPoint.x(), 2) + qPow(vector[2] + vector[3] * qSin(vector[5] * M_PI / 180) - lastPoint.y(), 2);
            x0 = vector[1] + vector[3] * qCos(vector[5] * M_PI / 180);
            y0 = vector[2] + vector[3] * qSin(vector[5] * M_PI / 180);
            x1 = vector[1] + vector[3] * qCos(vector[4] * M_PI / 180);
            y1 = vector[2] + vector[3] * qSin(vector[4] * M_PI / 180);
        }
        shapeMsgTmp.endPoint[0]=QPointF(x0, y0);
        shapeMsgTmp.endPoint[1]=QPointF(x1, y1);
    }break;
    case ellipse:{
        if (qFabs((vector[7] - vector[6]) * 180 / M_PI - 360) < doublePrecision){
            x0 = vector[1] + vector[3];
            y0 = vector[2] + vector[4];
            x1 = vector[1] - vector[3];
            y1 = vector[2] - vector[4];
            double minDis0 = qPow(x0 - lastPoint.x(), 2) + qPow(y0 - lastPoint.y(), 2);
            double minDis1 = qPow(x1 - lastPoint.x(), 2) + qPow(y1 - lastPoint.y(), 2);
            if (minDis0 <= minDis1){
                minDis = minDis0;
            }
            else{
                minDis = minDis1;
                double xTmp = x0, yTmp = y0;
                x0 = x1;
                y0 = y1;
                x1 = xTmp;
                y1 = yTmp;
            }
            shapeMsgTmp.endPoint[0]=QPointF(x0, y0);
            shapeMsgTmp.endPoint[1]=QPointF(x0, y0);
        }
        else{
            double r = qSqrt(qPow(vector[3], 2) + qPow(vector[4], 2));
            double rotate_angle = qAtan2(vector[4], vector[3]);
            double x00 = vector[1] + r*qCos(vector[6]);
            double y00 = vector[2] + r*qSin(vector[6]);
            double x01 = vector[1] + r*qCos(vector[7]);
            double y01 = vector[2] + r*qSin(vector[7]);

            //double centerX = (x00 - vector[1])*qCos(M_PI_4) - (y00 - vector[2])*qSin(M_PI_4) + vector[1];
            double centerY = (y00 - vector[2])*qCos(M_PI_4) + (x00 - vector[1])*qSin(M_PI_4) + vector[2];
            centerY = vector[5] * (centerY - vector[2]) + vector[2];

            y00 = vector[5] * (y00 - vector[2]) + vector[2];
            y01 = vector[5] * (y01 - vector[2]) + vector[2];
            x0 = (x00 - vector[1])*qCos(rotate_angle) - (y00 - vector[2])*qSin(rotate_angle) + vector[1];
            y0 = (y00 - vector[2])*qCos(rotate_angle) + (x00 - vector[1])*qSin(rotate_angle) + vector[2];
            x1 = (x01 - vector[1])*qCos(rotate_angle) - (y01 - vector[2])*qSin(rotate_angle) + vector[1];
            y1 = (y01 - vector[2])*qCos(rotate_angle) + (x01 - vector[1])*qSin(rotate_angle) + vector[2];
            if (!isTurnDir)
                minDis = qPow(x0 - lastPoint.x(), 2) + qPow(y0 - lastPoint.y(), 2);
            else{
                minDis = qPow(x1 - lastPoint.x(), 2) + qPow(y1 - lastPoint.y(), 2);
                double xTmp = x1, yTmp = y1;
                x1 = x0;
                y1 = y0;
                x0 = xTmp;
                y0 = yTmp;
            }
            shapeMsgTmp.endPoint[0]=QPointF(x0, y0);
            shapeMsgTmp.endPoint[1]=QPointF(x1, y1);
        }
    }break;}
    minDis = qSqrt(minDis);
    if (minDis <= doublePrecision)
        shapeMsgTmp.needAirMove = false;
    else
        shapeMsgTmp.needAirMove = true;
}

double sortShapeTask::sortAlgorithm(const QPointF &startPoint, QSharedPointer<QVector<shapeMsg> > sortedShapeMsgTmp)
{
    QMap<int, QVector<double>> shapePosMap;
    double addMinDis = 0;
    foreach(int j, curLayShapesVector)
        shapePosMap.insert(j, allShapeVector[j]);
    shapeMsg beginPoint;
    beginPoint.endPoint[0]=beginPoint.endPoint[1]=startPoint;
    sortedShapeMsgTmp->append(beginPoint);
    if (sorttype == shapeSort){
        shapeMsg shapeMsgForce;
        calMinDis(shapePosMap.value(startShapePos), sortedShapeMsgTmp->last().endPoint[1], shapeMsgForce);
        shapeMsgForce.shapePos = startShapePos;
        sortedShapeMsgTmp->append(shapeMsgForce);
        shapePosMap.remove(startShapePos);
        emit updateProgress(sortedShapeMsgTmp->size() - 1, false);
    }

    while (shapePosMap.size() > 0){
        double minDis = 0;
        shapeMsg ShapeMsgTmpSave;
        bool firstTime = true;
        QVector<double> vectorTmp;
        foreach(QVector<double> vector, shapePosMap.values().toVector()){
            shapeMsg shapeMsgTmp;
            double minDisTmp = calMinDis(vector, sortedShapeMsgTmp->last().endPoint[1], shapeMsgTmp);
            if (firstTime){
                firstTime = false;
                minDis = minDisTmp;
                ShapeMsgTmpSave = shapeMsgTmp;
                vectorTmp = vector;
            }
            else{
                if (minDisTmp < minDis){
                    minDis = minDisTmp;
                    ShapeMsgTmpSave = shapeMsgTmp;
                    vectorTmp = vector;
                }
            }
        }
        ShapeMsgTmpSave.shapePos = shapePosMap.key(vectorTmp);
        addMinDis += minDis;
        sortedShapeMsgTmp->append(ShapeMsgTmpSave);
        shapePosMap.remove(shapePosMap.key(vectorTmp));
        emit updateProgress(sortedShapeMsgTmp->size() - 1, false);
    }
    sortedShapeMsgTmp->squeeze();
    return addMinDis;
}

double sortShapeTask::calMinDis(const QVector<double> &vector, const QPointF &lastPoint, shapeMsg &shapeMsgTmp)
{
    for (int i = 0; i < 4; i++)
        shapeMsgTmp.arrowItem[i] = nullptr;
    shapeMsgTmp.itemHasColor = false;
    shapeMsgTmp.arrow01IsShowing = false;
    shapeMsgTmp.arrow23IsShowing = false;
    bool isTurnDir = false;
    double minDis = 0, x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    switch (static_cast<shapeType>(static_cast<int>(vector[0]))){
    case line:{
        double minDis0 = qPow(vector[1] - lastPoint.x(), 2) + qPow(vector[2] - lastPoint.y(), 2);
        double minDis1 = qPow(vector[3] - lastPoint.x(), 2) + qPow(vector[4] - lastPoint.y(), 2);
        if (minDis0 <= minDis1){
            minDis = minDis0;
            x0 = vector[1];
            y0 = vector[2];
            x1 = vector[3];
            y1 = vector[4];
        }
        else{
            minDis = minDis1;
            x0 = vector[3];
            y0 = vector[4];
            x1 = vector[1];
            y1 = vector[2];
            isTurnDir = true;
        }
        shapeMsgTmp.endPoint[0]=QPointF(x0, y0);
        shapeMsgTmp.endPoint[1]=QPointF(x1, y1);
    }break;
    case point:{
        minDis = qPow(vector[1] - lastPoint.x(), 2) + qPow(vector[2] - lastPoint.y(), 2);
        x0 = vector[1];
        y0 = vector[2];
        shapeMsgTmp.endPoint[0]=QPointF(x0, y0);
        shapeMsgTmp.endPoint[1]=QPointF(x0, y0);
    }break;
    case circle:{
        if (qSqrt(qPow(lastPoint.x() - vector[1], 2) + qPow(lastPoint.y() - vector[2], 2)) < 0.1){
            x0 = (vector[1] + vector[3]);
            y0 = vector[2];
        }
        else{
            double t = vector[3] / qSqrt(qPow(lastPoint.x() - vector[1], 2) + qPow(lastPoint.y() - vector[2], 2));
            x0 = t*(lastPoint.x() - vector[1]) + vector[1];
            y0 = t*(lastPoint.y() - vector[2]) + vector[2];
        }
        x1 = x0;
        y1 = y0;
        minDis = qPow(x0 - lastPoint.x(), 2) + qPow(y0 - lastPoint.y(), 2);
        shapeMsgTmp.endPoint[0]=QPointF(x0, y0);
        shapeMsgTmp.endPoint[1]=QPointF(x0, y0);
    }break;
    case arc:{
        double angle_move = 0;
        if (vector[5] - vector[4] < 0)
            angle_move = vector[5] - vector[4] + 360;
        else
            angle_move = vector[5] - vector[4];
        double minDis0 = qPow(vector[1] + vector[3] * qCos(vector[4] * M_PI / 180) - lastPoint.x(), 2) + qPow(vector[2] + vector[3] * qSin(vector[4] * M_PI / 180) - lastPoint.y(), 2);
        double minDis1 = qPow(vector[1] + vector[3] * qCos(vector[5] * M_PI / 180) - lastPoint.x(), 2) + qPow(vector[2] + vector[3] * qSin(vector[5] * M_PI / 180) - lastPoint.y(), 2);
        QPointF pointTmp;
        if (minDis0 <= minDis1){
            minDis = minDis0;
            x0 = vector[1] + vector[3] * qCos(vector[4] * M_PI / 180);
            y0 = vector[2] + vector[3] * qSin(vector[4] * M_PI / 180);
            x1 = vector[1] + vector[3] * qCos(vector[5] * M_PI / 180);
            y1 = vector[2] + vector[3] * qSin(vector[5] * M_PI / 180);

            pointTmp.setX((x0 - vector[1])*qCos(angle_move / 2 * M_PI / 180) - (y0 - vector[2])*qSin(angle_move / 2 * M_PI / 180) + vector[1]);
            pointTmp.setY((y0 - vector[2])*qCos(angle_move / 2 * M_PI / 180) + (x0 - vector[1])*qSin(angle_move / 2 * M_PI / 180) + vector[2]);
        }
        else{
            minDis = minDis1;
            x0 = vector[1] + vector[3] * qCos(vector[5] * M_PI / 180);
            y0 = vector[2] + vector[3] * qSin(vector[5] * M_PI / 180);
            x1 = vector[1] + vector[3] * qCos(vector[4] * M_PI / 180);
            y1 = vector[2] + vector[3] * qSin(vector[4] * M_PI / 180);

            pointTmp.setX((x0 - vector[1])*qCos(-angle_move / 2 * M_PI / 180) - (y0 - vector[2])*qSin(-angle_move / 2 * M_PI / 180) + vector[1]);
            pointTmp.setY((y0 - vector[2])*qCos(-angle_move / 2 * M_PI / 180) + (x0 - vector[1])*qSin(-angle_move / 2 * M_PI / 180) + vector[2]);
            isTurnDir = true;
        }
        shapeMsgTmp.endPoint[0]=QPointF(x0, y0);
        shapeMsgTmp.endPoint[1]=QPointF(x1, y1);
    }break;
    case ellipse:{
        if (qFabs((vector[7] - vector[6]) * 180 / M_PI - 360) < doublePrecision){
            x0 = vector[1] + vector[3];
            y0 = vector[2] + vector[4];
            x1 = vector[1] - vector[3];
            y1 = vector[2] - vector[4];
            double minDis0 = qPow(x0 - lastPoint.x(), 2) + qPow(y0 - lastPoint.y(), 2);
            double minDis1 = qPow(x1 - lastPoint.x(), 2) + qPow(y1 - lastPoint.y(), 2);
            if (minDis0 <= minDis1){
                minDis = minDis0;
            }
            else{
                minDis = minDis1;
                double xTmp = x0, yTmp = y0;
                x0 = x1;
                y0 = y1;
                x1 = xTmp;
                y1 = yTmp;
            }
            shapeMsgTmp.endPoint[0]=QPointF(x0, y0);
            shapeMsgTmp.endPoint[1]=QPointF(x0, y0);
        }
        else{
            double r = qSqrt(qPow(vector[3], 2) + qPow(vector[4], 2));
            double rotate_angle = qAtan2(vector[4], vector[3]);
            double x00 = vector[1] + r*qCos(vector[6]);
            double y00 = vector[2] + r*qSin(vector[6]);
            double x01 = vector[1] + r*qCos(vector[7]);
            double y01 = vector[2] + r*qSin(vector[7]);

            //double centerX = (x00 - vector[1])*qCos(M_PI_4) - (y00 - vector[2])*qSin(M_PI_4) + vector[1];
            double centerY = (y00 - vector[2])*qCos(M_PI_4) + (x00 - vector[1])*qSin(M_PI_4) + vector[2];
            centerY = vector[5] * (centerY - vector[2]) + vector[2];

            y00 = vector[5] * (y00 - vector[2]) + vector[2];
            y01 = vector[5] * (y01 - vector[2]) + vector[2];
            x0 = (x00 - vector[1])*qCos(rotate_angle) - (y00 - vector[2])*qSin(rotate_angle) + vector[1];
            y0 = (y00 - vector[2])*qCos(rotate_angle) + (x00 - vector[1])*qSin(rotate_angle) + vector[2];
            x1 = (x01 - vector[1])*qCos(rotate_angle) - (y01 - vector[2])*qSin(rotate_angle) + vector[1];
            y1 = (y01 - vector[2])*qCos(rotate_angle) + (x01 - vector[1])*qSin(rotate_angle) + vector[2];
            double minDis0 = qPow(x0 - lastPoint.x(), 2) + qPow(y0 - lastPoint.y(), 2);
            double minDis1 = qPow(x1 - lastPoint.x(), 2) + qPow(y1 - lastPoint.y(), 2);

            if (minDis0 <= minDis1){
                minDis = minDis0;
            }
            else{
                minDis = minDis1;
                double xTmp = x1, yTmp = y1;
                x1 = x0;
                y1 = y0;
                x0 = xTmp;
                y0 = yTmp;
                isTurnDir = true;
            }
            shapeMsgTmp.endPoint[0]=QPointF(x0, y0);
            shapeMsgTmp.endPoint[1]=QPointF(x1, y1);
        }
    }break;}
    minDis = qSqrt(minDis);
    if (minDis <= doublePrecision)
        shapeMsgTmp.needAirMove = false;
    else
        shapeMsgTmp.needAirMove = true;
    shapeMsgTmp.isTurnDir = isTurnDir;
    return minDis;
}
