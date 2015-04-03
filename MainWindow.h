#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>
#include <QTreeWidgetItem>
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
    void addSwitchAtPos(int i, QDKSwitch *sw);
    
private slots:
    void updateText();

    void changeLevel(int id);
    void loadROM();
    void SaveROM();
    void ExportLvl();
    void ImportLvl();
    void enableSaveBtn();
    void selectSprite(int num);
    void addSprite(QString text, int id);
    void removeSelectedSprite();
    void removeSprite(int index);
    void addNewSprite(QAction *action);
    void addSwitch(QDKSwitch *sw);
    void updateSwitch(int i, QDKSwitch *sw);
    void removeSwitch(int i);
    void tabsSwitched(int index);

    void on_btnFlip_clicked();
    void on_spbSpeed_valueChanged(int arg1);
    void on_cmbElevator_currentIndexChanged(int index);
    void on_spbRAW_valueChanged(int arg1);
    void on_treSwitches_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *);
    void delSwitchItem();

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
