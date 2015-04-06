#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QtCore/QFile>
#include <QtCore/QDataStream>
#include <QtCore/QDebug>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QShortcut>
#include <QProgressBar>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->lvlEdit->setupTileSelector(ui->tileSelect, 2.0f, 256);

    ui->actionOpen_ROM->setShortcut(QKeySequence::Open);
    ui->actionSave_ROM->setShortcut(QKeySequence::Save);
    ui->actionUndo->setShortcut(Qt::CTRL + Qt::Key_U);

    connect(ui->lvlEdit, SIGNAL(dataChanged()), this, SLOT(updateText()));
    connect(ui->tabWidget, SIGNAL(currentChanged(int)), ui->lvlEdit, SLOT(toggleSpriteMode(int)));
    connect(ui->tabWidget, SIGNAL(currentChanged(int)), this, SLOT(tabsSwitched(int)));
    connect(ui->btnWrite, SIGNAL(clicked()), ui->lvlEdit, SLOT(saveLevel()));
    connect(ui->actionOpen_ROM, SIGNAL(triggered()), this, SLOT(loadROM()));
    connect(ui->actionSave_ROM, SIGNAL(triggered()), this, SLOT(SaveROM()));
    connect(ui->actionUndo, SIGNAL(triggered()), ui->lvlEdit, SLOT(undo()));
    connect(ui->actionEmpty_Level, SIGNAL(triggered()), ui->lvlEdit, SLOT(clearLevel()));
    connect(ui->actionExportLvl, SIGNAL(triggered()), this, SLOT(ExportLvl()));
    connect(ui->actionImportLvl, SIGNAL(triggered()), this, SLOT(ImportLvl()));

    connect(ui->lvlEdit, SIGNAL(musicChanged(int)), ui->cmbMusic, SLOT(setCurrentIndex(int)));
    connect(ui->lvlEdit, SIGNAL(paletteChanged(int)), ui->spbPalette, SLOT(setValue(int)));
    connect(ui->lvlEdit, SIGNAL(sizeChanged(int)), ui->cmbSize, SLOT(setCurrentIndex(int)));
    connect(ui->lvlEdit, SIGNAL(tilesetChanged(int)), ui->cmbTileset, SLOT(setCurrentIndex(int)));
    connect(ui->lvlEdit, SIGNAL(timeChanged(int)), ui->spbTime, SLOT(setValue(int)));
    connect(ui->lvlEdit, SIGNAL(dataChanged()), this, SLOT(enableSaveBtn()));

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

    connect(ui->lvlEdit, SIGNAL(switchAdded(QDKSwitch*)), this, SLOT(addSwitch(QDKSwitch*)));
    connect(ui->lvlEdit, SIGNAL(switchUpdated(int,QDKSwitch*)), this, SLOT(updateSwitch(int,QDKSwitch*)));
    connect(ui->lvlEdit, SIGNAL(switchRemoved(int)), this, SLOT(removeSwitch(int)));

    connect(ui->lvlEdit, SIGNAL(tilesVRAMchanged(int)), this, SLOT(updateVRAMtiles(int)));
    connect(ui->lvlEdit, SIGNAL(spriteVRAMchanged(int)), this, SLOT(updateVRAMsprites(int)));

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
    QPixmap *pix;
    QPixmap pixLB(32, 32);
    qreal ar;

    for (int i = 0; i < list.size(); i++)
    {
        pix = new QPixmap("sprites/" + list.at(i));
        pixLB.fill();

        ar = (qreal)pix->width() / (qreal)pix->height();

        if (ar >= 1.0f)
        {
            QPainter painter;
            painter.begin(&pixLB);
            painter.drawPixmap(0, 16 - (16.0f/ar), pix->scaled(QSize(32, 32), Qt::KeepAspectRatio));
            painter.end();
        }
        else
        {
            QPainter painter;
            painter.begin(&pixLB);
            painter.drawPixmap(16 - (16.0f*ar), 0, pix->scaled(QSize(32, 32), Qt::KeepAspectRatio));
            painter.end();
        }
        delete pix;

        action = new QAction(QIcon(pixLB), ui->lvlEdit->spriteNumToString(list.at(i).mid(7,2).toInt(0, 16)), newSpriteMenu);
        action->setStatusTip(list.at(i).mid(7,2));
        newSpriteMenu->addAction(action);
        if (list.at(i).contains("set", Qt::CaseInsensitive))
            i+=0x21;
    }

    ui->toolButton->setMenu(newSpriteMenu);
    connect(newSpriteMenu, SIGNAL(triggered(QAction*)), this, SLOT(addNewSprite(QAction*)));

    QShortcut* scSprites = new QShortcut(QKeySequence(Qt::Key_Delete), ui->lstSprites);
    connect(scSprites, SIGNAL(activated()), this, SLOT(removeSelectedSprite()));

    QShortcut* scSwitches = new QShortcut(QKeySequence(Qt::Key_Delete), ui->treSwitches);
    connect(scSwitches, SIGNAL(activated()), this, SLOT(delSwitchItem()));

    changeLevel(0);
}

