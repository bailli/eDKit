#include "QTileSelector.h"

#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QResizeEvent>

QTileSelector::QTileSelector(QWidget *parent) :
    QWidget(parent)
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

    return QWidget::event(e);
}

void QTileSelector::updateImage()
{
    int pixmapX, pixmapY;
    pixmapX = tiles.width() / orgTileSize.width();
    pixmapY = tiles.height() / orgTileSize.height();

    imageDimension.setWidth(this->rect().width() / tileSize.width());
    imageDimension.setHeight(this->rect().height() / tileSize.height());

    allTilesRect = QRect(0, 0, imageDimension.width()*tileSize.width(), ((tileCount / imageDimension.width()) + 1)*tileSize.height());

    arrangedTilesDimension.setWidth(allTilesRect.width() / tileSize.width());
    arrangedTilesDimension.setHeight(allTilesRect.height() / tileSize.height());

    int x, x2, y, y2;
    QPainter painter;

    arrangedTiles = QImage(this->rect().width(), this->rect().height(), QImage::Format_ARGB32);
    arrangedTiles.fill(Qt::white);
    painter.begin(&arrangedTiles);
    painter.fillRect(allTilesRect, Qt::white);
    painter.end();


    for (int i = 0; i < tileCount; i++)
    {
        x = (i % imageDimension.width()) * tileSize.width();
        y = (i / imageDimension.width()) * tileSize.height();
        x2 = (i % pixmapX) * orgTileSize.width();
        y2 = (i / pixmapX) * orgTileSize.height();
        QRect target(x, y, tileSize.width(), tileSize.height());
        QRect source(x2, y2, orgTileSize.width(), orgTileSize.height());
        painter.begin(&arrangedTiles);
        painter.drawPixmap(target, tiles, source);
        painter.end();
    }

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
