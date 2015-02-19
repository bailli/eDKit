#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtGui/QListWidgetItem>

#define BASE_ROM "base.gb"

//"Sprite 0x3a"
//"Sprite 0x44"
//"Sprite 0x47"
//"Sprite 0x48"
//"Sprite 0x4d"
//"Sprite 0x4e"
//"Sprite 0x4f"
//"Sprite 0x50"
//"Sprite 0x54"
//"Sprite 0x57"
//"Sprite 0x58"
//"Sprite 0x5a"
//"Sprite 0x5c"
//"Sprite 0x5e"
//"Sprite 0x64"
//"Sprite 0x6e"
//"Sprite 0x70"
//"Sprite 0x72"
//"Sprite 0x7a"
//"Sprite 0x7c"
//"Sprite 0x7f"
//"Sprite 0x80"
//"Sprite 0x84"
//"Sprite 0x86"
//"Sprite 0x88"
//"Sprite 0x8a"
//"Sprite 0x8e"
//"Sprite 0x90"
//"Sprite 0x92"
//"Sprite 0x94"
//"Sprite 0x96"
//"Sprite 0x98"
//"Sprite 0x9a"
//"Sprite 0x9d"
//"Sprite 0xa2"
//"Sprite 0xa4"
//"Sprite 0xa6"
//"Sprite 0xa8"
//"Sprite 0xaa"
//"Sprite 0xac"
//"Sprite 0xae"
//"Sprite 0xb0"
//"Sprite 0xb6"
//"Sprite 0xb8"
//"Sprite 0xba"
//"Sprite 0xbe"
//"Sprite 0xc0"
//"Sprite 0xc2"
//"Sprite 0xc6"
//"Sprite 0xc8"
//"Sprite 0xca"
//"Sprite 0xcc"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private:
    void spriteContextMenu(QListWidgetItem *item, QPoint globalPos);
    
private slots:
    void updateText();

    void changeLevel(int id);
    void loadROM();
    void SaveROM();
    void selectSprite(int num);
    void addSprite(QString text, int id);
    void removeSprite(int index);
    void addNewSprite(QAction *action);
    void on_lstSprites_customContextMenuRequested(const QPoint &pos);

    void on_lstSprites_itemDoubleClicked(QListWidgetItem *item);

    void on_lvlEdit_customContextMenuRequested(const QPoint &pos);

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
