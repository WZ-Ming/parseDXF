#include "parsedxf.h"
#include <QApplication>
#include<QFontDatabase>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    parseDXF w;
    w.showMaximized();
    //w.show();
    return a.exec();
}
