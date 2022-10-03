#include "parsedxf.h"
#include "ui_parsedxf.h"

parseDXF::parseDXF(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::parseDXF)
{
    ui->setupUi(this);
    setMouseTracking(true);
    ui->graphicsView->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    ui->graphicsView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->graphicsView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    double view_x = ui->graphicsView->width();
    double view_y = ui->graphicsView->height();
    scene = new QGraphicsScene;
    scene->setSceneRect(-0.5*view_x, -0.5*view_y, view_x, view_y);
    scene->setBackgroundBrush(Qt::black);
    axisLength = CaxisLength;
    point_dis = Cpoint_dis;
    pen.setColor(Qt::white);
    pen.setWidth(0);
    initAxisItem();
    drawCenterAxis();
    ui->graphicsView->setScene(scene);
    layShapeCombo = new QMutiCheckBox(this);
    layShapeCombo->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    layShapeCombo->setMinimumHeight(30);
    layShapeCombo->setMaximumWidth(100);
    QFont font = layShapeCombo->font();
    font.setPointSize(12);
    layShapeCombo->setFont(font);
    ui->verticalLayout->insertWidget(1, layShapeCombo);
    ui->listWidget->setContextMenuPolicy(Qt::CustomContextMenu);

    sortThread = new QThread(this);
    sortTask = new sortShapeTask;
    sortTask->moveToThread(sortThread);
    connect(sortThread, &QThread::started, sortTask, &sortShapeTask::beginSortShape);
    connect(sortTask, &sortShapeTask::sortShapeDone, this, &parseDXF::drawSortPath);
    connect(ui->btn_openDXF, &QPushButton::clicked, this, &parseDXF::openDXF);
    connect(sortTask, &sortShapeTask::updateProgress, this, &parseDXF::updateProgress);
    connect(ui->btn_moveUp, &QPushButton::clicked, this, &parseDXF::moveUpShape);
    connect(ui->btn_moveDown, &QPushButton::clicked, this, &parseDXF::moveDownShape);
}

parseDXF::~parseDXF()
{
    deleteExtraItem();
    scene->clear();
    delete scene;
    delete sortTask;
    delete layShapeCombo;
    if (sortThread->isRunning()){
        sortThread->exit();
        sortThread->wait();
    }
    delete sortThread;
    delete ui;
}

void parseDXF::initComBox()
{
    if (sortTask->allShapeVector.size() == 0) return;
    layShapeCombo->clearItems();
    layShapeCombo->disconnect();
    QList<QString> allMapKey = layToShapeMutiMap.uniqueKeys();
    if (allMapKey.size() == 0) return;
    for (int i = 0; i < allMapKey.size(); i++)
        layShapeCombo->appendItem(allMapKey[i], true);

    connect(layShapeCombo, &QMutiCheckBox::checkedBoxTexts, [this](QString &texts){
        QStringList lay = texts.split(";", QString::SkipEmptyParts);
        sortTask->curLayShapesVector.clear();
        foreach(QString str, lay)
            sortTask->curLayShapesVector.append(layToShapeMutiMap.values(str).toVector());
        draw_scene();
    });
    layShapeCombo->triggerSendSignal();
}

void parseDXF::wheelEvent(QWheelEvent* event)
{
    QPointF last_point = ui->graphicsView->mapToScene(event->pos());
    if (event->delta() > 0)
        ui->graphicsView->scale(1.1, 1.1);
    else
        ui->graphicsView->scale(0.9, 0.9);
    point_dis = Cpoint_dis / ui->graphicsView->matrix().m11();
    axisLength = CaxisLength / ui->graphicsView->matrix().m11();

    scene->setSceneRect(ui->graphicsView->mapToScene(ui->graphicsView->rect()).boundingRect());
    QPointF next_point = ui->graphicsView->mapToScene(event->pos());
    QPointF move_point = next_point - last_point;
    scene->setSceneRect(scene->sceneRect().x() - move_point.x(), scene->sceneRect().y() - move_point.y(), scene->sceneRect().width(), scene->sceneRect().height());
    drawCenterAxis();
}

void parseDXF::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton){
        drag_flag = true;
        setCursor(Qt::OpenHandCursor);
    }
    else if (event->button() == Qt::RightButton){
        QMenu menu;
        QMenu subMenu;
        QAction act_refresh("刷新", this);
        QAction act_drawPath("路径", this);
        menu.addAction(&act_refresh);
        menu.addAction(&act_drawPath);
        menu.addMenu(&subMenu);
        subMenu.setTitle("排序");
        QAction act_pointSort("起始原点排序", this);
        QAction act_shapeSort("起始图形排序", this);
        QAction act_handSort("人工选择排序", this);
        subMenu.addAction(&act_pointSort);
        subMenu.addAction(&act_shapeSort);
        subMenu.addAction(&act_handSort);
        connect(&act_refresh, &QAction::triggered, this, &parseDXF::refreshView);
        connect(&act_drawPath, &QAction::triggered, this, &parseDXF::showSortPath);
        connect(&act_pointSort, &QAction::triggered, [this](){
            sortShape(sortTask->pointSort);
        });
        connect(&act_shapeSort, &QAction::triggered, [this](){
            sortShape(sortTask->shapeSort);
        });
        connect(&act_handSort, &QAction::triggered, [this](){
            sortShape(sortTask->handSort);
        });
        menu.exec(QCursor::pos());
    }
}

void parseDXF::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton){
        drag_flag = false;
        setCursor(Qt::ArrowCursor);
    }
}

void parseDXF::mouseMoveEvent(QMouseEvent *event)
{
    static QPoint lastPoint,disPoint;
    static QPointF pointF;
    if (drag_flag){
        disPoint = event->pos() - lastPoint;
        lastPoint=event->pos();
        scene->setSceneRect(scene->sceneRect().x()-disPoint.x()/ui->graphicsView->matrix().m11(),scene->sceneRect().y()-disPoint.y()/ui->graphicsView->matrix().m11(),
                scene->sceneRect().width(), scene->sceneRect().height());
    }
    else{
        lastPoint=event->pos();
        pointF = ui->graphicsView->mapToScene(lastPoint);
        setWindowTitle("坐标:" + QString::number(pointF.x(), 10, 3) + " , " + QString::number(-pointF.y(), 10, 3));
    }
}

void parseDXF::mouseDoubleClickEvent(QMouseEvent *event)
{
    QPointF pointScene = ui->graphicsView->mapToScene(event->pos());
    QGraphicsItem *item = nullptr;
    double pos_x = pointScene.x();
    double pos_y = -pointScene.y();
    bool selectedItemDone = false;
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    foreach(int i, sortTask->curLayShapesVector){
        auto shapeType = static_cast<sortShapeTask::shapeType>(static_cast<int>(sortTask->allShapeVector[i][0]));
        if (shapeType == sortTask->line){
            x0 = sortTask->allShapeVector[i][1];
            y0 = sortTask->allShapeVector[i][2];
            x1 = sortTask->allShapeVector[i][3];
            y1 = sortTask->allShapeVector[i][4];
            if(qFabs(x0-x1)>sortTask->doublePrecision && qFabs(y0-y1)>= sortTask->doublePrecision){
                if ((qFabs((y1 - y0)*pos_x + (x0 - x1)*pos_y + (x1 - x0)*y0 - (y1 - y0)*x0) / qSqrt(qPow((y1 - y0), 2) + qPow((x0 - x1), 2))) <= point_dis){
                    if (qFabs(pos_x - x0) <= qFabs(x1 - x0) && qFabs(pos_x - x1) <= qFabs(x0 - x1) && qFabs(pos_y - y0) <= qFabs(y1 - y0) && qFabs(pos_y - y1) <= qFabs(y0 - y1))
                        selectedItemDone = true;
                }
            }
            else if(qFabs(x0-x1)<sortTask->doublePrecision && qFabs(y0-y1)>= sortTask->doublePrecision){
                if (qFabs(pos_x - x0) <= point_dis && qFabs(pos_y - y0) <= qFabs(y1 - y0) && qFabs(pos_y - y1) <= qFabs(y0 - y1))
                    selectedItemDone = true;
            }
            else if(qFabs(x0-x1)>=sortTask->doublePrecision && qFabs(y0-y1)<sortTask->doublePrecision){
                if (qFabs(pos_y - y0) <= point_dis && qFabs(pos_x - x0) <= qFabs(x1 - x0) && qFabs(pos_x - x1) <= qFabs(x0 - x1))
                    selectedItemDone = true;
            }
        }
        else if (shapeType == sortTask->point){
            x0 = sortTask->allShapeVector[i][1];
            y0 = sortTask->allShapeVector[i][2];
            if (qFabs(qSqrt(qPow((pos_x - x0), 2) + qPow((pos_y - y0), 2))) <= point_dis)
                selectedItemDone = true;
        }
        if (selectedItemDone){
            listWidgetAddShape(i);
            return;
        }
    }
    item = scene->itemAt(pointScene, ui->graphicsView->transform());
    if (item != nullptr){
        listWidgetAddShape(findScenItemMap.key(item));
    }
    canOutput = false;
}

void parseDXF::changeItemColor(QGraphicsItem *item, const uchar itemColor)
{
    if (itemColor == 1)
        pen.setColor(Qt::GlobalColor::magenta);
    else
        pen.setColor(Qt::white);
    switch (item->type()){
    case QGraphicsLineItem::Type:{
        QGraphicsLineItem *lineItem;
        lineItem = qgraphicsitem_cast<QGraphicsLineItem*>(item);
        lineItem->setPen(pen);
    }break;
    case QGraphicsPathItem::Type:{
        QGraphicsPathItem *pathItem;
        pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(item);
        pathItem->setPen(pen);
    }break;
    case QGraphicsEllipseItem::Type:{
        QGraphicsEllipseItem *ellipseItem;
        ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(item);
        ellipseItem->setPen(pen);
    }break;
    default:break;
    }
    pen.setColor(Qt::white);
}

