#include "QTileEdit.h"

#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtGui/QPainter>
#include <QtGui/QMouseEvent>

#include <QtCore/QDebug>

#include "QTileSelector.h"

QTileEdit::QTileEdit(QWidget *parent) :
    QWidget(parent), dataIsChanged(false), mousePressed(false), emptyTile(0), tileToDraw(emptyTile), keepAspect(true), selector(NULL), tileDataIs16bit(false), spriteMode(false), spriteContext(false), spriteToMove(-1)
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
    if (!lvlData.size() || (offset < 0))
        return -1;

    int tileNumber;
    if (tileDataIs16bit)
    {
        if (lvlDataStart+offset*2 >= lvlData.size())
            return -1;
        tileNumber =  (unsigned char)lvlData[lvlDataStart+offset*2] + (unsigned char)lvlData[lvlDataStart+2*offset+1]*0x100;
    }
    else
    {
        if (lvlDataStart+offset >= lvlData.size())
            return -1;
        tileNumber = (unsigned char)lvlData[lvlDataStart+offset];
    }
    return tileNumber;

}

void QTileEdit::setTile(int x, int y, int tileNumber)
{
    if ((x < 0) || (y <  0))
        return;

    setTile(y*levelDimension.width()+x, tileNumber);
}

void QTileEdit::setTile(int offset, int tileNumber)
{
    if (tileDataIs16bit)
    {
        if (lvlDataStart+offset*2+1 >= lvlData.size())
            return;
        lvlData[lvlDataStart + offset * 2] = tileNumber % 0x100;
        lvlData[lvlDataStart + offset * 2 + 1] = tileNumber / 0x100;
    }
    else
    {
        if (lvlDataStart+offset >= lvlData.size())
            return;
        lvlData[lvlDataStart + offset] = (unsigned char)tileNumber;
    }
}


QString QTileEdit::spriteNumToString(int sprite)
{
    return spriteNames.value(sprite, QString("Sprite 0x%1").arg(sprite, 2, 16, QChar('0')));
}

QString QTileEdit::tileNumToString(int tile)
{
    if (tileDataIs16bit)
        return tileNames.value(tile, QString("0x%1").arg(tile, 4, 16, QChar('0')));
    else
        return tileNames.value(tile, QString("0x%1").arg(tile, 2, 16, QChar('0')));
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
    QPainter *painter = getPainter();

    if (!painter)
        return;

    paintLevel(painter);

    finishPainter(painter);
}

