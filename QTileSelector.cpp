#include "QTileSelector.h"

#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QResizeEvent>
#include <QDebug>
#include <QDKEdit.h>

QTileSelector::QTileSelector(QWidget *parent) :
    QWidget(parent), pixSrc(NULL)
{
    arrangedTiles = QImage(this->rect().width(), this->rect().height(), QImage::Format_ARGB32);
    QPainter painter;
    painter.begin(&arrangedTiles);
    painter.fillRect(this->rect(), Qt::white);
    painter.end();
    mouseOverTile = QRect(0, 0, 0, 0);
    //setMouseTracking(true);
}

void QTileSelector::changeTilePixmap(QPixmap tilePixmap)
{
    tiles = tilePixmap;
    updateImage();
}

void QTileSelector::setTilePixmap(QPixmap tilePixmap, QSize size, float scale, int count, QStringList names)
{
    tiles = tilePixmap;
    orgTileSize = size;
    tileCount = count;
    tileNames = names;
    scaleFactor = scale;
    tileSize = orgTileSize * scale;

    updateImage();
}

bool QTileSelector::event(QEvent *e)
{
    if (e->type() == QEvent::ToolTip)
    {
        if (groups.isEmpty())
        {
            QHelpEvent *he = static_cast<QHelpEvent*>(e);
            int xTile= he->x() / tileSize.width();
            int yTile = he->y() / tileSize.height();

            if ((xTile >= arrangedTilesDimension.width()) || (yTile >= arrangedTilesDimension.height()))
            {
                setToolTip("");
                return QWidget::event(e);
            }

            int num = yTile*imageDimension.width()+xTile;
            if (num < tileCount)
                setToolTip(tileNames.value(num, QString("TileNumber: 0x%1").arg(num, 0, 16)));
            else
                setToolTip("");
        }
        else
        {
            QHelpEvent *he = static_cast<QHelpEvent*>(e);
            if ((he->x() >= allTilesRect.width()) || (he->y() >= allTilesRect.height()))
            {
                setToolTip("");
                return QWidget::event(e);
            }

            int tile = -1;
            QRect tileRect;
            QMap<int, QRect>::const_iterator it = tileRects.constBegin();
            while (it != tileRects.constEnd())
            {
                tileRect = it.value();

                if ((he->x() >= tileRect.x()) && (he->y() >= tileRect.y()) &&
                        (he->x() < tileRect.x() + tileRect.width()) && (he->y() < tileRect.y() + tileRect.height()))
                {
                    tile = it.key();
                    break;
                }

                ++it;
            }

            if (tile != -1)
                setToolTip(tileNames.value(tile, QString("TileNumber: 0x%1").arg(tile, 0, 16)));
            else
                setToolTip("");
        }
    }

    return QWidget::event(e);
}

void QTileSelector::groupTiles(QTileGroups grouping, QSpacings spacings)
{
    groups = grouping;
    space = spacings;
    updateImage();
}

void QTileSelector::setTilePixSrc(QDKEdit *src)
{
    pixSrc = src;
    updateImage();
}

void QTileSelector::updateImage()
{
    int pixmapX, pixmapY;
    pixmapX = tiles.width() / orgTileSize.width();
    pixmapY = tiles.height() / orgTileSize.height();

    imageDimension.setWidth(this->rect().width() / tileSize.width());
    imageDimension.setHeight(this->rect().height() / tileSize.height());

    if (groups.isEmpty())
        allTilesRect = QRect(0, 0, imageDimension.width()*tileSize.width(), ((tileCount / imageDimension.width()) + 1)*tileSize.height());
    else
        allTilesRect = this->rect();

    arrangedTilesDimension.setWidth(allTilesRect.width() / tileSize.width());
    arrangedTilesDimension.setHeight(allTilesRect.height() / tileSize.height());

    int x, x2, y, y2;
    QPainter painter;

    arrangedTiles = QImage(this->rect().width(), this->rect().height(), QImage::Format_ARGB32);
    arrangedTiles.fill(Qt::white);
    painter.begin(&arrangedTiles);
    painter.fillRect(allTilesRect, Qt::white);

    if (groups.empty())
    {
        for (int i = 0; i < tileCount; i++)
        {
            x = (i % imageDimension.width()) * tileSize.width();
            y = (i / imageDimension.width()) * tileSize.height();
            x2 = (i % pixmapX) * orgTileSize.width();
            y2 = (i / pixmapX) * orgTileSize.height();
            QRect target(x, y, tileSize.width(), tileSize.height());
            QRect source(x2, y2, orgTileSize.width(), orgTileSize.height());
            painter.drawPixmap(target, tiles, source);
        }
    }

    else
    {
        QVector<int> tilelist;
        int vPos, hPos, x, y;
        QString title;
        QPixmap *pix;

        vPos = space.top;
        hPos = space.left;

        tileRects.clear();

        QTileGroups::const_iterator it = groups.constBegin();
        while (it != groups.constEnd())
        {
            title = it.key();
            tilelist = it.value();
            ++it;

            hPos = space.left;
            painter.drawText(QRect(hPos, vPos, allTilesRect.width(), tileSize.height()), Qt::AlignVCenter, title);
            vPos += tileSize.height() + space.afterText;

            for (int i = 0; i < tilelist.size(); i++)
            {
                x = (tilelist[i] % pixmapX) * orgTileSize.width();
                y = (tilelist[i] / pixmapX) * orgTileSize.height();
                if (pixSrc)
                    pix = pixSrc->getTilePixmap(tilelist[i]);

                else
                {
                    pix = new QPixmap(tileSize);
                    QPainter p;
                    p.begin(pix);
                    x2 = (tilelist[i] % pixmapX) * orgTileSize.width();
                    y2 = (tilelist[i] / pixmapX) * orgTileSize.height();
                    QRect source(x2, y2, orgTileSize.width(), orgTileSize.height());
                    p.drawPixmap(pix->rect(), tiles, source);
                    p.end();
                }
                QSize pixSize = pix->size();
                pixSize.scale(tileSize, Qt::KeepAspectRatio);
                QRect target(hPos+(tileSize.width() - pixSize.width())/2, vPos+(tileSize.height() - pixSize.height())/2, pixSize.width(), pixSize.height());
                tileRects.insert(tilelist[i], QRect(hPos, vPos, tileSize.width(), tileSize.height()));
                painter.drawPixmap(target, *pix);
                delete pix;

                hPos += tileSize.width() + space.hSpace;
                if (hPos + tileSize.width() + space.right > arrangedTiles.width())
                {
                    hPos = space.left;
                    vPos += tileSize.height() + space.vSpace;
                }
            }
            if (hPos == space.left)
                vPos -= tileSize.height() + space.vSpace;
            vPos += tileSize.height() + space.beforeText;
        }

    }

    painter.end();
    update();
}