void parseDXF::openDXF()
{
    QFileDialog fileDialog;
    QString file_select_name = fileDialog.getOpenFileName(this, "打开dxf文件", "", tr("DXF files(*.dxf)"));
    if (file_select_name.isEmpty()){
        QMessageBox::information(this, "提示", "未选择文件,文件打开失败!");
    }
    else{
        QFile afile(file_select_name);
        if (afile.open(QIODevice::ReadOnly | QIODevice::Text)){
            QScopedPointer<progressShow> progressshow(new progressShow(this));
            QSharedPointer<QVector<QMap<int, QString>>> blockVectorMap(new QVector<QMap<int, QString>>());
            QSharedPointer<QVector<QMap<int, QString>>> insertVectorMap(new QVector<QMap<int, QString>>());
            sortTask->allShapeVector.clear();
            sortTask->curLayShapesVector.clear();
            layToShapeMutiMap.clear();
            QTextStream aStream(&afile);
            aStream.setAutoDetectUnicode(true);
            QString sLine = aStream.readLine().trimmed();
            while (sLine.trimmed() != "EOF"){
                if (sLine == "$UCSORG"){
                    sLine = aStream.readLine();//10
                    sLine = aStream.readLine();
                    ucsX = sLine.trimmed().toDouble();
                    sLine = aStream.readLine();
                    sLine = aStream.readLine();
                    ucsY = sLine.trimmed().toDouble();
                    progressshow->updateProgressValue(25);
                    progressshow->update();
                }
                if (sLine == "ENTITIES"){
                    QString layPosTmp;
                    QVector<double>vector(8);
                    while (sLine != "ENDSEC"){
                        if (sLine == "LINE"){
                            sLine = aStream.readLine().trimmed();
                            vector[0] = sortTask->line;
                            while (sLine.toInt() != 0){
                                switch (sLine.toInt()){
                                case 8: layPosTmp = aStream.readLine().trimmed(); break;
                                case 10:vector[1] = aStream.readLine().trimmed().toDouble() - ucsX; break;
                                case 20:vector[2] = aStream.readLine().trimmed().toDouble() - ucsY; break;
                                case 11:vector[3] = aStream.readLine().trimmed().toDouble() - ucsX; break;
                                case 21:vector[4] = aStream.readLine().trimmed().toDouble() - ucsY; break;
                                default:aStream.readLine(); break;
                                }
                                sLine = aStream.readLine().trimmed();
                            }
                            sortTask->allShapeVector.append(vector.mid(0,5));
                            layToShapeMutiMap.insertMulti(layPosTmp, sortTask->allShapeVector.size() - 1);
                        }
                        else if (sLine == "ELLIPSE"){
                            sLine = aStream.readLine().trimmed();
                            vector[0] = sortTask->ellipse;
                            while (sLine.toInt() != 0){
                                switch (sLine.toInt()){
                                case 8: layPosTmp = aStream.readLine().trimmed(); break;
                                case 10:vector[1] = aStream.readLine().trimmed().toDouble() - ucsX; break;
                                case 20:vector[2] = aStream.readLine().trimmed().toDouble() - ucsY; break;
                                case 11:vector[3] = aStream.readLine().trimmed().toDouble(); break;
                                case 21:vector[4] = aStream.readLine().trimmed().toDouble(); break;
                                case 40:vector[5] = aStream.readLine().trimmed().toDouble(); break;
                                case 41:vector[6] = aStream.readLine().trimmed().toDouble(); break;
                                case 42:vector[7] = aStream.readLine().trimmed().toDouble(); break;
                                default:aStream.readLine(); break;
                                }
                                sLine = aStream.readLine().trimmed();
                            }
                            sortTask->allShapeVector.append(vector);
                            layToShapeMutiMap.insertMulti(layPosTmp, sortTask->allShapeVector.size() - 1);
                        }
                        else if (sLine == "LWPOLYLINE"){
                            sLine = aStream.readLine().trimmed();
                            double bulge = 0;
                            int lwp_close = 0;
                            bool lwp_first = true;
                            double lwp_firstx = 0;
                            double lwp_firsty = 0;
                            uchar getPoint = 0;
                            bool hasBulge = false;
                            double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
                            while (sLine.toInt() != 0){
                                switch (sLine.toInt()){
                                case 8: layPosTmp = aStream.readLine().trimmed(); break;
                                case 70:lwp_close = aStream.readLine().trimmed().toInt(); break;
                                case 42:
                                    bulge = aStream.readLine().trimmed().toDouble();
                                    hasBulge = true;
                                    break;
                                case 10:
                                    if (getPoint < 2)getPoint++;
                                    if (getPoint == 1) x0 = aStream.readLine().trimmed().toDouble() - ucsX;
                                    else if (getPoint == 2) x1 = aStream.readLine().trimmed().toDouble() - ucsX;
                                    if (lwp_first) lwp_firstx = x0;
                                    break;
                                case 20:
                                    if (getPoint == 1) y0 = aStream.readLine().trimmed().toDouble() - ucsY;
                                    else if (getPoint == 2) y1 = aStream.readLine().trimmed().toDouble() - ucsY;
                                    if (lwp_first)lwp_firsty = y0;

                                    if (getPoint == 2){
                                        if (hasBulge){
                                            double b = 0.5*(1 / bulge - bulge);
                                            double x = 0.5*((x0 + x1) - b*(y1 - y0));
                                            double y = 0.5*((y0 + y1) + b*(x1 - x0));
                                            double r = qSqrt(qPow(x1 - x, 2) + qPow(y1 - y, 2));
                                            if (qFabs(x1 - x0) < sortTask->doublePrecision && qFabs(y1 - y0) < sortTask->doublePrecision){
                                                vector[0] = sortTask->circle;
                                                vector[1] = x;
                                                vector[2] = y;
                                                vector[3] = r;
                                                sortTask->allShapeVector.append(vector.mid(0,4));
                                            }
                                            else{
                                                double angle_0, angle_1;
                                                if (bulge > 0){
                                                    angle_1 = qAsin((y1 - y) / r);
                                                    angle_1 = angle_1 * 180 / M_PI;
                                                    if (x1 - x < 0)
                                                        angle_1 = 180 - angle_1;
                                                    else{
                                                        if (angle_1 < 0)
                                                            angle_1 = 360 + angle_1;
                                                    }
                                                    angle_0 = qAsin((y0 - y) / r);
                                                    angle_0 = angle_0 * 180 / M_PI;
                                                    if (x0 - x < 0)
                                                        angle_0 = 180 - angle_0;
                                                    else{
                                                        if (angle_0 < 0)
                                                            angle_0 = 360 + angle_0;
                                                    }
                                                }
                                                else{
                                                    angle_1 = qAsin((y0 - y) / r);
                                                    angle_1 = angle_1 * 180 / M_PI;
                                                    if (x0 - x < 0)
                                                        angle_1 = 180 - angle_1;
                                                    else{
                                                        if (angle_1 < 0)
                                                            angle_1 = 360 + angle_1;
                                                    }
                                                    angle_0 = qAsin((y1 - y) / r);
                                                    angle_0 = angle_0 * 180 / M_PI;
                                                    if (x1 - x < 0)
                                                        angle_0 = 180 - angle_0;
                                                    else{
                                                        if (angle_0 < 0)
                                                            angle_0 = 360 + angle_0;
                                                    }
                                                }
                                                vector[0] = sortTask->arc;
                                                vector[1] = x;
                                                vector[2] = y;
                                                vector[3] = r;
                                                vector[4] = angle_0;
                                                vector[5] = angle_1;
                                                sortTask->allShapeVector.append(vector.mid(0,6));
                                            }
                                        }
                                        else{
                                            vector[0] = sortTask->line;
                                            vector[1] = x0;
                                            vector[2] = y0;
                                            vector[3] = x1;
                                            vector[4] = y1;
                                            sortTask->allShapeVector.append(vector.mid(0,5));
                                        }
                                        layToShapeMutiMap.insertMulti(layPosTmp, sortTask->allShapeVector.size() - 1);
                                        x0 = x1;
                                        y0 = y1;
                                        hasBulge = false;
                                    }
                                    lwp_first = false;
                                    break;
                                default:aStream.readLine(); break;
                                }
                                sLine = aStream.readLine().trimmed();
                            }
                            if (lwp_close){
                                x0 = x1;
                                y0 = y1;
                                x1 = lwp_firstx;
                                y1 = lwp_firsty;
                                if (hasBulge){
                                    double b = 0.5*(1 / bulge - bulge);
                                    double x = 0.5*((x0 + x1) - b*(y1 - y0));
                                    double y = 0.5*((y0 + y1) + b*(x1 - x0));
                                    double r = qSqrt(qPow(x1 - x, 2) + qPow(y1 - y, 2));
                                    if (qFabs(x1 - x0) < sortTask->doublePrecision && qFabs(y1 - y0) < sortTask->doublePrecision){
                                        vector[0] = sortTask->circle;
                                        vector[1] = x;
                                        vector[2] = y;
                                        vector[3] = r;
                                        sortTask->allShapeVector.append(vector.mid(0,4));
                                    }
                                    else{
                                        double angle_0, angle_1;
                                        if (bulge > 0){
                                            angle_1 = qAsin((y1 - y) / r);
                                            angle_1 = angle_1 * 180 / M_PI;
                                            if (x1 < 0)
                                                angle_1 = 180 - angle_1;
                                            else{
                                                if (angle_1 < 0)
                                                    angle_1 = 360 + angle_1;
                                            }
                                            angle_0 = qAsin((y0 - y) / r);
                                            angle_0 = angle_0 * 180 / M_PI;
                                            if (x0 < 0)
                                                angle_0 = 180 - angle_0;
                                            else{
                                                if (angle_0 < 0)
                                                    angle_0 = 360 + angle_0;
                                            }
                                        }
                                        else{
                                            angle_1 = qAsin((y0 - y) / r);
                                            angle_1 = angle_1 * 180 / M_PI;
                                            if (x0 < 0)
                                                angle_1 = 180 - angle_1;
                                            else{
                                                if (angle_1 < 0)
                                                    angle_1 = 360 + angle_1;
                                            }
                                            angle_0 = qAsin((y1 - y) / r);
                                            angle_0 = angle_0 * 180 / M_PI;
                                            if (x1 < 0)
                                                angle_0 = 180 - angle_0;
                                            else{
                                                if (angle_0 < 0)
                                                    angle_0 = 360 + angle_0;
                                            }
                                        }
                                        vector[0] = sortTask->arc;
                                        vector[1] = x;
                                        vector[2] = y;
                                        vector[3] = r;
                                        vector[4] = angle_0;
                                        vector[5] = angle_1;
                                        sortTask->allShapeVector.append(vector.mid(0,6));
                                    }
                                }
                                else{
                                    vector[0] = sortTask->line;
                                    vector[1] = x0;
                                    vector[2] = y0;
                                    vector[3] = x1;
                                    vector[4] = y1;
                                    sortTask->allShapeVector.append(vector.mid(0,5));
                                }
                                layToShapeMutiMap.insertMulti(layPosTmp, sortTask->allShapeVector.size() - 1);
                            }
                        }
                        else if (sLine == "ARC"){
                            sLine = aStream.readLine().trimmed();
                            vector[0] = sortTask->arc;
                            while (sLine.toInt() != 0){
                                switch (sLine.toInt()){
                                case 8: layPosTmp = aStream.readLine().trimmed(); break;
                                case 10:vector[1] = aStream.readLine().trimmed().toDouble() - ucsX; break;
                                case 20:vector[2] = aStream.readLine().trimmed().toDouble() - ucsY; break;
                                case 40:vector[3] = aStream.readLine().trimmed().toDouble(); break;
                                case 50:vector[4] = aStream.readLine().trimmed().toDouble(); break;
                                case 51:vector[5] = aStream.readLine().trimmed().toDouble(); break;
                                default:aStream.readLine(); break;
                                }
                                sLine = aStream.readLine().trimmed();
                            }
                            sortTask->allShapeVector.append(vector.mid(0,6));
                            layToShapeMutiMap.insertMulti(layPosTmp, sortTask->allShapeVector.size() - 1);
                        }
                        else if (sLine == "CIRCLE"){
                            sLine = aStream.readLine().trimmed();
                            vector[0] = sortTask->circle;
                            while (sLine.toInt() != 0){
                                switch (sLine.toInt()){
                                case 8: layPosTmp = aStream.readLine().trimmed(); break;
                                case 10:vector[1] = aStream.readLine().trimmed().toDouble() - ucsX; break;
                                case 20:vector[2] = aStream.readLine().trimmed().toDouble() - ucsY; break;
                                case 40:vector[3] = aStream.readLine().trimmed().toDouble(); break;
                                default:aStream.readLine(); break;
                                }
                                sLine = aStream.readLine().trimmed();
                            }
                            sortTask->allShapeVector.append(vector.mid(0,4));
                            layToShapeMutiMap.insertMulti(layPosTmp, sortTask->allShapeVector.size() - 1);
                        }
                        else if (sLine == "POINT"){
                            sLine = aStream.readLine().trimmed();
                            vector[0] = sortTask->point;
                            while (sLine.toInt() != 0){
                                switch (sLine.toInt()){
                                case 8: layPosTmp = aStream.readLine().trimmed(); break;
                                case 10:vector[1] = aStream.readLine().trimmed().toDouble(); break;
                                case 20:vector[2] = aStream.readLine().trimmed().toDouble(); break;
                                default:aStream.readLine(); break;
                                }
                                sLine = aStream.readLine().trimmed();
                            }
                            sortTask->allShapeVector.append(vector.mid(0,3));
                            layToShapeMutiMap.insertMulti(layPosTmp, sortTask->allShapeVector.size() - 1);
                        }
                        else if (sLine == "INSERT"){
                            sLine = aStream.readLine().trimmed();
                            QMap<int, QString> qmapTmp;
                            while (sLine.toInt() != 0){
                                switch (sLine.toInt()){
                                case 2://块名
                                case 10:
                                case 20:
                                case 41:
                                case 42:
                                case 43:
                                case 50:{
                                    QString sLineTmp = aStream.readLine().trimmed();
                                    qmapTmp.insert(sLine.toInt(), sLineTmp);
                                }break;
                                default:aStream.readLine(); break;
                                }
                                sLine = aStream.readLine().trimmed();
                            }
                            insertVectorMap->append(qmapTmp);
                        }
                        sLine = aStream.readLine().trimmed();
                        progressshow->updateProgressValue(50);
                    }
                }
                if (sLine == "BLOCKS"){
                    while (sLine != "ENDSEC"){
                        if (sLine == "BLOCK"){
                            sLine = aStream.readLine().trimmed();
                            int case_num = 0;
                            QMap<int, QString> qmapTmp;
                            double OrgOffSetX = 0, OrgOffSetY = 0;
                            bool isEllipse = false, skipSave = false;
                            while (sLine.toInt() != 3){
                                switch (sLine.toInt()){
                                case 2:{
                                    QString sLineTmp = aStream.readLine().trimmed();
                                    if (sLineTmp == "*Model_Space" || sLineTmp == "*Paper_Space" || sLineTmp == "*Paper_Space0")
                                        skipSave = true;
                                    else
                                        qmapTmp.insert(sLine.toInt(), sLineTmp);
                                }break;
                                case 10: OrgOffSetX = aStream.readLine().trimmed().toDouble(); break;
                                case 20: OrgOffSetY = aStream.readLine().trimmed().toDouble(); break;
                                default:aStream.readLine(); break;
                                }
                                sLine = aStream.readLine().trimmed();
                            }
                            bool searchDone = false;
                            while (!searchDone){
                                switch (sLine.toInt()){
                                case 0:{
                                    sLine = aStream.readLine().trimmed();
                                    if (sLine == "ENDBLK")
                                        searchDone = true;
                                    else{
                                        case_num = case_num + 1;
                                        qmapTmp.insert(25 + 100 * case_num, "1000");
                                        case_num = case_num + 1;
                                        qmapTmp.insert(0 + 100 * case_num, sLine);
                                        if (sLine == "ELLIPSE")
                                            isEllipse = true;
                                    }
                                }break;
                                case 8:
                                case 10:
                                case 20:
                                case 40:
                                case 41:
                                case 42:
                                case 11:
                                case 21:
                                case 50:
                                case 51:
                                case 70:{
                                    case_num = case_num + 1;
                                    QString strTmp;
                                    if (sLine.toInt() == 10)
                                        strTmp = QString::number(aStream.readLine().trimmed().toDouble() - OrgOffSetX, 10, 16);
                                    else if (sLine.toInt() == 11 && !isEllipse)
                                        strTmp = QString::number(aStream.readLine().trimmed().toDouble() - OrgOffSetX, 10, 16);
                                    else if (sLine.toInt() == 20)
                                        strTmp = QString::number(aStream.readLine().trimmed().toDouble() - OrgOffSetY, 10, 16);
                                    else if (sLine.toInt() == 21 && !isEllipse)
                                        strTmp = QString::number(aStream.readLine().trimmed().toDouble() - OrgOffSetY, 10, 16);
                                    else
                                        strTmp = aStream.readLine().trimmed();
                                    qmapTmp.insert(sLine.toInt() + 100 * case_num, strTmp);
                                    if (sLine.toInt() == 21)
                                        isEllipse = false;
                                }break;
                                default:aStream.readLine(); break;
                                }
                                sLine = aStream.readLine().trimmed();
                            }
                            if (!skipSave){
                                case_num = case_num + 1;
                                qmapTmp.insert(25 + 100 * case_num, "1000");
                                blockVectorMap->append(qmapTmp);
                            }
                        }
                        sLine = aStream.readLine().trimmed();
                    }
                }
                sLine = aStream.readLine().trimmed();
            }
            progressshow->updateProgressValue(75);
            afile.close();
            ParseInsert(insertVectorMap, blockVectorMap);
            initComBox();
            sortTask->allShapeVector.squeeze();
            sortTask->curLayShapesVector.squeeze();
            progressshow->updateProgressValue(100);
        }
    }
}

