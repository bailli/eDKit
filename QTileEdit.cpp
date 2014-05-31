#include "QTileEdit.h"

#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtGui/QPainter>
#include <QtGui/QMouseEvent>

#include <QtCore/QDebug>

#include "QTileSelector.h"

QTileEdit::QTileEdit(QWidget *parent) :
    QWidget(parent), dataIsChanged(false), emptyTile(0), tileToDraw(emptyTile), keepAspect(true), selector(NULL), tileDataIs16bit(false), spriteMode(false), spriteToMove(-1)
{
    //setMouseTracking(true);
    background = QImage(levelDimension.width()*tileSize.width(), levelDimension.height()*tileSize.height(), QImage::Format_ARGB32);
    background.fill(Qt::white);
    originalSize = QImage(0, 0, QImage::Format_RGB32);
}

void QTileEdit::getMouse(bool enable)
{
    setMouseTracking(enable);
    if (selector != NULL)
        selector->getMouse(enable);
}

bool QTileEdit::isChanged()
{
    return dataIsChanged;
}

void QTileEdit::updateLevel()
{
    update();
}

void QTileEdit::resized(QSize newSize)
{
    scaledSize = orgSize;
    scaledSize.scale(newSize, Qt::KeepAspectRatio);
    scaleFactorX = (float)scaledSize.width() / (float)orgSize.width();
    scaleFactorY = (float)scaledSize.height() / (float)orgSize.height();
}

void QTileEdit::setLevelDimension(int width, int height)
{
    levelDimension = QSize(width, height);
    orgSize = QSize(levelDimension.width()*tileSize.width(), levelDimension.height()*tileSize.height());
    resized(this->size());
    originalSize = QImage(orgSize, QImage::Format_RGB32);
}

void QTileEdit::setTileSize(int width, int height)
{
    tileSize = QSize(width, height);
    orgSize = QSize(levelDimension.width()*tileSize.width(), levelDimension.height()*tileSize.height());
    resized(this->size());
    originalSize = QImage(orgSize, QImage::Format_RGB32);
}

void QTileEdit::setTileToDraw(int tileNumber)
{
    tileToDraw = tileNumber;
}

bool QTileEdit::loadTileSet(QString filename, int emptyTileNumber, int count)
{
    if (!tileSet.load(filename))
        return false;

    emptyTile = emptyTileNumber;
    tileCount = count;
    tileSetDimension = QSize(tileSet.width() / tileSize.width(), tileSet.height() / tileSize.height());
    return true;
}

void QTileEdit::setLevelData(QByteArray data, int start = 0, int length = 0)
{
    lvlData = data;
    lvlDataStart = start;
    if (length == 0)
        lvlDataLength = data.size() - start;
    else
        lvlDataLength = length;

    update();
}

int QTileEdit::getTile(int x, int y)
{
    if (!lvlData.size())
        return -1;

    return getTile(y*levelDimension.width()+x);
}

int QTileEdit::getTile(int offset)
{
    if (!lvlData.size())
        return -1;

    int tileNumber;
    if (tileDataIs16bit)
        tileNumber =  (unsigned char)lvlData[lvlDataStart+offset*2] + (unsigned char)lvlData[lvlDataStart+2*offset+1]*0x100;
    else
        tileNumber = (unsigned char)lvlData[lvlDataStart+offset];
    return tileNumber;

}

void QTileEdit::setTile(int x, int y, int tileNumber)
{
    setTile(y*levelDimension.width()+x, tileNumber);
}

void QTileEdit::setTile(int offset, int tileNumber)
{
    if (tileDataIs16bit)
    {
        lvlData[lvlDataStart + offset * 2] = tileNumber % 0x100;
        lvlData[lvlDataStart + offset * 2 + 1] = tileNumber / 0x100;
    }
    else
        lvlData[lvlDataStart + offset] = (unsigned char)tileNumber;
}