void QTileSelector::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.drawImage(QPoint(0, 0), arrangedTiles);

    //draw selection
    painter.setPen(Qt::black);
    painter.drawRect(mouseOverTile.x(), mouseOverTile.y(), mouseOverTile.width(), mouseOverTile.height());

    //draw selected tile
    painter.setPen(Qt::red);
    painter.drawRect(selectedTile.x(), selectedTile.y(), selectedTile.width(), selectedTile.height());
}

void QTileSelector::mouseMoveEvent(QMouseEvent *e)
{
    if (groups.isEmpty())
    {
        //check if selection rect has moved
        int xTile= e->x() / tileSize.width();
        int yTile = e->y() / tileSize.height();

        QRect newSelection;

        if ((xTile >= arrangedTilesDimension.width()) || (yTile >= arrangedTilesDimension.height()))
            newSelection = QRect(0, 0, 0, 0);
        else
            newSelection = QRect(xTile * tileSize.width(), yTile * tileSize.height(), tileSize.width()-1, tileSize.height()-1);

        if (mouseOverTile != newSelection)
        {
            mouseOverTile = newSelection;
            update();
        }


        //check whether left or right mouse button has been pressed
        if (e->buttons() != Qt::LeftButton)
            return;

        if (newSelection != selectedTile)
        {
            int newTileNumber = xTile + (yTile * imageDimension.width());
            if (newTileNumber >= tileCount)
                return;

            selectedTile = newSelection;
            selectedTileNumber = newTileNumber;
            update();
            emit tileSelected(selectedTileNumber);
        }
    }
    else
    {
        QRect newSelection;
        int tile = -1;
        QRect tileRect;
        //check if mouse is inside rect
        if ((e->x() >= allTilesRect.width()) || (e->y() >= allTilesRect.height()))
            newSelection = QRect(0, 0, 0, 0);
        else
        {
            QMap<int, QRect>::const_iterator it = tileRects.constBegin();
            while (it != tileRects.constEnd())
            {
                tileRect = it.value();

                if ((e->x() >= tileRect.x()) && (e->y() >= tileRect.y()) &&
                    (e->x() < tileRect.x() + tileRect.width()) && (e->y() < tileRect.y() + tileRect.height()))
                {
                    tile = it.key();
                    break;
                }

                ++it;
            }

            if (tile != -1)
                newSelection = tileRect;
        }

        if (mouseOverTile != newSelection)
        {
            mouseOverTile = newSelection;
            update();
        }

        //check whether left or right mouse button has been pressed
        if (e->buttons() != Qt::LeftButton)
            return;

        if (newSelection != selectedTile)
        {
            if ((tile >= tileCount) || (tile == -1))
                return;

            selectedTile = newSelection;
            selectedTileNumber = tile;
            update();

            emit tileSelected(selectedTileNumber);
        }
    }
}

void QTileSelector::getMouse(bool enable)
{
    setMouseTracking(enable);
}

void QTileSelector::mousePressEvent(QMouseEvent *e)
{
    mouseMoveEvent(e);
}

void QTileSelector::resizeEvent(QResizeEvent *e)
{
    if (e->oldSize() != QSize(-1, -1))
        updateImage();
}