void parseDXF::ParseInsert(QSharedPointer<QVector<QMap<int, QString>>> insertVectorMap, QSharedPointer<QVector<QMap<int, QString>>> blockVectorMap)
{
    int map = 0;
    QString layPosTmp;
    QMap<int, QString>::const_iterator map_J;
    QVector<double>vector(8);
    double x = 0, y = 0, rotateAngle = 0, stretchX = 1, stretchY = 1;
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;

    for (int i = 0; i<insertVectorMap->size(); i++){
        rotateAngle = 0;
        stretchX = 1;
        stretchY = 1;
        for (int j = 0; j<blockVectorMap->size(); j++){
            map_J = blockVectorMap->at(j).constBegin();
            if (insertVectorMap->at(i).constBegin().value() == map_J.value()){
                for (map_J = insertVectorMap->at(i).constBegin(); map_J != insertVectorMap->at(i).constEnd(); map_J++){
                    if (map_J.key() >= 100)
                        map = map_J.key() % 100;
                    else
                        map = map_J.key();
                    switch (map){
                    case 10:x = map_J.value().toDouble() - ucsX; break;
                    case 20:y = map_J.value().toDouble() - ucsY; break;
                    case 41:stretchX = map_J.value().toDouble(); break;
                    case 42:stretchY = map_J.value().toDouble(); break;
                    case 50:rotateAngle = map_J.value().toDouble(); break;
                    }
                }
                for (map_J = blockVectorMap->at(j).constBegin(); map_J != blockVectorMap->at(j).constEnd(); map_J++){
                    if (map_J.value() == "LINE"){
                        vector[0] = sortTask->line;
                        map_J++;
                        if (map_J.key() >= 100)
                            map = map_J.key() % 100;
                        else
                            map = map_J.key();
                        while (map != 25){
                            switch (map){
                            case 8:layPosTmp = map_J.value(); break;
                            case 10:x0 = map_J.value().toDouble()*stretchX + x; break;
                            case 20:y0 = map_J.value().toDouble()*stretchY + y; break;
                            case 11:x1 = map_J.value().toDouble()*stretchX + x; break;
                            case 21:y1 = map_J.value().toDouble()*stretchY + y; break;
                            }
                            map_J++;
                            if (map_J.key() >= 100)
                                map = map_J.key() % 100;
                            else
                                map = map_J.key();
                        }
                        vector[1] = x + (x0 - x)*qCos(rotateAngle / 180 * M_PI) - (y0 - y)*qSin(rotateAngle / 180 * M_PI);
                        vector[2] = y + (y0 - y)*qCos(rotateAngle / 180 * M_PI) + (x0 - x)*qSin(rotateAngle / 180 * M_PI);
                        vector[3] = x + (x1 - x)*qCos(rotateAngle / 180 * M_PI) - (y1 - y)*qSin(rotateAngle / 180 * M_PI);
                        vector[4] = y + (y1 - y)*qCos(rotateAngle / 180 * M_PI) + (x1 - x)*qSin(rotateAngle / 180 * M_PI);

                        sortTask->allShapeVector.append(vector.mid(0,5));
                        layToShapeMutiMap.insertMulti(layPosTmp, sortTask->allShapeVector.size() - 1);
                    }
                    else if (map_J.value() == "ELLIPSE"){
                        vector[0] = sortTask->ellipse;
                        map_J++;
                        if (map_J.key() >= 100)
                            map = map_J.key() % 100;
                        else
                            map = map_J.key();
                        while (map != 25){
                            switch (map){
                            case 8:layPosTmp = map_J.value(); break;
                            case 10:x0 = map_J.value().toDouble()*stretchX + x; break;
                            case 20:y0 = map_J.value().toDouble()*stretchY + y; break;
                            case 11:x1 = map_J.value().toDouble()*stretchX; break;//
                            case 21:y1 = map_J.value().toDouble()*stretchY; break;//
                            case 40:vector[5] = map_J.value().toDouble(); break;
                            case 41:vector[6] = map_J.value().toDouble(); break;
                            case 42:vector[7] = map_J.value().toDouble(); break;
                            }
                            map_J++;
                            if (map_J.key() >= 100)
                                map = map_J.key() % 100;
                            else
                                map = map_J.key();
                        }
                        vector[1] = x + (x0 - x)*qCos(rotateAngle / 180 * M_PI) - (y0 - y)*qSin(rotateAngle / 180 * M_PI);
                        vector[2] = y + (y0 - y)*qCos(rotateAngle / 180 * M_PI) + (x0 - x)*qSin(rotateAngle / 180 * M_PI);
                        vector[3] = x + (x1 + x0 - x)*qCos(rotateAngle / 180 * M_PI) - (y1 + y0 - y)*qSin(rotateAngle / 180 * M_PI) - vector[1];
                        vector[4] = y + (y1 + y0 - y)*qCos(rotateAngle / 180 * M_PI) + (x1 + x0 - x)*qSin(rotateAngle / 180 * M_PI) - vector[2];
                        sortTask->allShapeVector.append(vector);
                        layToShapeMutiMap.insertMulti(layPosTmp, sortTask->allShapeVector.size() - 1);
                    }
                    else if (map_J.value() == "POINT"){
                        vector[0] = sortTask->point;
                        map_J++;
                        if (map_J.key() >= 100)
                            map = map_J.key() % 100;
                        else
                            map = map_J.key();
                        while (map != 25){
                            switch (map){
                            case 8:layPosTmp = map_J.value(); break;
                            case 10:x0 = map_J.value().toDouble()*stretchX + x; break;
                            case 20:y0 = map_J.value().toDouble()*stretchY + y; break;
                            }
                            map_J++;
                            if (map_J.key() >= 100)
                                map = map_J.key() % 100;
                            else
                                map = map_J.key();
                        }
                        vector[1] = x + (x0 - x)*qCos(rotateAngle / 180 * M_PI) - (y0 - y)*qSin(rotateAngle / 180 * M_PI);
                        vector[2] = y + (y0 - y)*qCos(rotateAngle / 180 * M_PI) + (x0 - x)*qSin(rotateAngle / 180 * M_PI);
                        sortTask->allShapeVector.append(vector.mid(0,3));
                        layToShapeMutiMap.insertMulti(layPosTmp, sortTask->allShapeVector.size() - 1);
                    }
                    else if (map_J.value() == "CIRCLE"){
                        vector[0] = sortTask->circle;
                        map_J++;
                        double r = 0;
                        if (map_J.key() >= 100)
                            map = map_J.key() % 100;
                        else
                            map = map_J.key();
                        while (map != 25){
                            switch (map){
                            case 8:layPosTmp = map_J.value(); break;
                            case 10:x0 = map_J.value().toDouble()*stretchX + x; break;
                            case 20:y0 = map_J.value().toDouble()*stretchY + y; break;
                            case 40:r = map_J.value().toDouble()*stretchX; break;
                            }
                            map_J++;
                            if (map_J.key() >= 100)
                                map = map_J.key() % 100;
                            else
                                map = map_J.key();
                        }
                        vector[1] = x + (x0 - x)*qCos(rotateAngle / 180 * M_PI) - (y0 - y)*qSin(rotateAngle / 180 * M_PI);
                        vector[2] = y + (y0 - y)*qCos(rotateAngle / 180 * M_PI) + (x0 - x)*qSin(rotateAngle / 180 * M_PI);
                        vector[3] = r;
                        sortTask->allShapeVector.append(vector.mid(0,4));
                        layToShapeMutiMap.insertMulti(layPosTmp, sortTask->allShapeVector.size() - 1);
                    }
                    else if (map_J.value() == "LWPOLYLINE"){
                        map_J++;
                        double bulge = 0;
                        int lwp_close = 0;
                        bool lwp_first = true;
                        double lwp_firstx = 0;
                        double lwp_firsty = 0;
                        uchar getPoint = 0;
                        bool hasBulge = false;
                        double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
                        double x_tmp = 0, y_tmp = 0;
                        if (map_J.key() >= 100)
                            map = map_J.key() % 100;
                        else
                            map = map_J.key();
                        while (map != 25){
                            switch (map){
                            case 8:layPosTmp = map_J.value(); break;
                            case 70:lwp_close = map_J.value().toInt(); break;
                            case 42:
                                bulge = map_J.value().toDouble();
                                hasBulge = true;
                                break;
                            case 10:x_tmp = map_J.value().toDouble()*stretchX + x; break;
                            case 20:
                                y_tmp = map_J.value().toDouble()*stretchY + y;
                                double x11 = x + (x_tmp - x)*qCos(rotateAngle / 180 * M_PI) - (y_tmp - y)*qSin(rotateAngle / 180 * M_PI);
                                double y11 = y + (y_tmp - y)*qCos(rotateAngle / 180 * M_PI) + (x_tmp - x)*qSin(rotateAngle / 180 * M_PI);
                                if (lwp_first){
                                    lwp_firstx = x11;
                                    lwp_firsty = y11;
                                }
                                if (getPoint < 2) getPoint++;
                                if (getPoint == 1){
                                    x0 = x11;
                                    y0 = y11;
                                }
                                else if (getPoint == 2){
                                    x1 = x11;
                                    y1 = y11;
                                    if (hasBulge){
                                        double b = 0.5*(1 / bulge - bulge);
                                        double x = 0.5*((x0 + x1) - b*(y1 - y0));
                                        double y = 0.5*((y0 + y1) + b*(x1 - x0));
                                        double r = qSqrt(qPow(x1 - x, 2) + qPow(y1 - y, 2));
                                        if (qFabs(x1 - x0) < sortTask->doublePrecision && qFabs(y1 - y0) < sortTask->doublePrecision){
                                            vector[0] = sortTask->circle;
                                            vector[1] = x;
                                            vector[2] = y;
                                            vector[3] = r;
                                            sortTask->allShapeVector.append(vector.mid(0,4));
                                        }
                                        else{
                                            double angle_0, angle_1;
                                            if (bulge > 0){
                                                angle_1 = qAsin((y1 - y) / r);
                                                angle_1 = angle_1 * 180 / M_PI;
                                                if (x1 - x < 0)
                                                    angle_1 = 180 - angle_1;
                                                else{
                                                    if (angle_1 < 0)
                                                        angle_1 = 360 + angle_1;
                                                }
                                                angle_0 = qAsin((y0 - y) / r);
                                                angle_0 = angle_0 * 180 / M_PI;
                                                if (x0 - x < 0)
                                                    angle_0 = 180 - angle_0;
                                                else{
                                                    if (angle_0 < 0)
                                                        angle_0 = 360 + angle_0;
                                                }
                                            }
                                            else{
                                                angle_1 = qAsin((y0 - y) / r);
                                                angle_1 = angle_1 * 180 / M_PI;
                                                if (x0 - x < 0)
                                                    angle_1 = 180 - angle_1;
                                                else{
                                                    if (angle_1 < 0)
                                                        angle_1 = 360 + angle_1;
                                                }
                                                angle_0 = qAsin((y1 - y) / r);
                                                angle_0 = angle_0 * 180 / M_PI;
                                                if (x1 - x < 0)
                                                    angle_0 = 180 - angle_0;
                                                else{
                                                    if (angle_0 < 0)
                                                        angle_0 = 360 + angle_0;
                                                }
                                            }
                                            vector[0] = sortTask->arc;
                                            vector[1] = x;
                                            vector[2] = y;
                                            vector[3] = r;
                                            vector[4] = angle_0;
                                            vector[5] = angle_1;
                                            sortTask->allShapeVector.append(vector.mid(0,6));
                                        }
                                    }
                                    else{
                                        vector[0] = sortTask->line;
                                        vector[1] = x0;
                                        vector[2] = y0;
                                        vector[3] = x1;
                                        vector[4] = y1;
                                        sortTask->allShapeVector.append(vector.mid(0,5));
                                    }
                                    layToShapeMutiMap.insertMulti(layPosTmp, sortTask->allShapeVector.size() - 1);
                                    x0 = x1;
                                    y0 = y1;
                                    hasBulge = false;
                                }
                                if (lwp_first)lwp_first = false;
                                break;
                            }
                            map_J++;
                            if (map_J.key() >= 100)
                                map = map_J.key() % 100;
                            else
                                map = map_J.key();
                        }
                        if (lwp_close){
                            x0 = x1;
                            y0 = y1;
                            x1 = lwp_firstx;
                            y1 = lwp_firsty;
                            if (hasBulge){
                                double b = 0.5*(1 / bulge - bulge);
                                double x = 0.5*((x0 + x1) - b*(y1 - y0));
                                double y = 0.5*((y0 + y1) + b*(x1 - x0));
                                double r = qSqrt(qPow(x1 - x, 2) + qPow(y1 - y, 2));
                                if (qFabs(x1 - x0) < sortTask->doublePrecision && qFabs(y1 - y0) < sortTask->doublePrecision){
                                    vector[0] = sortTask->circle;
                                    vector[1] = x;
                                    vector[2] = y;
                                    vector[3] = r;
                                    sortTask->allShapeVector.append(vector.mid(0,4));
                                }
                                else{
                                    double angle_0, angle_1;
                                    if (bulge > 0){
                                        angle_1 = qAsin((y1 - y) / r);
                                        angle_1 = angle_1 * 180 / M_PI;
                                        if (x1 - x < 0)
                                            angle_1 = 180 - angle_1;
                                        else{
                                            if (angle_1 < 0)
                                                angle_1 = 360 + angle_1;
                                        }
                                        angle_0 = qAsin((y0 - y) / r);
                                        angle_0 = angle_0 * 180 / M_PI;
                                        if (x0 - x < 0)
                                            angle_0 = 180 - angle_0;
                                        else{
                                            if (angle_0 < 0)
                                                angle_0 = 360 + angle_0;
                                        }
                                    }
                                    else{
                                        angle_1 = qAsin((y0 - y) / r);
                                        angle_1 = angle_1 * 180 / M_PI;
                                        if (x0 - x < 0)
                                            angle_1 = 180 - angle_1;
                                        else{
                                            if (angle_1 < 0)
                                                angle_1 = 360 + angle_1;
                                        }
                                        angle_0 = qAsin((y1 - y) / r);
                                        angle_0 = angle_0 * 180 / M_PI;
                                        if (x1 - x < 0)
                                            angle_0 = 180 - angle_0;
                                        else{
                                            if (angle_0 < 0)
                                                angle_0 = 360 + angle_0;
                                        }
                                    }
                                    vector[0] = sortTask->arc;
                                    vector[1] = x;
                                    vector[2] = y;
                                    vector[3] = r;
                                    vector[4] = angle_0;
                                    vector[5] = angle_1;
                                    sortTask->allShapeVector.append(vector.mid(0,6));
                                }
                            }
                            else{
                                vector[0] = sortTask->line;
                                vector[1] = x0;
                                vector[2] = y0;
                                vector[3] = x1;
                                vector[4] = y1;
                                sortTask->allShapeVector.append(vector.mid(0,5));
                            }
                            layToShapeMutiMap.insertMulti(layPosTmp, sortTask->allShapeVector.size() - 1);
                        }
                    }
                    else if (map_J.value() == "ARC"){
                        vector[0] = sortTask->arc;
                        map_J++;
                        double r = 0, angle_0 = 0, angle_1 = 0;
                        if (map_J.key() >= 100)
                            map = map_J.key() % 100;
                        else
                            map = map_J.key();
                        while (map != 25){
                            switch (map){
                            case 8:layPosTmp = map_J.value(); break;
                            case 10:x0 = map_J.value().toDouble()*stretchX + x; break;
                            case 20:y0 = map_J.value().toDouble()*stretchY + y; break;
                            case 40:r = map_J.value().toDouble()*stretchX; break;
                            case 50:angle_0 = map_J.value().toDouble(); break;
                            case 51:angle_1 = map_J.value().toDouble(); break;
                            }
                            map_J++;
                            if (map_J.key() >= 100)
                                map = map_J.key() % 100;
                            else
                                map = map_J.key();
                        }
                        vector[1] = x + (x0 - x)*qCos(rotateAngle / 180 * M_PI) - (y0 - y)*qSin(rotateAngle / 180 * M_PI);
                        vector[2] = y + (y0 - y)*qCos(rotateAngle / 180 * M_PI) + (x0 - x)*qSin(rotateAngle / 180 * M_PI);
                        vector[3] = r;
                        if (angle_0 + rotateAngle >= 360)
                            angle_0 = angle_0 + rotateAngle - 360;
                        else
                            angle_0 = angle_0 + rotateAngle;
                        if (angle_1 + rotateAngle >= 360)
                            angle_1 = angle_1 + rotateAngle - 360;
                        else
                            angle_1 = angle_1 + rotateAngle;
                        vector[4] = angle_0;
                        vector[5] = angle_1;
                        sortTask->allShapeVector.append(vector.mid(0,6));
                        layToShapeMutiMap.insertMulti(layPosTmp, sortTask->allShapeVector.size() - 1);
                    }
                }
            }
        }
    }
}

