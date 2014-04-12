#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QtCore/QFile>
#include <QtCore/QDataStream>
#include <QtCore/QDebug>
#include <QMessageBox>
#include <QFileDialog>

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
    ui->spbLevel->setFocus();
}

void MainWindow::changeLevel(int id)
{
    if (ui->lvlEdit->isChanged())
    {
        QMessageBox::StandardButton result = QMessageBox::question(NULL, "Level data changed", "The level data has been changed. Save data?", QMessageBox::Yes | QMessageBox::No);
        if (result == QMessageBox::Yes) // save changes
            ui->lvlEdit->saveLevel();
        else if (result != QMessageBox::Yes) // discard changes
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
        else if (result != QMessageBox::Yes) // discard changes
            qWarning() << "Unexpected return value form messagebox!";
    }

    QString file = QFileDialog::getSaveFileName(0, "Select Donkey Kong (GB) ROM", qApp->applicationDirPath(), "Donkey Kong (GB) ROM (*.gb)");

    if (!QFile::exists(file))
        QFile::copy(qApp->applicationDirPath() + BASE_ROM, file);

    ui->lvlEdit->saveAllLevels(file);
}

MainWindow::~MainWindow()
{
    delete ui;
}