QRect QTileEdit::tileNumberToQRect(int tileNumber)
{
    int x, y, x2, y2;

    y = tileNumber / tileSetDimension.width();
    x = tileNumber % tileSetDimension.width();
    if (y > tileSetDimension.height())
        return QRect(0, 0, tileSize.width(), tileSize.height());

    x2 = x*tileSize.width();
    y2 = y*tileSize.height();

    return QRect(x2, y2, tileSize.width(), tileSize.height());
}

void QTileEdit::drawTile(int tileNumber, int x, int y)
{
    QPainter painter(this);

    painter.setBackgroundMode(Qt::TransparentMode);
    painter.drawPixmap(QRect(x*tileSize.width(), y*tileSize.height(), tileSize.width(), tileSize.height()), tileSet, tileNumberToQRect(tileNumber));

}

void QTileEdit::setBackground(QImage backgroundImage)
{
    background = backgroundImage;
}

void QTileEdit::paintEvent(QPaintEvent *e)
{
    QPainter painter;

    bool ok;
    if (this->size() == orgSize)
        ok = painter.begin(this);
    else
        ok = painter.begin(&originalSize);

    if (!ok)
        return;

    //draw background
    //QPainter painter(this);
    painter.drawImage(QRect(0, 0, orgSize.width(), orgSize.height()), background);

    //draw tiles
    painter.setBackgroundMode(Qt::TransparentMode);
    int tileNumber;
    for (int i = 0; i < levelDimension.height(); i++)
        for (int j = 0; j < levelDimension.width(); j++)
        {
            tileNumber = getTile(j, i);
            painter.drawPixmap(QRect(j*tileSize.width(), i*tileSize.height(), tileSize.width(), tileSize.height()), tileSet, tileNumberToQRect(tileNumber));
        }

    if (spriteMode)
    {
        int x, y;

        // draw sprites
        for (int i = 0; i < sprites.size(); i++)
        {
            if (!sprites.at(i).pixelPerfect)
            {
                x = sprites.at(i).x*tileSize.width();
                y = sprites.at(i).y*tileSize.height();
            }
            else
            {
                x = sprites.at(i).x;
                y = sprites.at(i).y;
            }
            painter.drawPixmap(x, y, *sprites.at(i).sprite);
        }
    }

    //draw selection
    painter.setPen(Qt::gray);
    painter.drawRect(mouseOverTile.x(), mouseOverTile.y(), mouseOverTile.width(), mouseOverTile.height());

    painter.end();

    if (this->size() != orgSize)
    {
       QPainter widgetPainter(this);
        QRect target;
        if (!keepAspect)
            target = this->rect();
        else
        {
            //scaledSize = orgSize;
            //scaledSize.scale(this->size(), Qt::KeepAspectRatio);
            target = QRect(0, 0, scaledSize.width(), scaledSize.height());
        }
        widgetPainter.drawImage(target, originalSize);
    }
}

void QTileEdit::resizeEvent(QResizeEvent *e)
{
    resized(e->size());
}

