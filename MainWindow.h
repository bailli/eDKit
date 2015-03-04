#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>
#include <QAbstractButton>

#define BASE_ROM "base.gb"

struct QDKSwitch;

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
//    void spriteContextMenu(QListWidgetItem *item, QPoint globalPos);
    
private slots:
    void updateText();

    void changeLevel(int id);
    void loadROM();
    void SaveROM();
    void selectSprite(int num);
    void addSprite(QString text, int id);
    void removeSelectedSprite();
    void removeSprite(int index);
    void addNewSprite(QAction *action);
    void addSwitch(QDKSwitch *sw);
    void removeSwitch(int i);

    void on_btnFlip_clicked();
    void on_spbSpeed_valueChanged(int arg1);
    void on_cmbElevator_currentIndexChanged(int index);
    void on_spbRAW_valueChanged(int arg1);
    void on_treSwitches_customContextMenuRequested(const QPoint &pos);
    void on_bgrLeverPos_buttonClicked(QAbstractButton * button);
    void on_bgrLeverPos_buttonClicked(int id);
//    void on_lstSprites_customContextMenuRequested(const QPoint &pos);
//    void on_lstSprites_itemDoubleClicked(QListWidgetItem *item);
//    void on_lvlEdit_customContextMenuRequested(const QPoint &pos);

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