void parseDXF::initAxisItem()
{
    axisXLineItem = new QGraphicsLineItem;
    axisYLineItem = new QGraphicsLineItem;
    pen.setStyle(Qt::DashDotDotLine);
    axisXLineItem->setPen(pen);
    axisYLineItem->setPen(pen);
    scene->addItem(axisXLineItem);
    scene->addItem(axisYLineItem);
    pen.setStyle(Qt::SolidLine);
}

void parseDXF::drawCenterAxis()
{
    axisXLineItem->setLine(0, -axisLength, 0, axisLength);
    axisYLineItem->setLine(-axisLength, 0, axisLength, 0);
}

void parseDXF::draw_scene()
{
    deleteExtraItem();

    findScenItemMap.clear();
    scene->clear();

    initAxisItem();

    double minX=-10,maxX=10,minY=-10,maxY=10;
    bool firstFor=true;
    double r0 = 0, r1 = 0, angle_0 = 0, angle_1 = 0, angle_move = 0, theta = 0;
    foreach(int i, sortTask->curLayShapesVector){
        auto shapeType = static_cast<sortShapeTask::shapeType>(static_cast<int>(sortTask->allShapeVector[i][0]));
        if (shapeType == sortTask->point){
            QGraphicsLineItem *item = new QGraphicsLineItem;
            item->setLine(sortTask->allShapeVector[i][1], -sortTask->allShapeVector[i][2], sortTask->allShapeVector[i][1], -sortTask->allShapeVector[i][2]);
            item->setPen(pen);
            item->setFlag(QGraphicsItem::ItemIsSelectable, false);
            findScenItemMap.insert(i, item);
            scene->addItem(item);
            if(firstFor){
                firstFor=false;
                minX=maxX=sortTask->allShapeVector[i][1];
                minY=maxY=sortTask->allShapeVector[i][2];
            }
            else{
                if(sortTask->allShapeVector[i][1]>maxX)maxX=sortTask->allShapeVector[i][1];
                if(sortTask->allShapeVector[i][1]<minX)minX=sortTask->allShapeVector[i][1];
                if(sortTask->allShapeVector[i][2]>maxY)maxY=sortTask->allShapeVector[i][2];
                if(sortTask->allShapeVector[i][2]<minY)minY=sortTask->allShapeVector[i][2];
            }
        }
        else if (shapeType == sortTask->line){
            QGraphicsLineItem *item = new QGraphicsLineItem;
            item->setLine(sortTask->allShapeVector[i][1], -sortTask->allShapeVector[i][2], sortTask->allShapeVector[i][3], -sortTask->allShapeVector[i][4]);
            item->setPen(pen);
            item->setFlag(QGraphicsItem::ItemIsSelectable, false);
            findScenItemMap.insert(i, item);
            scene->addItem(item);
            if(firstFor){
                firstFor=false;
                if(sortTask->allShapeVector[i][1]>sortTask->allShapeVector[i][3]){
                    minX=sortTask->allShapeVector[i][3];
                    maxX=sortTask->allShapeVector[i][1];
                }else{
                    minX=sortTask->allShapeVector[i][1];
                    maxX=sortTask->allShapeVector[i][3];
                }
                if(sortTask->allShapeVector[i][2]>sortTask->allShapeVector[i][4]){
                    minY=sortTask->allShapeVector[i][4];
                    maxY=sortTask->allShapeVector[i][2];
                }
                else{
                    minY=sortTask->allShapeVector[i][2];
                    maxY=sortTask->allShapeVector[i][4];
                }
            }
            else{
                if(sortTask->allShapeVector[i][1]>maxX)maxX=sortTask->allShapeVector[i][1];
                if(sortTask->allShapeVector[i][1]<minX)minX=sortTask->allShapeVector[i][1];
                if(sortTask->allShapeVector[i][2]>maxY)maxY=sortTask->allShapeVector[i][2];
                if(sortTask->allShapeVector[i][2]<minY)minY=sortTask->allShapeVector[i][2];
                if(sortTask->allShapeVector[i][3]>maxX)maxX=sortTask->allShapeVector[i][3];
                if(sortTask->allShapeVector[i][3]<minX)minX=sortTask->allShapeVector[i][3];
                if(sortTask->allShapeVector[i][4]>maxY)maxY=sortTask->allShapeVector[i][4];
                if(sortTask->allShapeVector[i][4]<minY)minY=sortTask->allShapeVector[i][4];
            }
        }
        else if (shapeType == sortTask->circle){
            QGraphicsEllipseItem *item = new QGraphicsEllipseItem;
            item->setRect((sortTask->allShapeVector[i][1] - sortTask->allShapeVector[i][3]), -(sortTask->allShapeVector[i][2] + sortTask->allShapeVector[i][3]), 2 * sortTask->allShapeVector[i][3], 2 * sortTask->allShapeVector[i][3]);
            item->setPen(pen);
            item->setFlag(QGraphicsItem::ItemIsSelectable, true);
            findScenItemMap.insert(i, item);
            scene->addItem(item);
            if(firstFor){
                firstFor=false;
                minX=sortTask->allShapeVector[i][1]-sortTask->allShapeVector[i][3];
                maxX=sortTask->allShapeVector[i][1]+sortTask->allShapeVector[i][3];
                minY=sortTask->allShapeVector[i][2]-sortTask->allShapeVector[i][3];
                maxY=sortTask->allShapeVector[i][2]+sortTask->allShapeVector[i][3];
            }
            else{
                if(sortTask->allShapeVector[i][1]+sortTask->allShapeVector[i][3]>maxX)maxX=sortTask->allShapeVector[i][1]+sortTask->allShapeVector[i][3];
                if(sortTask->allShapeVector[i][1]-sortTask->allShapeVector[i][3]<minX)minX=sortTask->allShapeVector[i][1]-sortTask->allShapeVector[i][3];
                if(sortTask->allShapeVector[i][2]+sortTask->allShapeVector[i][3]>maxY)maxY=sortTask->allShapeVector[i][2]+sortTask->allShapeVector[i][3];
                if(sortTask->allShapeVector[i][2]-sortTask->allShapeVector[i][3]<minY)minY=sortTask->allShapeVector[i][2]-sortTask->allShapeVector[i][3];
            }
        }
        else if (shapeType == sortTask->arc){
            QPainterPath path;
            path.arcMoveTo(sortTask->allShapeVector[i][1] - sortTask->allShapeVector[i][3], -(sortTask->allShapeVector[i][2] + sortTask->allShapeVector[i][3]), 2 * sortTask->allShapeVector[i][3], 2 * sortTask->allShapeVector[i][3], sortTask->allShapeVector[i][4]);
            if ((sortTask->allShapeVector[i][5] - sortTask->allShapeVector[i][4]) < 0)
                angle_move = sortTask->allShapeVector[i][5] - sortTask->allShapeVector[i][4] + 360;
            else
                angle_move = sortTask->allShapeVector[i][5] - sortTask->allShapeVector[i][4];
            path.arcTo(sortTask->allShapeVector[i][1] - sortTask->allShapeVector[i][3], -(sortTask->allShapeVector[i][2] + sortTask->allShapeVector[i][3]), 2 * sortTask->allShapeVector[i][3], 2 * sortTask->allShapeVector[i][3], sortTask->allShapeVector[i][4], angle_move);
            QGraphicsPathItem *item = new QGraphicsPathItem;
            item->setPath(path);
            item->setPen(pen);
            item->setFlag(QGraphicsItem::ItemIsSelectable, true);
            findScenItemMap.insert(i, item);
            scene->addItem(item);
            if(firstFor){
                firstFor=false;
                minX=sortTask->allShapeVector[i][1]-sortTask->allShapeVector[i][3];
                maxX=sortTask->allShapeVector[i][1]+sortTask->allShapeVector[i][3];
                minY=sortTask->allShapeVector[i][2]-sortTask->allShapeVector[i][3];
                maxY=sortTask->allShapeVector[i][2]+sortTask->allShapeVector[i][3];
            }
            else{
                if(sortTask->allShapeVector[i][1]+sortTask->allShapeVector[i][3]>maxX)maxX=sortTask->allShapeVector[i][1]+sortTask->allShapeVector[i][3];
                if(sortTask->allShapeVector[i][1]-sortTask->allShapeVector[i][3]<minX)minX=sortTask->allShapeVector[i][1]-sortTask->allShapeVector[i][3];
                if(sortTask->allShapeVector[i][2]+sortTask->allShapeVector[i][3]>maxY)maxY=sortTask->allShapeVector[i][2]+sortTask->allShapeVector[i][3];
                if(sortTask->allShapeVector[i][2]-sortTask->allShapeVector[i][3]<minY)minY=sortTask->allShapeVector[i][2]-sortTask->allShapeVector[i][3];
            }
        }
        else if (shapeType == sortTask->ellipse){
            r0 = qSqrt(qPow(sortTask->allShapeVector[i][3], 2) + qPow(sortTask->allShapeVector[i][4], 2));
            r1 = sortTask->allShapeVector[i][5] * r0;
            angle_0 = sortTask->allShapeVector[i][6] * 180 / M_PI;
            angle_1 = sortTask->allShapeVector[i][7] * 180 / M_PI;
            QPainterPath path;
            path.arcMoveTo(sortTask->allShapeVector[i][1] - r0, -(sortTask->allShapeVector[i][2] + r1), 2 * r0, 2 * r1, angle_0);
            if ((angle_1 - angle_0) < 0)
                angle_1 = angle_1 - angle_0 + 360;
            else
                angle_1 = angle_1 - angle_0;
            path.arcTo(sortTask->allShapeVector[i][1] - r0, -(sortTask->allShapeVector[i][2] + r1), 2 * r0, 2 * r1, angle_0, angle_1);
            theta = -qAtan2(sortTask->allShapeVector[i][4], sortTask->allShapeVector[i][3]) * 180 / M_PI;
            QGraphicsPathItem *item = new QGraphicsPathItem;
            item->setPath(path);
            item->setPen(pen);
            item->setFlag(QGraphicsItem::ItemIsSelectable, true);
            item->setTransformOriginPoint(sortTask->allShapeVector[i][1], -sortTask->allShapeVector[i][2]);
            item->setRotation(theta);
            findScenItemMap.insert(i, item);
            scene->addItem(item);
            if(firstFor){
                firstFor=false;
                minX=sortTask->allShapeVector[i][1]-r0;
                maxX=sortTask->allShapeVector[i][1]+r0;
                minY=sortTask->allShapeVector[i][2]-r0;
                maxY=sortTask->allShapeVector[i][2]+r0;
            }
            else{
                if(sortTask->allShapeVector[i][1]+r0>maxX)maxX=sortTask->allShapeVector[i][1]+r0;
                if(sortTask->allShapeVector[i][1]-r0<minX)minX=sortTask->allShapeVector[i][1]-r0;
                if(sortTask->allShapeVector[i][2]+r0>maxY)maxY=sortTask->allShapeVector[i][2]+r0;
                if(sortTask->allShapeVector[i][2]-r0<minY)minY=sortTask->allShapeVector[i][2]-r0;
            }
        }
    }
    QMatrix matrix(1,0,0,1,0,0);
    ui->graphicsView->setMatrix(matrix);
    scene->setSceneRect(-0.5*ui->graphicsView->width(), -0.5*ui->graphicsView->height(), ui->graphicsView->width(), ui->graphicsView->height());

    double scaleXTmp=scene->sceneRect().width()/(maxX-minX),scaleYTmp=scene->sceneRect().height()/(maxY-minY);
    double scaleTmp=qMin(scaleXTmp,scaleYTmp);
    matrix.setMatrix(ui->graphicsView->matrix().m11()*scaleTmp,ui->graphicsView->matrix().m12(),ui->graphicsView->matrix().m21(),ui->graphicsView->matrix().m22()*scaleTmp,ui->graphicsView->matrix().dx(),ui->graphicsView->matrix().dy());
    ui->graphicsView->setMatrix(matrix);

    point_dis = Cpoint_dis / ui->graphicsView->matrix().m11();
    axisLength = CaxisLength / ui->graphicsView->matrix().m11();
    drawCenterAxis();
    scene->setSceneRect(ui->graphicsView->mapToScene(ui->graphicsView->rect()).boundingRect());

    double middleX=(minX+maxX)/2,middleY=(minY+maxY)/2;
    scene->setSceneRect(scene->sceneRect().x() + middleX, scene->sceneRect().y() - middleY, scene->sceneRect().width(), scene->sceneRect().height());
}

