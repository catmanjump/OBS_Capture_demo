#include "cpswidget.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    cpswidget w;
    w.show();
    return a.exec();
}
