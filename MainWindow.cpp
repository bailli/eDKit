#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QtCore/QFile>
#include <QtCore/QDataStream>
#include <QtCore/QDebug>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->lvlEdit->setupTileSelector(ui->tileSelect, 2.0f, 256);

    ui->actionOpen_ROM->setShortcut(QKeySequence::Open);
    ui->actionSave_ROM->setShortcut(QKeySequence::Save);

    connect(ui->lvlEdit, SIGNAL(dataChanged()), this, SLOT(updateText()));
    connect(ui->tabWidget, SIGNAL(currentChanged(int)), ui->lvlEdit, SLOT(toggleSpriteMode(int)));
    connect(ui->btnWrite, SIGNAL(clicked()), ui->lvlEdit, SLOT(saveLevel()));
    connect(ui->actionOpen_ROM, SIGNAL(triggered()), this, SLOT(loadROM()));
    connect(ui->actionSave_ROM, SIGNAL(triggered()), this, SLOT(SaveROM()));

    connect(ui->lvlEdit, SIGNAL(musicChanged(int)), ui->cmbMusic, SLOT(setCurrentIndex(int)));
    connect(ui->lvlEdit, SIGNAL(paletteChanged(int)), ui->spbPalette, SLOT(setValue(int)));
    connect(ui->lvlEdit, SIGNAL(sizeChanged(int)), ui->cmbSize, SLOT(setCurrentIndex(int)));
    connect(ui->lvlEdit, SIGNAL(tilesetChanged(int)), ui->cmbTileset, SLOT(setCurrentIndex(int)));
    connect(ui->lvlEdit, SIGNAL(timeChanged(int)), ui->spbTime, SLOT(setValue(int)));

    //connect(ui->spbLevel, SIGNAL(valueChanged(int)), ui->lvlEdit, SLOT(changeLevel(int)));
    connect(ui->spbLevel, SIGNAL(valueChanged(int)), this, SLOT(changeLevel(int)));
    connect(ui->cmbSize, SIGNAL(currentIndexChanged(int)), ui->lvlEdit, SLOT(changeSize(int)));
    connect(ui->cmbMusic, SIGNAL(currentIndexChanged(int)), ui->lvlEdit, SLOT(changeMusic(int)));
    connect(ui->cmbTileset, SIGNAL(currentIndexChanged(int)), ui->lvlEdit, SLOT(changeTileset(int)));
    connect(ui->spbPalette, SIGNAL(valueChanged(int)), ui->lvlEdit, SLOT(changePalette(int)));
    connect(ui->spbTime, SIGNAL(valueChanged(int)), ui->lvlEdit, SLOT(changeTime(int)));

    connect(ui->ckbTransparency, SIGNAL(clicked(bool)), ui->lvlEdit, SLOT(changeSpriteTransparency(bool)));
    connect(ui->lvlEdit, SIGNAL(spriteSelected(int)), this, SLOT(selectSprite(int)));
    connect(ui->lvlEdit, SIGNAL(spriteAdded(QString, int)), this, SLOT(addSprite(QString, int)));
    connect(ui->lvlEdit, SIGNAL(spriteRemoved(int)), this, SLOT(removeSprite(int)));

    if ((qApp->arguments().size() > 1) && (QFile::exists(qApp->arguments().at(1))))
    {
        ui->lvlEdit->loadAllLevels(qApp->arguments().at(1));
        ui->lvlEdit->changeLevel(0);
        ui->lvlInfo->setPlainText(ui->lvlEdit->getLevelInfo());
    }
    else
    {
        ui->lvlEdit->loadAllLevels(BASE_ROM);
        ui->lvlEdit->changeLevel(0);
        ui->lvlInfo->setPlainText(ui->lvlEdit->getLevelInfo());
    }
    ui->tabWidget->setCurrentIndex(0);
    ui->spbLevel->setFocus();


    QMenu *newSpriteMenu = new QMenu(this);
    QDir dir("sprites/");
    QStringList list = dir.entryList(QStringList("*.png"), QDir::Files, QDir::Name);

    QAction *action;
    for (int i = 0; i < list.size(); i++)
    {
        action = new QAction(QIcon("sprites/" + list.at(i)), ui->lvlEdit->spriteNumToString(list.at(i).mid(7,2).toInt(0, 16)), newSpriteMenu);
        action->setStatusTip(list.at(i).mid(7,2));
        newSpriteMenu->addAction(action);
        if (list.at(i).contains("set", Qt::CaseInsensitive))
            i+=0x21;
    }

    ui->toolButton->setMenu(newSpriteMenu);
    connect(newSpriteMenu, SIGNAL(triggered(QAction*)), this, SLOT(addNewSprite(QAction*)));
}

