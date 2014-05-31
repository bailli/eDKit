#ifndef QTILEEDIT_H
#define QTILEEDIT_H

#include <QWidget>

class QTileSelector;

struct QSprite
{
    bool pixelPerfect;
    int x, y;
    QSize size;
    QPixmap *sprite;
    int id;
};

class QTileEdit : public QWidget
{
    Q_OBJECT
public:
    explicit QTileEdit(QWidget *parent = 0);

    bool isChanged();
    bool loadTileSet(QString filename, int emptyTileNumber, int count);

    void setLevelDimension(int width, int height);
    void setTileSize(int width, int height);
    void setLevelData(QByteArray data, int start, int length);
    void setBackground(QImage backgroundImage);

    void getMouse(bool enable);

    void setupTileSelector(QTileSelector *tileSelector, float scale = 1.25f, int limitTileCount = 0);

protected:
    void paintEvent(QPaintEvent *e);
    void mouseMoveEvent(QMouseEvent *e);
    void mousePressEvent(QMouseEvent *e);
    void resizeEvent(QResizeEvent *e);

    void resized(QSize newSize);
    void drawTile(int tileNumber, int x, int y);
    QRect tileNumberToQRect(int tileNumber);
    int getTile(int x, int y);
    int getTile(int offset);
    void setTile(int x, int y, int tileNumber);
    void setTile(int offset, int tileNumber);

    QVector<QSprite> sprites;
    QByteArray lvlData;
    int lvlDataStart;
    int lvlDataLength;
    int emptyTile;
    int tileToDraw;
    int tileCount;
    int spriteToMove;
    float scaleFactorX;
    float scaleFactorY;
    bool dataIsChanged;
    bool tileDataIs16bit;
    bool keepAspect;
    bool spriteMode;
    QRect mouseOverTile;
    QSize orgSize;
    QSize scaledSize;
    QSize levelDimension;
    QSize tileSetDimension;
    QSize tileSize;
    QPixmap tileSet;
    QImage background;
    QImage tiledLevel;
    QImage originalSize;
    QTileSelector *selector;

signals:
    void dataChanged();
    void singleTileChanged(int x, int y, int drawnTile);

public slots:
    void updateLevel();
    void setTileToDraw(int tileNumber);
    void toggleSpriteMode(bool enabled);
    void toggleSpriteMode(int enabled);
};

#endif // QTILEEDIT_H