void QTileEdit::paintLevel(QPainter *painter)
{
    //draw background
    //QPainter painter(this);
    painter->drawImage(QRect(0, 0, orgSize.width(), orgSize.height()), background);

    //draw tiles
    painter->setBackgroundMode(Qt::TransparentMode);
    int tileNumber;
    for (int i = 0; i < levelDimension.height(); i++)
        for (int j = 0; j < levelDimension.width(); j++)
        {
            tileNumber = getTile(j, i);
            painter->drawPixmap(QRect(j*tileSize.width(), i*tileSize.height(), tileSize.width(), tileSize.height()), tileSet, tileNumberToQRect(tileNumber));
        }

    if (spriteMode)
    {
        int x, y;

        // draw sprites
        for (int i = 0; i < sprites.size(); i++)
        {
            if (!sprites.at(i).pixelPerfect)
            {
                x = sprites.at(i).x*tileSize.width() + (sprites.at(i).drawOffset.x()*(qreal)tileSize.width());
                y = sprites.at(i).y*tileSize.height() + (sprites.at(i).drawOffset.y()*(qreal)tileSize.height());
            }
            else
            {
                x = sprites.at(i).x;
                y = sprites.at(i).y;
            }

            switch (sprites.at(i).rotate)
            {
                case LEFT:
                    painter->save();
                    painter->translate(sprites.at(i).sprite->height(), 0);
                    painter->rotate(90);
                    painter->drawPixmap(y, -1*x, *sprites.at(i).sprite);
                    painter->restore();
                    break;
                case RIGHT:
                    painter->save();
                    painter->translate(0, sprites.at(i).sprite->width());
                    painter->rotate(-90);
                    painter->drawPixmap(-1*y, x, *sprites.at(i).sprite);
                    painter->restore();
                    break;
                case TOP:
                    painter->save();
                    painter->translate(sprites.at(i).sprite->width(), sprites.at(i).sprite->height());
                    painter->rotate(180);
                    painter->drawPixmap(-1*x, -1*y, *sprites.at(i).sprite);
                    painter->restore();
                    break;
                case FLIPPED:
                    painter->save();
                    painter->translate(sprites.at(i).sprite->width(), 0);
                    painter->scale(-1, 1);
                    painter->drawPixmap(-1*x, y, *sprites.at(i).sprite);
                    painter->restore();
                    break;
                 case BOTTOM:
                 default: painter->drawPixmap(x, y, *sprites.at(i).sprite); break;
            }
        }
    }

    //draw selection
    painter->setPen(Qt::gray);
    painter->drawRect(mouseOverTile.x(), mouseOverTile.y(), mouseOverTile.width(), mouseOverTile.height());

    //draw sprite selection
    if (spriteMode)
    {
        painter->setPen(Qt::red);
        painter->drawRect(spriteSelection.x(), spriteSelection.y(), spriteSelection.width(), spriteSelection.height());
    }
}

QPainter *QTileEdit::getPainter()
{
    QPainter *painter = new QPainter();

    bool ok;
    if (this->size() == orgSize)
        ok = painter->begin(this);
    else
        ok = painter->begin(&originalSize);

    if (!ok)
        return NULL;
    else
        return painter;
}

void QTileEdit::finishPainter(QPainter *painter)
{
    if (!painter)
        return;

    painter->end();

    if (this->size() != orgSize)
    {
        QPainter widgetPainter(this);
        QRect target;
        if (!keepAspect)
            target = this->rect();
        else
            target = QRect(0, 0, scaledSize.width(), scaledSize.height());

        widgetPainter.drawImage(target, originalSize);
    }

    delete painter;
}

void QTileEdit::resizeEvent(QResizeEvent *e)
{
    resized(e->size());
}

void QTileEdit::mouseMoveEvent(QMouseEvent *e)
{
    if ((e->x()+1 > scaledSize.width()) || (e->x() < 0) || (e->y()+1 > scaledSize.height()) || (e->y() < 0))
        return;

    if (!spriteMode)
    {
        //check if selection rect has moved
        int xTile= (float)e->x() / (float)tileSize.width() / scaleFactorX;
        int yTile = (float)e->y() / (float)tileSize.height() / scaleFactorY;

        setToolTip(QString("%1 (%2x%3)").arg(tileNumToString(getTile(xTile, yTile))).arg(xTile).arg(yTile));

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
        QRect spriteRect;

        if (!mousePressed)
        {
            spriteToMove = getSpriteAtXY((float)e->x() / scaleFactorX, (float)e->y() / scaleFactorY, &spriteRect);

            if (spriteToMove == -1)
            {
                mouseOverTile = QRect();
                if (e->buttons() != Qt::LeftButton)
                {
                    update();
                    return;
                }
            }
        }
        else
            spriteRect = getSpriteRect(spriteToMove);

        if (spriteToMove == -1)
            return;

        setToolTip(QString("0x%1").arg(sprites.at(spriteToMove).id, 2, 16, QChar('0')));

        // sprite changed - update all selections
        if (mouseOverTile != spriteRect)
        {
            mouseOverTile = spriteRect;
            update();
        }

        if (e->buttons() != Qt::LeftButton)
            return;

        // mouse button is pressed
        // update sprite selection
        spriteSelection = spriteRect;

        // sprite gets moved
        // calculate new x,y
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

            if (sprites.at(spriteToMove).pixelPerfect)
                spriteRect = QRect(newX, newY, sprites.at(spriteToMove).size.width(), sprites.at(spriteToMove).size.height());
            else
                spriteRect = QRect(newX*tileSize.width(), newY*tileSize.height(), sprites.at(spriteToMove).size.width()*tileSize.width(), sprites.at(spriteToMove).size.height()*tileSize.height());

            mouseOverTile = spriteRect;
            spriteSelection = spriteRect;
            dataIsChanged = true;

            emit dataChanged();
            update();
        }
    }
}

