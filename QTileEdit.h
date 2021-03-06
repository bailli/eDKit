#ifndef QTILEEDIT_H
#define QTILEEDIT_H

#include <QWidget>
#include <QMap>
#include <QStack>

class QTileSelector;

enum { BOTTOM = 0, FLIPPED = 1, TOP = 2, LEFT = 3, RIGHT = 4 };

struct QSprite
{
    bool pixelPerfect;
    int x, y;
    QSize size;
    QPointF drawOffset;
    QPixmap *sprite;
    int rotate;
    int id;
    quint8 flagByte;    
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
    int getSelectedSprite(int *id);

    QString spriteNumToString(int sprite);
    QString tileNumToString(int tile);

    void getMouse(bool enable);

    void setupTileSelector(QTileSelector *tileSelector, float scale = 1.25f, int limitTileCount = 0);

protected:
    void paintEvent(QPaintEvent *e);
    virtual void paintLevel(QPainter *painter);
    void mouseMoveEvent(QMouseEvent *e);
    void mousePressEvent(QMouseEvent *e);
    void mouseReleaseEvent(QMouseEvent *);
    void resizeEvent(QResizeEvent *e);
    QPainter *getPainter();
    void finishPainter(QPainter *painter);

    void resized(QSize newSize);
    void drawTile(int tileNumber, int x, int y);
    QRect tileNumberToQRect(int tileNumber);
    int getSpriteAtXY(int x, int y, QRect *spriteRect);
    QRect getSpriteRect(int num);
    int getTile(int x, int y);
    int getTile(int offset);
    void setTile(int x, int y, int tileNumber);
    void setTile(int offset, int tileNumber);

    QMap<int, QString> spriteNames;
    QMap<int, QString> tileNames;

    QStack<QPair<QByteArray, QVector<QSprite> > > undoStack;
    bool keepUndo;
    virtual void createUndoData();
    virtual void clearUndoData();
    virtual void deleteLastUndo();

    QVector<QSprite> sprites;
    QByteArray lvlData;
    int lvlDataStart;
    int lvlDataLength;
    int emptyTile;
    int tileToDraw;
    int tileCount;
    int spriteToMove;
    int selectedSprite;
    int mousePressed;
    float scaleFactorX;
    float scaleFactorY;
    bool dataIsChanged;
    bool tileDataIs16bit;
    bool spriteContext;
    bool keepAspect;
    bool spriteMode;
    QRect mouseOverTile;
    QRect spriteSelection;
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
    void spriteSelected(int spriteNo);
    void spriteAdded(QString text, int id);
    void spriteRemoved(int index);
    void flagByteChanged(int num);

public slots:
    void updateLevel();
    void keepUndoData();
    virtual void clearLevel();
    void setTileToDraw(int tileNumber);
    void toggleSpriteMode(bool enabled);
    void toggleSpriteMode(int enabled);
    void getSpriteFlag(int num, quint8 *flag);
    void setSpriteFlag(int num, quint8 flag);
    void deleteSprite(int num);
    virtual void undo();
};

#endif // QTILEEDIT_H