void MainWindow::tabsSwitched(int index)
{
    ui->lvlEdit->toggleSwitchMode((bool)(index > 1));

    if (index > 1)
        on_treSwitches_currentItemChanged(ui->treSwitches->currentItem(), NULL);
}

void MainWindow::removeSelectedSprite()
{
    int i = ui->lstSprites->currentRow();
    if (i != -1)
        ui->lvlEdit->deleteSprite(i);
}

void MainWindow::selectSprite(int num)
{
    ui->lstSprites->setCurrentRow(num);
    ui->grpSpriteProp->setEnabled(false);
    if (num == -1)
        return;

    int spriteID;
    int selSprite = ui->lvlEdit->getSelectedSprite(&spriteID);
    if (selSprite == -1)
        return;

    if ((spriteID != 0x7F) && (spriteID != 0x98) && (spriteID != 0x80) && (spriteID != 0x80)
          && (spriteID != 0x84) && (spriteID != 0x70) && (spriteID != 0x72) && (spriteID != 0x54)
          && (spriteID != 0xB8) && (spriteID != 0x6E) && (spriteID != 0x9A) && (spriteID != 0xCC))
        return;

    quint8 flag;
    ui->lvlEdit->getSpriteFlag(selSprite, &flag);

    ui->grpSpriteProp->setEnabled(true);
    ui->btnFlip->setEnabled(false);
    ui->spbSpeed->setEnabled(false);
    ui->cmbElevator->setEnabled(false);
    ui->spbRAW->setEnabled(false);
    ui->spbRAW->setValue(flag);

    //sprites that may get flipped
    if ((spriteID == 0x7F) || (spriteID == 0x98) || (spriteID == 0x80))
    {
        ui->btnFlip->setEnabled(true);
        ui->btnFlip->setText("Flip");
        ui->lblDirection->setText("Direction");
    }

    //sprites that may get flipped but should display another text ;)
    if (spriteID == 0x84)
    {
        ui->btnFlip->setEnabled(true);
        ui->lblDirection->setText("Direction");
        if (flag & 1)
            ui->btnFlip->setText("Left");
        else
            ui->btnFlip->setText("Right");
    }

    //sprites with walking speed property
    if ((spriteID == 0x80) || (spriteID == 0x98) || (spriteID == 0x84))
    {
        ui->spbSpeed->setEnabled(true);
        ui->spbSpeed->setValue(flag >> 1);
    }

    //elevator time between boards and speed
    //table @ 0x30F77
    //flag byte selects pair from table
    // 0x40 0x20 ; 0x60 0x20 ; 0x80 0x20 ; 0xA0 0x20
    // 0x40 0x40 ; 0x60 0x40 ; 0x80 0x40 ; 0xA0 0x40
    // 0x40 0x60 ; 0x60 0x60 ; 0x80 0x60 ; 0xA0 0x60
    // 0x40 0x80 ; 0x60 0x80 ; 0x80 0x80 ; 0xA0 0x80
    // 16 settings
    // default is 0x60 0x40 => 0x05
    //speed = (flag % 4) * 0x20 + 0x40;
    //time = (flag / 4) * 0x20 + 0x20;

    // first byte is speed; second byte time between new boards
    if ((spriteID == 0x70) || (spriteID == 0x72))
    {
        ui->cmbElevator->setEnabled(true);
        if ((flag >= 0) && (flag < 16))
            ui->cmbElevator->setCurrentIndex(flag);
        else
            ui->cmbElevator->setCurrentIndex(5);
    }

    //board speed
    if (spriteID == 0x54)
    {
        ui->btnFlip->setEnabled(true);
        ui->lblDirection->setText("Speed");
        if (!flag)
            ui->btnFlip->setText("Normal");
        else
            ui->btnFlip->setText("Slow");
    }

    if ((spriteID == 0xB8) || (spriteID == 0x6E) || (spriteID == 0x9A) || (spriteID == 0xCC))
    {
        ui->spbRAW->setEnabled(true);
        ui->spbRAW->setValue(flag);
    }
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
    ui->btnWrite->setEnabled(false);
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

void MainWindow::ExportLvl()
{
    if (ui->lvlEdit->isChanged())
    {
        QMessageBox::StandardButton result = QMessageBox::question(NULL, "Level data changed", "The level data has been changed. Save data?", QMessageBox::Yes | QMessageBox::No);
        if (result == QMessageBox::Yes) // save changes
            ui->lvlEdit->saveLevel();
        else if (result != QMessageBox::No) // discard changes
            qWarning() << "Unexpected return value form messagebox!";
    }

    QString file = QFileDialog::getSaveFileName(0, "Export level data", qApp->applicationDirPath(), "eDKit level data (*.lvl)");

    if (!ui->lvlEdit->exportCurrentLevel(file))
        qWarning() << "Level export failed";
}

void MainWindow::ImportLvl()
{
    QString file = QFileDialog::getOpenFileName(0, "Import level data", qApp->applicationDirPath(), "eDKit level data (*.lvl)");

    if (!QFile::exists(file))
        return;
    ui->lvlEdit->importLevel(file);
}

void MainWindow::addNewSprite(QAction *action)
{
    ui->lvlEdit->addSprite(action->statusTip().toInt(0, 16));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_btnFlip_clicked()
{
    int spriteID;
    int selSprite = ui->lvlEdit->getSelectedSprite(&spriteID);
    if (selSprite == -1)
        return;

    quint8 flag;
    ui->lvlEdit->getSpriteFlag(selSprite, &flag);

    // Mario
    if (spriteID == 0x7F)
    {
        if (flag == 0x00)
            flag = 0x01;
        else
            flag = 0x00;
    }

    //walking guys
    if ((spriteID == 0x80) || (spriteID == 0x98))
    {
        if ((flag & 1) == 0x01)
            flag ^= 1;
        else
            flag |= 1;

   }

    //sprites that may get flipped but should display another text ;)
    if (spriteID == 0x84)
    {
        if (flag & 1)
        {
            ui->btnFlip->setText("Right");
            flag ^= 1;
        }
        else
        {
            ui->btnFlip->setText("Left");
            flag |= 1;
        }
    }

    //board speed
    if (spriteID == 0x54)
    {
        if (flag)
        {
            flag = 0;
            ui->btnFlip->setText("Normal");
        }
        else
        {
            flag = 1;
            ui->btnFlip->setText("Slow");
        }
    }

    ui->lvlEdit->setSpriteFlag(selSprite, flag);
    ui->spbRAW->setValue(flag);
}

void MainWindow::on_spbSpeed_valueChanged(int arg1)
{
    if ((arg1 < 0) || (arg1 > 127))
        return;

    int spriteID;
    int selSprite = ui->lvlEdit->getSelectedSprite(&spriteID);
    if (selSprite == -1)
        return;

    quint8 flag;
    ui->lvlEdit->getSpriteFlag(selSprite, &flag);

    //sprites with walking speed property
    if ((spriteID == 0x80) || (spriteID == 0x98) || (spriteID == 0x84))
    {
        flag = (arg1 * 2) | (flag & 1);
        ui->lvlEdit->setSpriteFlag(selSprite, flag);
        ui->spbRAW->setValue(flag);
    }
}

void MainWindow::on_cmbElevator_currentIndexChanged(int index)
{
    if ((index < 0) || (index > 15))
        return;

    int spriteID;
    int selSprite = ui->lvlEdit->getSelectedSprite(&spriteID);
    if (selSprite == -1)
        return;

    // elevator
    if ((spriteID == 0x70) || (spriteID == 0x72))
    {
        ui->lvlEdit->setSpriteFlag(selSprite, index);
        ui->spbRAW->setValue(index);
    }
}

void MainWindow::on_spbRAW_valueChanged(int arg1)
{
    if ((arg1 < 0) || (arg1 > 255))
        return;

    int spriteID;
    int selSprite = ui->lvlEdit->getSelectedSprite(&spriteID);
    if (selSprite == -1)
        return;

    // DKs
    if ((spriteID == 0xB8) || (spriteID == 0x6E) || (spriteID == 0x9A) || (spriteID == 0xCC))
        ui->lvlEdit->setSpriteFlag(selSprite, arg1);
}

void MainWindow::addSwitch(QDKSwitch *sw)
{
    addSwitchAtPos(ui->treSwitches->topLevelItemCount(), sw);
}

void MainWindow::addSwitchAtPos(int i, QDKSwitch *sw)
{
    QTreeWidgetItem *item = new QTreeWidgetItem();
    QTreeWidgetItem *child;
    QDKSwitchObject *obj;

    QString state;
    switch (sw->state)
    {
        case 1: state = "middle"; break;
        case 2: state = "right"; break;
        default: state = "left";
    }

    item->setText(0, QString("Switch %1 (%2) at %3x%4").arg(i).arg(state).arg(sw->x).arg(sw->y));

    for (int i = 0; i < sw->connectedTo.size(); i++)
    {
        child = new QTreeWidgetItem();
        obj = &sw->connectedTo[i];
        if (!obj->isSprite)
            child->setText(0, QString("Tile at %1x%2").arg(obj->x).arg(obj->y));
        else
            child->setText(0, QString("Sprite at %1x%2").arg(obj->x).arg(obj->y));

        item->addChild(child);
    }

    ui->treSwitches->insertTopLevelItem(i, item);
    ui->treSwitches->expandAll();
}

void MainWindow::updateSwitch(int i, QDKSwitch *sw)
{
    QTreeWidgetItem *item = ui->treSwitches->takeTopLevelItem(i);
    if (!item)
        return;

    addSwitchAtPos(i, sw);
}

void MainWindow::removeSwitch(int i)
{
    if ((i < 0) || (i > ui->treSwitches->topLevelItemCount()))
        return;

    QTreeWidgetItem *item = ui->treSwitches->takeTopLevelItem(i);
    delete item;
}

void MainWindow::on_treSwitches_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *)
{
    if (!current)
        return;

    QTreeWidgetItem *parent;
    parent = current->parent();
    if (!parent)
        parent = current;

    bool ok;
    int i = parent->text(0).mid(7, 1).toInt(&ok);
    if (!ok)
        i = -1;

    ui->lvlEdit->selectSwitch(i);
}


void MainWindow::delSwitchItem()
{
    QTreeWidgetItem *item = ui->treSwitches->currentItem();
    if (!item)
        return;
    if (!item->parent())
        ui->lvlEdit->deleteCurrentSwitch();
    else
        ui->lvlEdit->deleteSwitchObj(item->parent()->indexOfChild(item));
}

void MainWindow::enableSaveBtn()
{
    ui->btnWrite->setEnabled(true);
}

void MainWindow::updateVRAMtiles(int tiles)
{
    if (tiles <= 80)
    {
        ui->barVRAMtiles->setRange(0, 80);
        ui->barVRAMtiles->setValue(tiles);
        QPalette p = ui->barVRAMtiles->palette();
        p.setColor(QPalette::Highlight, Qt::green);
        ui->barVRAMtiles->setPalette(p);
    }
    else if (tiles < (256 - ui->barVRAMsprites->value()) + 80)
    {
        ui->barVRAMtiles->setRange(0, (256 - ui->barVRAMsprites->value()) + 80);
        ui->barVRAMtiles->setValue(tiles);
        QPalette p = ui->barVRAMtiles->palette();
        p.setColor(QPalette::Highlight, QColor(0xFF, 0xA0, 0x00));
        ui->barVRAMtiles->setPalette(p);
    }
    else
    {
        ui->barVRAMtiles->setRange(0, 512);
        ui->barVRAMtiles->setValue(tiles);
        QPalette p = ui->barVRAMtiles->palette();
        p.setColor(QPalette::Highlight, Qt::red);
        ui->barVRAMtiles->setPalette(p);
    }
}

void MainWindow::updateVRAMsprites(int sprites)
{
    if (sprites <= 256)
    {
        ui->barVRAMsprites->setRange(0, 256);
        ui->barVRAMsprites->setValue(sprites);
        QPalette p = ui->barVRAMsprites->palette();
        p.setColor(QPalette::Highlight, Qt::green);
        ui->barVRAMsprites->setPalette(p);
    }
    else
    {
        ui->barVRAMsprites->setRange(0, 512);
        ui->barVRAMsprites->setValue(sprites);
        QPalette p = ui->barVRAMsprites->palette();
        p.setColor(QPalette::Highlight, Qt::red);
        ui->barVRAMsprites->setPalette(p);
    }

    if (ui->barVRAMtiles->value() > 80)
        updateVRAMtiles(ui->barVRAMtiles->value());
}