void QTileEdit::mousePressEvent(QMouseEvent *e)
{
    if (e->type() == QEvent::MouseButtonPress)
        mousePressed = true;

    if (!spriteMode)
        mouseMoveEvent(e);
    else
    {
        if ((e->button() == Qt::LeftButton) || (e->button() == Qt::RightButton))
        {
            if (spriteSelection != mouseOverTile)
            {
                spriteSelection = mouseOverTile;
                selectedSprite = spriteToMove;
                emit spriteSelected(selectedSprite);
                update();
            }
        }
        if (e->button() == Qt::RightButton)
        {
            if (!spriteContext)
            {
                if (spriteToMove == -1)
                    return;
                sprites.remove(spriteToMove);
                mouseOverTile = QRect();
                spriteSelection = QRect();
                dataIsChanged = true;
                emit spriteSelected(-1);
                emit spriteRemoved(spriteToMove);
                emit dataChanged();
                spriteToMove = -1;
                selectedSprite = -1;
                update();
                return;
            }
            else
            {
                emit customContextMenuRequested(e->pos());
                mouseOverTile = QRect();
                mousePressed = false;
                spriteToMove = -1;
                update();
                return;
            }
        }
    }
}

int QTileEdit::getSpriteAtXY(int x, int y, QRect *spriteRect = NULL)
{
    int i;
    int sx1, sx2, sy1, sy2;

    for (i = 0; i < sprites.size(); i++)
    {
        if (sprites.at(i).pixelPerfect)
        {
            sx1 = sprites.at(i).x;
            sy1 = sprites.at(i).y;
            if ((sprites.at(i).rotate == LEFT) || (sprites.at(i).rotate == RIGHT))
            {
                sx2 = sprites.at(i).x + sprites.at(i).size.width();
                sy2 = sprites.at(i).x + sprites.at(i).size.width();
            }
            else
            {
                sx2 = sprites.at(i).x + sprites.at(i).size.height();
                sy2 = sprites.at(i).x + sprites.at(i).size.height();
            }
        }
        else
        {
            sx1 = sprites.at(i).x * tileSize.width()  + (sprites.at(i).drawOffset.x()*(qreal)tileSize.width());
            sy1 = sprites.at(i).y * tileSize.height() + (sprites.at(i).drawOffset.y()*(qreal)tileSize.height());

            if ((sprites.at(i).rotate == LEFT) || (sprites.at(i).rotate == RIGHT))
            {
                sx2 = (sprites.at(i).x + sprites.at(i).size.height()) * tileSize.width()  + (sprites.at(i).drawOffset.x()*(qreal)tileSize.width());
                sy2 = (sprites.at(i).y + sprites.at(i).size.width()) * tileSize.height()  + (sprites.at(i).drawOffset.y()*(qreal)tileSize.height());
            }
            else
            {
                sx2 = (sprites.at(i).x + sprites.at(i).size.width()) * tileSize.width() + (sprites.at(i).drawOffset.x()*(qreal)tileSize.width());
                sy2 = (sprites.at(i).y + sprites.at(i).size.height()) * tileSize.height() + (sprites.at(i).drawOffset.y()*(qreal)tileSize.height());
            }
        }

        if ((x >= sx1) && (x < sx2) && (y >= sy1) && (y < sy2))
            break;
    }

    if (i == sprites.size())
        return -1;
    else
    {
        if (spriteRect)
            *spriteRect = QRect(sx1, sy1, sx2-sx1-1, sy2-sy1-1);
        return i;
    }
}