void parseDXF::drawSortPath()
{
    QPointF beginPoint, endPoint;
    pen.setStyle(Qt::DashDotDotLine);
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    double x01 = 0, y01 = 0, x02 = 0, y02 = 0;
    double line_bi = 0;
    QGraphicsLineItem *lineItem=nullptr;
    QString itemName;
    for (int i = 0; i < sortTask->sortedShapeMsg->size() - 1; i++){
        if (i > 0){
            QVector<double>vector = sortTask->allShapeVector[sortTask->sortedShapeMsg->at(i).shapePos];
            sortShapeTask::shapeType shape = static_cast<sortShapeTask::shapeType>(static_cast<int>(vector[0]));
            if (shape == sortTask->point) itemName = "POINT";
            else if (shape == sortTask->line) itemName = "LINE";
            else if (shape == sortTask->circle) itemName = "CIRCLE";
            else if (shape == sortTask->arc) itemName = "ARC";
            else if (shape == sortTask->ellipse) itemName = "ELLIPSE";
            ui->listWidget->addItem(QString::number(i) + "-" + itemName);
        }
        if (i == sortTask->sortedShapeMsg->size() - 2){
            QVector<double>vector = sortTask->allShapeVector[sortTask->sortedShapeMsg->at(i+1).shapePos];
            sortShapeTask::shapeType shape = static_cast<sortShapeTask::shapeType>(static_cast<int>(vector[0]));
            if (shape == sortTask->point) itemName = "POINT";
            else if (shape == sortTask->line) itemName = "LINE";
            else if (shape == sortTask->circle) itemName = "CIRCLE";
            else if (shape == sortTask->arc) itemName = "ARC";
            else if (shape == sortTask->ellipse) itemName = "ELLIPSE";
            ui->listWidget->addItem(QString::number(i + 1) + "-" + itemName);
        }

        if (i == 0)
            pen.setColor(Qt::yellow);
        else if (i == sortTask->sortedShapeMsg->size() - 2)
            pen.setColor(Qt::red);
        else
            pen.setColor(Qt::green);
        beginPoint=sortTask->sortedShapeMsg->at(i).endPoint[1];
        endPoint = sortTask->sortedShapeMsg->at(i+1).endPoint[0];
        x0 = beginPoint.x();
        y0 = beginPoint.y();
        x1 = endPoint.x();
        y1 = endPoint.y();

        lineItem = new QGraphicsLineItem(x0, -y0, x1, -y1);
        lineItem->setPen(pen);
        lineItemsVector.append(lineItem);
        scene->addItem(lineItem);

        if (qPow(x1 - x0, 2) + qPow(y1 - y0, 2) >= 0.1){
            x01 = (x0 - x1)*qCos(M_PI / 4) - (y0 - y1)*qSin(M_PI / 4) + x1;
            y01 = (y0 - y1)*qCos(M_PI / 4) + (x0 - x1)*qSin(M_PI / 4) + y1;
            x02 = (x0 - x1)*qCos(-M_PI / 4) - (y0 - y1)*qSin(-M_PI / 4) + x1;
            y02 = (y0 - y1)*qCos(-M_PI / 4) + (x0 - x1)*qSin(-M_PI / 4) + y1;
            line_bi = qSqrt(qPow(x0 - x1, 2) + qPow(y0 - y1, 2)) / (axisLength / 5 * qSqrt(2));
            x01 = x1 + 1 / line_bi*(x01 - x1);
            y01 = y1 + 1 / line_bi*(y01 - y1);
            x02 = x1 + 1 / line_bi*(x02 - x1);
            y02 = y1 + 1 / line_bi*(y02 - y1);

            lineItem = new QGraphicsLineItem(x1, -y1, x01, -y01);
            lineItem->setPen(pen);
            lineItemsVector.append(lineItem);
            scene->addItem(lineItem);

            lineItem = new QGraphicsLineItem(x1, -y1, x02, -y02);
            lineItem->setPen(pen);
            lineItemsVector.append(lineItem);
            scene->addItem(lineItem);
        }
    }
    lineItemsVector.squeeze();
    pen.setStyle(Qt::SolidLine);
    pen.setColor(Qt::white);
    if (sortThread->isRunning()){
        sortThread->exit();
        sortThread->wait();
    }
    delete progressshow;
    canOutput = true;
}