void QTileEdit::mouseMoveEvent(QMouseEvent *e)
{
    if (!spriteMode)
    {
        //check if selection rect has moved
        int xTile= (float)e->x() / (float)tileSize.width() / scaleFactorX;
        int yTile = (float)e->y() / (float)tileSize.height() / scaleFactorY;

        setToolTip(QString("0x%1").arg(getTile(xTile, yTile), 2*((int)tileDataIs16bit+1), 16, QChar('0')));

        QRect newSelection(xTile * tileSize.width(), yTile * tileSize.height(), tileSize.width()-1, tileSize.height()-1);

        if (mouseOverTile != newSelection)
        {
            mouseOverTile = newSelection;
            update();
        }

        //check whether left or right mouse button has been pressed
        if ((e->buttons() != Qt::LeftButton) && (e->buttons() != Qt::RightButton))
            return;

        int tmpTileToDraw = tileToDraw;

        //always draw emptyTile for right click
        if (e->buttons() == Qt::RightButton)
            tmpTileToDraw = emptyTile;


        if (getTile(xTile, yTile) != tmpTileToDraw)
        {
            setTile(xTile, yTile, tmpTileToDraw);
            dataIsChanged = true;
            emit dataChanged();
            emit singleTileChanged(xTile, yTile, tmpTileToDraw);
            update();
        }
    }
    else
    {
        //check if mouse is over a sprite
        int spriteNo;
        int mx = (float)e->x() / scaleFactorX;
        int my = (float)e->y() / scaleFactorY;
        int sx1, sx2, sy1, sy2;

        for (spriteNo = 0; spriteNo < sprites.size(); spriteNo++)
        {
            if (sprites.at(spriteNo).pixelPerfect)
            {
                sx1 = sprites.at(spriteNo).x;
                sx2 = sprites.at(spriteNo).x + sprites.at(spriteNo).size.width();
                sy1 = sprites.at(spriteNo).y;
                sy2 = sprites.at(spriteNo).x + sprites.at(spriteNo).size.height();
            }
            else
            {
                sx1 = sprites.at(spriteNo).x * tileSize.width();
                sx2 = (sprites.at(spriteNo).x + sprites.at(spriteNo).size.width()) * tileSize.width();
                sy1 = sprites.at(spriteNo).y * tileSize.height();
                sy2 = (sprites.at(spriteNo).y + sprites.at(spriteNo).size.height()) * tileSize.height();
            }

            if ((mx >= sx1) && (mx < sx2) && (my >= sy1) && (my < sy2))
                break;
        }

        if (spriteNo == sprites.size())
        {
            mouseOverTile = QRect();
            if (e->buttons() != Qt::LeftButton)
            {
                spriteToMove = -1;
                update();
                return;
            }
        }
        else
            spriteToMove = spriteNo;

        if (spriteToMove == -1)
            return;

        if (e->button() == Qt::RightButton)
        {
            sprites.remove(spriteToMove);
            spriteToMove = -1;
            mouseOverTile = QRect();
            dataIsChanged = true;
            emit dataChanged();
            update();
            return;
        }

        setToolTip(QString("0x%1").arg(sprites.at(spriteNo).id, 2, 16, QChar('0')));

        QRect newSelection(sx1, sy1, sx2-sx1-1, sy2-sy1-1);

        if (mouseOverTile != newSelection)
        {
            mouseOverTile = newSelection;
            update();
        }

//        return;

        if (e->buttons() != Qt::LeftButton)
            return;

        //calculate new x,y
        int newX, newY;

        if (sprites.at(spriteToMove).pixelPerfect)
        {
            newX = e->x();
            newY = e->y();
        }
        else
        {
            newX = (float)e->x() / (float)tileSize.width() / scaleFactorX;
            newY = (float)e->y() / (float)tileSize.height() / scaleFactorY;
        }

        if ((newX != sprites.at(spriteToMove).x) || (newY != sprites.at(spriteToMove).y))
        {
            sprites[spriteToMove].x = newX;
            sprites[spriteToMove].y = newY;
            dataIsChanged = true;
            emit dataChanged();
            update();
        }
    }
}

void QTileEdit::mousePressEvent(QMouseEvent *e)
{
    mouseMoveEvent(e);
}

void QTileEdit::setupTileSelector(QTileSelector *tileSelector, float scale, int limitTileCount)
{
    selector = tileSelector;
    QFile names("tiles.txt");
    QStringList tileNames;
    if (names.exists())
        if (names.open(QIODevice::ReadOnly))
        {
            QTextStream namesStream(&names);
            while (!namesStream.atEnd())
            {
                tileNames.append(namesStream.readLine());
            }
        }

    int count;

    if (limitTileCount != 0)
        count = limitTileCount;
    else
        count = tileCount;

    selector->setTilePixmap(tileSet, tileSize, scale, count, tileNames);
    if (hasMouseTracking())
        selector->getMouse(true);
    connect(selector, SIGNAL(tileSelected(int)), this, SLOT(setTileToDraw(int)));
}

void QTileEdit::toggleSpriteMode(bool enabled)
{
    if (enabled != spriteMode)
    {
        spriteMode = enabled;
        update();
    }
}

void QTileEdit::toggleSpriteMode(int enabled)
{
    toggleSpriteMode((bool)enabled);
}
