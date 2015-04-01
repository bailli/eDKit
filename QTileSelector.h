#ifndef QTILESELECTOR_H
#define QTILESELECTOR_H

#include <QWidget>
#include <QMap>
#include <QSet>

class QDKEdit;

typedef QColor QGBPalette[4];

typedef QMap<QString, QVector<int> > QTileGroups;

struct QSpacings
{
    int top;
    int bottom;
    int left;
    int right;
    int vSpace;
    int hSpace;
    int beforeText;
    int afterText;
};

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
    QTileGroups groups;
    QSpacings space;
    QMap<int, QRect> tileRects;
    QDKEdit *pixSrc;

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
    void changeTilePixmap(QPixmap tilePixmap);
    void groupTiles(QTileGroups grouping, QSpacings spacings);
    void setTilePixSrc(QDKEdit *src);
};

#endif // QTILESELECTOR_H
