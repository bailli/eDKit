#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#define BASE_ROM "base.gb"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    
private slots:
    void updateText();

    void changeLevel(int id);
    void loadROM();
    void SaveROM();
    void selectSprite(int num);
    void addSprite(QString sprite);
    void removeSprite(int index);

    void addNewSprite(QAction *action);

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