void parseDXF::updateProgress(const int value, const bool iscompleted)
{
    progressshow->updateProgressValue(value);
    if (iscompleted){
        delete progressshow;
        QMessageBox::information(this, "提示", "导出成功！");
    }
}

void parseDXF::listWidgetAddShape(const int shapePos)
{
    auto shapeType = static_cast<sortShapeTask::shapeType>(static_cast<int>(sortTask->allShapeVector[shapePos][0]));
    QString itemName;
    if (shapeType == sortTask->point) itemName = "POINT";
    else if (shapeType == sortTask->line) itemName = "LINE";
    else if (shapeType == sortTask->circle) itemName = "CIRCLE";
    else if (shapeType == sortTask->arc) itemName = "ARC";
    else if (shapeType == sortTask->ellipse) itemName = "ELLIPSE";
    shapeMsg beginPoint;
    if (sortTask->sortedShapeMsg->size() == 0){
        shapeMsg beginPoint;
        sortTask->sortedShapeMsg->append(beginPoint);
    }
    shapeMsg shapeMsgTmp;
    shapeMsgTmp.shapePos = shapePos;
    int row = ui->listWidget->currentRow();
    if (row == ui->listWidget->count() - 1){
        ui->listWidget->addItem(itemName);
        ui->listWidget->setCurrentRow(ui->listWidget->count() - 1);
        listClickedVector.append(ui->listWidget->count());
        sortTask->sortedShapeMsg->append(shapeMsgTmp);
    }
    else{
        ui->listWidget->insertItem(row + 1, itemName);
        ui->listWidget->setCurrentRow(row + 1);
        for (QVector<int>::iterator it = listClickedVector.begin(); it != listClickedVector.end(); it++){
            if (*it >= row + 2)
                *it += 1;
        }
        listClickedVector.append(row + 2);
        sortTask->sortedShapeMsg->append(shapeMsgTmp);
        sortTask->sortedShapeMsg->move(sortTask->sortedShapeMsg->size() - 1, row + 2);
    }
    sortTask->addHandSortMsg(*(sortTask->sortedShapeMsg->begin()+(row+2)), sortTask->sortedShapeMsg->at(row+1).endPoint[1]);
    drawArrow(row + 2);
}

void parseDXF::on_listWidget_clicked(const QModelIndex &index)
{
    if (!listClickedVector.contains(index.row() + 1)){
        listClickedVector.append(index.row() + 1);
        drawArrow(index.row() + 1);
    }
}

