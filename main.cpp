#include <QtGui/QApplication>
#include <QtGui/QMessageBox>
#include <QtCore/QFile>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    if (QFile::exists(qApp->applicationDirPath() + BASE_ROM))
    {
        QMessageBox::warning(NULL, "Error", "You need a base.gb in the editor's directory!");
        return 1;
    }

    MainWindow w;
    w.show();
    
    return a.exec();
}