void MainWindow::selectSprite(int num)
{
    ui->lstSprites->setCurrentRow(num);
}

void MainWindow::addSprite(QString text, int id)
{
    QString icon = QString("sprites/sprite_%1.png").arg(id, 2, 16, QChar('0'));
    if (!QFile::exists(icon))
        icon = QString("sprites/sprite_%1_set_00.png").arg(id, 2, 16, QChar('0'));
    QListWidgetItem *item = new QListWidgetItem(QIcon(icon), text);
    item->setToolTip(text);
    item->setStatusTip(QString("%1").arg(id, 2, 16, QChar('0')));
    ui->lstSprites->addItem(item);
}

void MainWindow::removeSprite(int index)
{
    QListWidgetItem *tmp = ui->lstSprites->takeItem(index);
    delete tmp;
}


void MainWindow::changeLevel(int id)
{
    if (ui->lvlEdit->isChanged())
    {
        QMessageBox::StandardButton result = QMessageBox::question(NULL, "Level data changed", "The level data has been changed. Save data?", QMessageBox::Yes | QMessageBox::No);
        if (result == QMessageBox::Yes) // save changes
            ui->lvlEdit->saveLevel();
        else if (result != QMessageBox::No) // discard changes
            qWarning() << "Unexpected return value form messagebox!";
    }
    ui->lvlEdit->changeLevel(id);
}

void MainWindow::updateText()
{
    ui->lvlInfo->setPlainText(ui->lvlEdit->getLevelInfo());
}

void MainWindow::loadROM()
{
    QString file = QFileDialog::getOpenFileName(0, "Select Donkey Kong (GB) ROM", qApp->applicationDirPath(), "Donkey Kong (GB) ROM (*.gb)");

    if (!QFile::exists(file))
        return;

    ui->lvlEdit->loadAllLevels(file);
    ui->lvlEdit->changeLevel(0);
    ui->lvlInfo->setPlainText(ui->lvlEdit->getLevelInfo());
}

void MainWindow::SaveROM()
{
    if (ui->lvlEdit->isChanged())
    {
        QMessageBox::StandardButton result = QMessageBox::question(NULL, "Level data changed", "The level data has been changed. Save data?", QMessageBox::Yes | QMessageBox::No);
        if (result == QMessageBox::Yes) // save changes
            ui->lvlEdit->saveLevel();
        else if (result != QMessageBox::No) // discard changes
            qWarning() << "Unexpected return value form messagebox!";
    }

    QString file = QFileDialog::getSaveFileName(0, "Select Donkey Kong (GB) ROM", qApp->applicationDirPath(), "Donkey Kong (GB) ROM (*.gb)");

    if (!QFile::exists(file))
        QFile::copy(qApp->applicationDirPath() + BASE_ROM, file);

    ui->lvlEdit->saveAllLevels(file);
}