void parseDXF::on_listWidget_customContextMenuRequested(const QPoint &pos)
{
    if (ui->listWidget->itemAt(pos) != nullptr){
        QMenu menu;
        QAction act_turnDir("转向", this);
        QAction act_moveUp("上移", this);
        QAction act_moveDown("下移", this);
        QAction act_del("删除", this);
        QAction act_clear("clear", this);
        connect(&act_turnDir, &QAction::triggered, this, &parseDXF::turnDirShape);
        connect(&act_moveUp, &QAction::triggered, this, &parseDXF::moveUpShape);
        connect(&act_moveDown, &QAction::triggered, this, &parseDXF::moveDownShape);
        connect(&act_del, &QAction::triggered, this, &parseDXF::deleteShape);
        connect(&act_clear, &QAction::triggered, this, &parseDXF::clearListWidget);
        menu.addAction(&act_turnDir);
        menu.addAction(&act_moveUp);
        menu.addAction(&act_moveDown);
        menu.addAction(&act_del);
        menu.addAction(&act_clear);
        menu.exec(QCursor::pos());
    }
}

void parseDXF::turnDirShape()
{
    int row = ui->listWidget->currentRow();
    QVector<shapeMsg>::iterator it=sortTask->sortedShapeMsg->begin()+(row+1);
    if(it->isTurnDir) it->isTurnDir=false;
    else it->isTurnDir=true;
    drawArrow(row + 1);
}

void parseDXF::clearListWidget()
{
    int result = QMessageBox::question(this, "消息框", "是否确定清除当前排序?", QMessageBox::Yes | QMessageBox::No, QMessageBox::NoButton);
    if (result == QMessageBox::Yes){
        deleteExtraItem();
    }
}

void parseDXF::deleteShape()
{
    int result = QMessageBox::question(this, "消息框", "是否删除该图形?", QMessageBox::Yes | QMessageBox::No, QMessageBox::NoButton);
    if (result == QMessageBox::Yes){
        int i = ui->listWidget->currentRow() + 1;
        if (sortTask->sortedShapeMsg->at(i).arrow01IsShowing){
            scene->removeItem(sortTask->sortedShapeMsg->at(i).arrowItem[0]);
            scene->removeItem(sortTask->sortedShapeMsg->at(i).arrowItem[1]);
            (sortTask->sortedShapeMsg->begin()+i)->arrow01IsShowing = false;
        }
        if (sortTask->sortedShapeMsg->at(i).arrow23IsShowing){
            scene->removeItem(sortTask->sortedShapeMsg->at(i).arrowItem[2]);
            scene->removeItem(sortTask->sortedShapeMsg->at(i).arrowItem[3]);
            (sortTask->sortedShapeMsg->begin()+i)->arrow23IsShowing = false;
        }
        if (sortTask->sortedShapeMsg->at(i).itemHasColor){
            QGraphicsItem* item = findScenItemMap.value(sortTask->sortedShapeMsg->at(i).shapePos);
            changeItemColor(item, 0);
            (sortTask->sortedShapeMsg->begin()+i)->itemHasColor = false;
        }
        QVector<shapeMsg>::iterator it=sortTask->sortedShapeMsg->begin()+i;
        for(int j=0;j<4;j++){
            if(it->arrowItem[j]!=nullptr)
                delete it->arrowItem[j];
        }
        sortTask->sortedShapeMsg->removeAt(i);
        if (listClickedVector.contains(i))
            listClickedVector.removeOne(i);
        for (QVector<int>::iterator it = listClickedVector.begin(); it != listClickedVector.end(); it++){
            if (*it > i)
                *it -= 1;
        }
        QListWidgetItem* aItem = ui->listWidget->takeItem(ui->listWidget->currentRow()); //移除指定行的项，但不delete
        delete aItem;
    }
}

void parseDXF::moveUpShape()
{
    int row = ui->listWidget->currentRow();
    if(row<=0)  return;
    if (listClickedVector.contains(row + 1)){
        if (!listClickedVector.contains(row)){
            listClickedVector.append(row);
            listClickedVector.removeOne(row + 1);
        }
    }
    sortTask->sortedShapeMsg->move(row + 1, row);
    ui->listWidget->insertItem(row - 1, ui->listWidget->takeItem(row));
    ui->listWidget->setCurrentRow(row - 1);
    canOutput = false;
}

void parseDXF::moveDownShape()
{
    int row = ui->listWidget->currentRow();
    if (row<0 || row == ui->listWidget->count() - 1) return;
    if (listClickedVector.contains(row + 1)){
        if (!listClickedVector.contains(row + 2)){
            listClickedVector.append(row + 2);
            listClickedVector.remove(row + 1);
        }
    }
    sortTask->sortedShapeMsg->move(row + 1, row + 2);
    ui->listWidget->insertItem(row + 1, ui->listWidget->takeItem(row));
    ui->listWidget->setCurrentRow(row + 1);
    canOutput = false;
}

void parseDXF::refreshView()
{
    if (!lineItemsVector.isEmpty()){
        if (scene->items().contains(lineItemsVector[0])){
            foreach(QGraphicsLineItem *lineItem, lineItemsVector)
                scene->removeItem(lineItem);
        }
    }

    if(!listClickedVector.isEmpty()){
        foreach(int i, listClickedVector){
            if (sortTask->sortedShapeMsg->at(i).arrow01IsShowing){
                scene->removeItem(sortTask->sortedShapeMsg->at(i).arrowItem[0]);
                scene->removeItem(sortTask->sortedShapeMsg->at(i).arrowItem[1]);
                (sortTask->sortedShapeMsg->begin()+i)->arrow01IsShowing = false;
            }
            if (sortTask->sortedShapeMsg->at(i).arrow23IsShowing){
                scene->removeItem(sortTask->sortedShapeMsg->at(i).arrowItem[2]);
                scene->removeItem(sortTask->sortedShapeMsg->at(i).arrowItem[3]);
                (sortTask->sortedShapeMsg->begin()+i)->arrow23IsShowing = false;
            }
            if (sortTask->sortedShapeMsg->at(i).itemHasColor){
                QGraphicsItem* item = findScenItemMap.value(sortTask->sortedShapeMsg->at(i).shapePos);
                changeItemColor(item, 0);
                (sortTask->sortedShapeMsg->begin()+i)->itemHasColor = false;
            }
        }
        listClickedVector.clear();
    }
}

void parseDXF::deleteExtraItem(bool clearSortedShapeMsg)
{
    if (!lineItemsVector.isEmpty()){
        if (scene->items().contains(lineItemsVector[0])){
            foreach(QGraphicsLineItem *lineItem, lineItemsVector){
                scene->removeItem(lineItem);
                delete lineItem;
            }
        }
        lineItemsVector.clear();
    }

    if(!listClickedVector.isEmpty()){
        foreach(int i, listClickedVector){
            if (sortTask->sortedShapeMsg->at(i).arrow01IsShowing){
                scene->removeItem(sortTask->sortedShapeMsg->at(i).arrowItem[0]);
                scene->removeItem(sortTask->sortedShapeMsg->at(i).arrowItem[1]);
            }
            if (sortTask->sortedShapeMsg->at(i).arrow23IsShowing){
                scene->removeItem(sortTask->sortedShapeMsg->at(i).arrowItem[2]);
                scene->removeItem(sortTask->sortedShapeMsg->at(i).arrowItem[3]);
            }
            if (sortTask->sortedShapeMsg->at(i).itemHasColor){
                QGraphicsItem* item = findScenItemMap.value(sortTask->sortedShapeMsg->at(i).shapePos);
                changeItemColor(item, 0);
            }
        }
        listClickedVector.clear();
    }

    if(!sortTask->sortedShapeMsg->isEmpty()){
        foreach(shapeMsg shapeMsgTmp,*sortTask->sortedShapeMsg){
            for(int i=0;i<4;i++){
                if(shapeMsgTmp.arrowItem[i]!=nullptr)
                    delete shapeMsgTmp.arrowItem[i];
            }
        }
        if(clearSortedShapeMsg)
            sortTask->sortedShapeMsg->clear();
    }

    ui->listWidget->clear();
    canOutput = false;
}

void parseDXF::showSortPath()
{
    if (lineItemsVector.isEmpty()){
        QMessageBox::information(this, "提示", "未进行排序，无法显示路径！");
        return;
    }
    else if (!scene->items().contains(lineItemsVector[0])){
        foreach(QGraphicsLineItem *lineItem, lineItemsVector)
            scene->addItem(lineItem);
    }
    else
        QMessageBox::information(this, "提示", "路径已显示！");
}

void parseDXF::sortShape(const sortShapeTask::sortType type)
{
    if (sortTask->curLayShapesVector.isEmpty()){
        QMessageBox::information(this, "提示", "无图形进行排序!");
        return;
    }
    sortTask->sorttype = type;
    if (type == sortShapeTask::shapeSort || type == sortShapeTask::handSort){
        if (sortTask->sortedShapeMsg->isEmpty()){
            QMessageBox::information(this, "提示", "未选择起始图形，请双击选择起始图形进行排序!");
            return;
        }
        sortTask->startShapePos = sortTask->sortedShapeMsg->at(1).shapePos;
    }
    if (type == sortShapeTask::handSort)
        deleteExtraItem(false);
    else
        deleteExtraItem();
    progressshow = new progressShow(this);
    progressshow->initProgressBar(sortTask->curLayShapesVector.size(), "正在排序中...");
    sortThread->start();
}

