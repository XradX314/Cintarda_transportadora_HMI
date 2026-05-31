#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("HMI Cinta Transportadora");
    app.setOrganizationName("TP4-Microcontroladores");

    MainWindow w;
    w.show();
    return app.exec();
}