void MainWindow::addNewSprite(QAction *action)
{
    ui->lvlEdit->addSprite(action->statusTip().toInt(0, 16));
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_lstSprites_itemDoubleClicked(QListWidgetItem *item)
{
    quint8 flag;
    ui->lvlEdit->getSpriteFlag(ui->lstSprites->row(item), &flag);
    qDebug() << flag;
}

void MainWindow::spriteContextMenu(QListWidgetItem *item, QPoint globalPos)
{
    quint8 flag;
    quint8 spriteID = item->statusTip().toInt(0, 16);
    ui->lvlEdit->getSpriteFlag(ui->lstSprites->row(item), &flag);

    QMenu *menu = new QMenu();

    //sprites that may get flipped
    if ((spriteID == 0x7F) || (spriteID == 0x98) || (spriteID == 0x80))
        menu->addAction("Flip sprite");

    //sprites that may get flipped but should display another text ;)
    if (spriteID == 0x84)
    {
        if (flag & 1)
            menu->addAction("Flip direction (left)");
        else
            menu->addAction("Flip direction (right)");
    }

    //sprites with walking speed property
    if ((spriteID == 0x80) || (spriteID == 0x98) || (spriteID == 0x84))
        menu->addAction(QString("Change speed (%1)").arg(flag >> 1));

    //elevator time between boards and speed
    //table @ 0x30F77
    //flag byte selects pair from table
    // 0x40 0x20 ; 0x60 0x20 ; 0x80 0x20 ; 0xA0 0x20
    // 0x40 0x40 ; 0x60 0x40 ; 0x80 0x40 ; 0xA0 0x40
    // 0x40 0x60 ; 0x60 0x60 ; 0x80 0x60 ; 0xA0 0x60
    // 0x40 0x80 ; 0x60 0x80 ; 0x80 0x80 ; 0xA0 0x80
    // 16 settings
    // default is 0x60 0x40 => 0x05

    // first byte is speed; second byte time between new boards
    if ((spriteID == 0x70) || (spriteID == 0x72))
    {
        quint8 time, speed;
        speed = (flag % 4) * 0x20 + 0x40;
        time = (flag / 4) * 0x20 + 0x20;
        menu->addAction(QString("Change elevator setting (speed %1; time %2)").arg(speed, 2, 16, QChar('0')).arg(time, 2, 16, QChar('0')));
    }

    //board speed
    if (spriteID == 0x54)
    {
        if (!flag)
            menu->addAction(QString("Switch speed (normal)"));
        else
            menu->addAction(QString("Switch speed (slow)"));
    }

    if ((spriteID == 0xB8) || (spriteID == 0x6E) || (spriteID == 0x9A) || (spriteID == 0xCC))
        menu->addAction(QString("Edit flag byte (%1)").arg(flag));

    menu->addAction("Delete sprite");

    if (menu->actions().size() > 0)
    {
        QAction *selected = menu->exec(globalPos);
        if (selected != NULL)
        {
            if (selected->text().startsWith("Flip"))
            {
                if (spriteID == 0x7F)
                {
                    if (flag == 0x00)
                        flag = 0x01;
                    else
                        flag = 0x00;
                }

                if ((spriteID == 0x80) || (spriteID == 0x98) || (spriteID == 0x84))
                {
                    if ((flag & 1) == 0x01)
                        flag ^= 1;
                    else
                        flag |= 1;
                }
            }
            else if (selected->text().startsWith("Change speed"))
            {
                quint8 newSpeed = QInputDialog::getInt(this, "Sprite property", "Running speed:", (flag >> 1), 0, 127);
                flag = (newSpeed * 2) | (flag & 1);
            }
            else if (selected->text().startsWith("Switch speed"))
            {
                if (flag)
                    flag = 0;
                else
                    flag = 1;
            }
            else if (selected->text().startsWith("Change elevator"))
            {
                flag = QInputDialog::getInt(this, "Sprite property", "Elevator setting:", flag, 0, 15);
            }
            else if (selected->text().startsWith("Delete sprite"))
            {
                ui->lvlEdit->deleteSprite(ui->lstSprites->row(item));
                return;
            }
            else if (selected->text().startsWith("Edit"))
            {
                flag = QInputDialog::getInt(this, "Sprite property", "Key hole flag byte:", flag, 0, 255);
            }
            else
                qWarning() << QString("Unhandled context menu selection: %1").arg(selected->text());

            ui->lvlEdit->setSpriteFlag(ui->lstSprites->row(item), flag);
        }
    }
}

void MainWindow::on_lstSprites_customContextMenuRequested(const QPoint &pos)
{
    QListWidgetItem *item = ui->lstSprites->itemAt(pos);
    if (item == NULL)
        return;

    spriteContextMenu(item, ui->lstSprites->mapToGlobal(pos));
}

void MainWindow::on_lvlEdit_customContextMenuRequested(const QPoint &pos)
{
    QListWidgetItem *item = ui->lstSprites->currentItem();
    if (item == NULL)
        return;

    spriteContextMenu(item, ui->lvlEdit->mapToGlobal(pos));
}