void QTileEdit::mouseReleaseEvent(QMouseEvent *)
{
    mousePressed = false;
}

int QTileEdit::getSelectedSprite(int *id)
{
    if (selectedSprite != -1)
        *id = sprites.at(selectedSprite).id;
    else
        *id = 0x00;

    return selectedSprite;
}


QRect QTileEdit::getSpriteRect(int num)
{
    int sx1, sx2, sy1, sy2;

    if (num >= sprites.size())
        return QRect();

    if (sprites.at(num).pixelPerfect)
    {
        sx1 = sprites.at(num).x;
        sy1 = sprites.at(num).y;
        if ((sprites.at(num).rotate == LEFT) || (sprites.at(num).rotate == RIGHT))
        {
            sx2 = sprites.at(num).x + sprites.at(num).size.height();
            sy2 = sprites.at(num).x + sprites.at(num).size.width();
        }
        else
        {
            sx2 = sprites.at(num).x + sprites.at(num).size.width();
            sy2 = sprites.at(num).x + sprites.at(num).size.height();
        }
    }
    else
    {
        sx1 = sprites.at(num).x * tileSize.width()  + (sprites.at(num).drawOffset.x()*(qreal)tileSize.width());
        sy1 = sprites.at(num).y * tileSize.height()  + (sprites.at(num).drawOffset.y()*(qreal)tileSize.height());
        if ((sprites.at(num).rotate == LEFT) || (sprites.at(num).rotate == RIGHT))
        {
            sx2 = (sprites.at(num).x + sprites.at(num).size.height()) * tileSize.width() + (sprites.at(num).drawOffset.x()*(qreal)tileSize.width());
            sy2 = (sprites.at(num).y + sprites.at(num).size.width()) * tileSize.height() + (sprites.at(num).drawOffset.y()*(qreal)tileSize.height());
        }
        else
        {
            sx2 = (sprites.at(num).x + sprites.at(num).size.width()) * tileSize.width() + (sprites.at(num).drawOffset.x()*(qreal)tileSize.width());
            sy2 = (sprites.at(num).y + sprites.at(num).size.height()) * tileSize.height() + (sprites.at(num).drawOffset.y()*(qreal)tileSize.height());
        }

    }

    return QRect(sx1, sy1, sx2-sx1-1, sy2-sy1-1);
}

void QTileEdit::setupTileSelector(QTileSelector *tileSelector, float scale, int limitTileCount)
{
    selector = tileSelector;
    QFile names("tiles.txt");
    QStringList tileNames;
    if (names.exists())
    {
        if (names.open(QIODevice::ReadOnly))
        {
            QTextStream namesStream(&names);
            while (!namesStream.atEnd())
            {
                tileNames.append(namesStream.readLine());
            }
        }
    }
    else
        for (int i = 0; i < limitTileCount; i++)
            tileNames.append(tileNumToString(i));

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

void QTileEdit::deleteSprite(int num)
{
    if (num >= sprites.size())
        return;

    sprites.remove(num);

    mouseOverTile = QRect();
    spriteSelection = QRect();
    dataIsChanged = true;
    emit spriteSelected(-1);
    emit spriteRemoved(num);
    emit dataChanged();
    spriteToMove = -1;
    selectedSprite = -1;
    update();
}

void QTileEdit::toggleSpriteMode(int enabled)
{
    toggleSpriteMode((bool)enabled);
}

void QTileEdit::getSpriteFlag(int num, quint8 *flag)
{
    if (num < sprites.size())
        *flag = sprites.at(num).flagByte;
}

void QTileEdit::setSpriteFlag(int num, quint8 flag)
{
    if (num < sprites.size())
        sprites[num].flagByte = flag;

    emit flagByteChanged(num);
}
