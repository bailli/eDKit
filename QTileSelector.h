#ifndef QTILESELECTOR_H
#define QTILESELECTOR_H

#include <QtGui/QWidget>

class QTileSelector : public QWidget
{
    Q_OBJECT
public:
    explicit QTileSelector(QWidget *parent = 0);
    void getMouse(bool enable);

private:
    QSize tileSize;
    QSize orgTileSize;
    QSize imageDimension;
    QSize arrangedTilesDimension;
    QPixmap tiles;
    QImage arrangedTiles;
    QRect selectedTile;
    QRect mouseOverTile;
    QRect allTilesRect;
    QStringList tileNames;
    int tileCount;
    int selectedTileNumber;
    void updateImage();
    float scaleFactor;

protected:
    void paintEvent(QPaintEvent *);
    void mouseMoveEvent(QMouseEvent *e);
    void mousePressEvent(QMouseEvent *e);
    void resizeEvent(QResizeEvent *e);
    bool event(QEvent *e);


signals:
    void tileSelected(int tileNumber);

public slots:
    void setTilePixmap(QPixmap tilePixmap, QSize size, float scale, int count, QStringList names);
};

#endif // QTILESELECTOR_H