void parseDXF::drawArrow(const int sortShapePos)
{
    pen.setColor(Qt::magenta);
    QGraphicsLineItem *lineItem0, *lineItem1;
    double x0=0, y0=0, x1=0, y1=0;
    double x01=0, y01=0, x02=0, y02=0;
    bool newItem = false;
    QVector<shapeMsg>::iterator it=sortTask->sortedShapeMsg->begin()+sortShapePos;
    QVector<double>vector = sortTask->allShapeVector[it->shapePos];
    bool isTurnDir = it->isTurnDir;
    sortShapeTask::shapeType shape = static_cast<sortShapeTask::shapeType>(static_cast<int>(vector[0]));
    if (shape == sortTask->point){
        if (it->arrowItem[0] == nullptr){
            x0 = vector[1];
            y0 = vector[2];
            lineItem0 = new QGraphicsLineItem(x0 - axisLength / 10, -y0, x0 + axisLength / 10, -y0);
            lineItem0->setPen(pen);
            lineItem1 = new QGraphicsLineItem(x0, -(y0 - axisLength / 10), x0, -(y0 + axisLength / 10));
            lineItem1->setPen(pen);
            (sortTask->sortedShapeMsg->begin()+sortShapePos)->arrowItem[0] = lineItem0;
            (sortTask->sortedShapeMsg->begin()+sortShapePos)->arrowItem[1] = lineItem1;
        }
        if (!it->arrow01IsShowing){
            scene->addItem((sortTask->sortedShapeMsg->begin()+sortShapePos)->arrowItem[0]);
            scene->addItem((sortTask->sortedShapeMsg->begin()+sortShapePos)->arrowItem[1]);
            (sortTask->sortedShapeMsg->begin()+sortShapePos)->arrow01IsShowing = true;
        }
        return;
    }
    else if (shape == sortTask->line){
        if (!isTurnDir){
            if (it->arrowItem[0] == nullptr){
                x0 = vector[1];
                y0 = vector[2];
                x1 = vector[3];
                y1 = vector[4];
                newItem = true;
            }
        }
        else{
            if (it->arrowItem[2] == nullptr){
                x1 = vector[1];
                y1 = vector[2];
                x0 = vector[3];
                y0 = vector[4];
                newItem = true;
            }
        }
        if (newItem){
            x01 = (x0 - x1)*qCos(M_PI / 4) - (y0 - y1)*qSin(M_PI / 4) + x1;
            y01 = (y0 - y1)*qCos(M_PI / 4) + (x0 - x1)*qSin(M_PI / 4) + y1;
            x02 = (x0 - x1)*qCos(-M_PI / 4) - (y0 - y1)*qSin(-M_PI / 4) + x1;
            y02 = (y0 - y1)*qCos(-M_PI / 4) + (x0 - x1)*qSin(-M_PI / 4) + y1;
            double line_bi = qSqrt(qPow(x0 - x1, 2) + qPow(y0 - y1, 2)) / (axisLength / 5 * qSqrt(2));
            x01 = x1 + 1 / line_bi*(x01 - x1);
            y01 = y1 + 1 / line_bi*(y01 - y1);
            x02 = x1 + 1 / line_bi*(x02 - x1);
            y02 = y1 + 1 / line_bi*(y02 - y1);
        }
    }
    else if (shape == sortTask->circle){
        x1 = vector[1] - vector[3];
        y1 = vector[2];
        if (!isTurnDir){
            if (it->arrowItem[0] == nullptr){
                x01 = x1 + axisLength / 5;
                y01 = y1 - axisLength / 5;
                x02 = x1 - axisLength / 5;
                y02 = y1 - axisLength / 5;
                newItem = true;
            }
        }
        else{
            if (it->arrowItem[2] == nullptr){
                x01 = x1 + axisLength / 5;
                y01 = y1 + axisLength / 5;
                x02 = x1 - axisLength / 5;
                y02 = y1 + axisLength / 5;
                newItem = true;
            }
        }
    }
    else if (shape == sortTask->arc){
        x0 = vector[1];
        y0 = vector[2];
        double r = vector[3];
        double angle_0 = vector[4], angle_1 = vector[5];
        if (!isTurnDir){
            if (it->arrowItem[0] == nullptr){
                x1 = x0 + r*qCos(angle_1*M_PI / 180);
                y1 = y0 + r*qSin(angle_1*M_PI / 180);
                x01 = (x0 - x1)*qCos(M_PI / 4) - (y0 - y1)*qSin(M_PI / 4) + x1;
                y01 = (y0 - y1)*qCos(M_PI / 4) + (x0 - x1)*qSin(M_PI / 4) + y1;
                x02 = (x0 - x1)*qCos(3 * M_PI / 4) - (y0 - y1)*qSin(3 * M_PI / 4) + x1;
                y02 = (y0 - y1)*qCos(3 * M_PI / 4) + (x0 - x1)*qSin(3 * M_PI / 4) + y1;
                newItem = true;
            }
        }
        else{
            if (it->arrowItem[2] == nullptr){
                x1 = x0 + r*qCos(angle_0*M_PI / 180);
                y1 = y0 + r*qSin(angle_0*M_PI / 180);
                x01 = (x0 - x1)*qCos(-M_PI / 4) - (y0 - y1)*qSin(-M_PI / 4) + x1;
                y01 = (y0 - y1)*qCos(-M_PI / 4) + (x0 - x1)*qSin(-M_PI / 4) + y1;
                x02 = (x0 - x1)*qCos(-3 * M_PI / 4) - (y0 - y1)*qSin(-3 * M_PI / 4) + x1;
                y02 = (y0 - y1)*qCos(-3 * M_PI / 4) + (x0 - x1)*qSin(-3 * M_PI / 4) + y1;
                newItem = true;
            }
        }
        if (newItem){
            double line_bi = qSqrt(qPow(x0 - x1, 2) + qPow(y0 - y1, 2)) / (axisLength / 5 * qSqrt(2));
            x01 = x1 + 1 / line_bi*(x01 - x1);
            y01 = y1 + 1 / line_bi*(y01 - y1);
            x02 = x1 + 1 / line_bi*(x02 - x1);
            y02 = y1 + 1 / line_bi*(y02 - y1);
        }
    }
    else if (shape == sortTask->ellipse){
        x0 = vector[1];
        y0 = vector[2];
        x1 = vector[3];
        y1 = vector[4];
        double r0 = qSqrt(qPow(vector[3], 2) + qPow(vector[4], 2));
        double bi = vector[5];
        double angle_0 = vector[6] * 180 / M_PI;
        double angle_1 = vector[7] * 180 / M_PI;
        double rotate_angle = qAtan2(y1, x1);
        double circle_x=0, circle_y=0;
        if (rotate_angle<0)
            rotate_angle = rotate_angle + M_PI * 2;
        if (!isTurnDir){
            if (it->arrowItem[0] == nullptr){
                circle_x = x0 + r0*qCos(angle_1*M_PI / 180);
                circle_y = y0 + r0*qSin(angle_1*M_PI / 180);
                x01 = (x0 - circle_x)*qCos(M_PI / 4) - (y0 - circle_y)*qSin(M_PI / 4) + circle_x;
                y01 = (y0 - circle_y)*qCos(M_PI / 4) + (x0 - circle_x)*qSin(M_PI / 4) + circle_y;
                x02 = (x0 - circle_x)*qCos(3 * M_PI / 4) - (y0 - circle_y)*qSin(3 * M_PI / 4) + circle_x;
                y02 = (y0 - circle_y)*qCos(3 * M_PI / 4) + (x0 - circle_x)*qSin(3 * M_PI / 4) + circle_y;
                newItem = true;
            }
        }
        else{
            if (it->arrowItem[2] == nullptr){
                circle_x = x0 + r0*qCos(angle_0*M_PI / 180);
                circle_y = y0 + r0*qSin(angle_0*M_PI / 180);
                x01 = (x0 - circle_x)*qCos(-M_PI / 4) - (y0 - circle_y)*qSin(-M_PI / 4) + circle_x;
                y01 = (y0 - circle_y)*qCos(-M_PI / 4) + (x0 - circle_x)*qSin(-M_PI / 4) + circle_y;
                x02 = (x0 - circle_x)*qCos(-3 * M_PI / 4) - (y0 - circle_y)*qSin(-3 * M_PI / 4) + circle_x;
                y02 = (y0 - circle_y)*qCos(-3 * M_PI / 4) + (x0 - circle_x)*qSin(-3 * M_PI / 4) + circle_y;
                newItem = true;
            }
        }
        if (newItem){
            double line_bi = r0 / (axisLength / 5 * qSqrt(2));
            x01 = circle_x + 1 / line_bi*(x01 - circle_x);
            y01 = circle_y + 1 / line_bi*(y01 - circle_y);
            x02 = circle_x + 1 / line_bi*(x02 - circle_x);
            y02 = circle_y + 1 / line_bi*(y02 - circle_y);
            double circle_y1 = bi*(circle_y - y0) + y0;
            y01 = y01 - (circle_y - circle_y1);
            y02 = y02 - (circle_y - circle_y1);
            double ell_x = (circle_x - x0)*qCos(rotate_angle) - (circle_y1 - y0)*qSin(rotate_angle) + x0;
            double ell_y = (circle_y1 - y0)*qCos(rotate_angle) + (circle_x - x0)*qSin(rotate_angle) + y0;
            double ell_x01 = (x01 - x0)*qCos(rotate_angle) - (y01 - y0)*qSin(rotate_angle) + x0;
            double ell_y01 = (y01 - y0)*qCos(rotate_angle) + (x01 - x0)*qSin(rotate_angle) + y0;
            double ell_x02 = (x02 - x0)*qCos(rotate_angle) - (y02 - y0)*qSin(rotate_angle) + x0;
            double ell_y02 = (y02 - y0)*qCos(rotate_angle) + (x02 - x0)*qSin(rotate_angle) + y0;
            x1 = ell_x;
            y1 = ell_y;
            x01 = ell_x01;
            y01 = ell_y01;
            x02 = ell_x02;
            y02 = ell_y02;
        }
    }
    if (newItem){
        lineItem0 = new QGraphicsLineItem(x1, -y1, x01, -y01);
        lineItem0->setPen(pen);
        lineItem1 = new QGraphicsLineItem(x1, -y1, x02, -y02);
        lineItem1->setPen(pen);
        if (!isTurnDir){
            (sortTask->sortedShapeMsg->begin()+sortShapePos)->arrowItem[0] = lineItem0;
            (sortTask->sortedShapeMsg->begin()+sortShapePos)->arrowItem[1] = lineItem1;
        }
        else{
            (sortTask->sortedShapeMsg->begin()+sortShapePos)->arrowItem[2] = lineItem0;
            (sortTask->sortedShapeMsg->begin()+sortShapePos)->arrowItem[3] = lineItem1;
        }
    }

    if (!isTurnDir){
        if (!it->arrow01IsShowing){
            scene->addItem(it->arrowItem[0]);
            scene->addItem(it->arrowItem[1]);
            (sortTask->sortedShapeMsg->begin()+sortShapePos)->arrow01IsShowing = true;
        }
        if (it->arrow23IsShowing){
            scene->removeItem(it->arrowItem[2]);
            scene->removeItem(it->arrowItem[3]);
            (sortTask->sortedShapeMsg->begin()+sortShapePos)->arrow23IsShowing = false;
        }
    }
    else{
        if (!it->arrow23IsShowing){
            scene->addItem(it->arrowItem[2]);
            scene->addItem(it->arrowItem[3]);
            it->arrow23IsShowing = true;
        }
        if (it->arrow01IsShowing){
            scene->removeItem(it->arrowItem[0]);
            scene->removeItem(it->arrowItem[1]);
            it->arrow01IsShowing = false;
        }
    }
    pen.setColor(Qt::white);
    QGraphicsItem* item = findScenItemMap.value(it->shapePos);
    changeItemColor(item, 1);
    it->itemHasColor = true;
}
