#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>

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
