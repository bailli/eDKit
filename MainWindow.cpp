#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QtCore/QFile>
#include <QtCore/QDataStream>
#include <QtCore/QDebug>
#include <QtGui/QMessageBox>
#include <QtGui/QFileDialog>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->lvlEdit->setupTileSelector(ui->tileSelect, 2.0f, 256);

    ui->actionOpen_ROM->setShortcut(QKeySequence::Open);
    ui->actionSave_ROM->setShortcut(QKeySequence::Save);

    connect(ui->spbLevel, SIGNAL(valueChanged(int)), ui->lvlEdit, SLOT(changeLevel(int)));
    connect(ui->lvlEdit, SIGNAL(dataChanged()), this, SLOT(updateText()));
    connect(ui->tabWidget, SIGNAL(currentChanged(int)), ui->lvlEdit, SLOT(toggleSpriteMode(int)));
    connect(ui->actionOpen_ROM, SIGNAL(triggered()), this, SLOT(loadROM()));
    connect(ui->actionSave_ROM, SIGNAL(triggered()), this, SLOT(SaveROM()));

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
    QString file = QFileDialog::getSaveFileName(0, "Select Donkey Kong (GB) ROM", qApp->applicationDirPath(), "Donkey Kong (GB) ROM (*.gb)");

    if (!QFile::exists(file))
    {
        qDebug() << file;
        QFile::copy(qApp->applicationDirPath() + BASE_ROM, file);
        qDebug() << "here2";
    }

    ui->lvlEdit->saveAllLevels(file);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_btnWrite_clicked()
{
    ui->lvlEdit->saveAllLevels("mod.gb");
}
