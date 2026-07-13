#include "src/ui/mainwindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/assets/app/app_icon.png"));
    MainWindow w;
    w.setWindowIcon(QIcon(":/assets/app/app_icon.png"));
    w.show();
    return a.exec();
}
