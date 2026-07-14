#include "src/ui/mainwindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setOrganizationName(QStringLiteral("Gomoku"));
    a.setApplicationName(QStringLiteral("Gomoku"));
    a.setWindowIcon(QIcon(":/assets/icons/app_icon.png"));
    MainWindow w;
    w.setWindowIcon(QIcon(":/assets/icons/app_icon.png"));
    w.show();
    return a.exec();
}
