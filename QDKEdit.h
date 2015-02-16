#ifndef QDKEDIT_H
#define QDKEDIT_H

#include "QTileEdit.h"

#include <QtCore/QFile>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtGui/QPainter>

//find rombanks containing the level data
#define ROMBANK_1 0x05
#define COMPARE_POS_1 0x25FC  // < 0x2D

#define ROMBANK_POS_2 0x25FF  // 0x06
#define COMPARE_POS_2 0x2603  // < 0x50

#define ROMBANK_POS_3 0x2606  // 0x12

// some constants
#define MAX_LEVEL_ID 256
#define LAST_LEVEL 105
#define MAX_TILESETS 0x22
#define MAX_SPRITES 0x1B
#define POINTER_TABLE 0x14000
#define SUBTILESET_TABLE 0x30EEB
#define SGB_SYSTEM_PAL 0x786F0 // decompressed size 0x1000
// the SGB packet is always 51.(quint16)var+0x80.E4 00.E5.00.E6 00.C1.00.00 00.00.00.00
// asm @ 0x0E70
#define PAL_ARCADE 0x30F9A
// the palette for the 4 arcade levels are @ 0x4F9A (bank 0x0C) + (6*id)
// this value is palette index + 0xC8
// not freely editable!
#define PAL_TABLE 0x6093B
// PAL_TABLE + (id-4)*8
// palettes for all other levels

// BGP depends on tileset asm @ 0E9D

#define ELEVATOR_TABLE 0x30F77

struct QDKSprite : QSprite
{
    quint16 ramPos;
    quint32 levelPos;
//    quint8 addFlag;
};

typedef QColor QGBPalette[4];

struct QDKLevel
{
    quint8 id; // max 256 !
    quint8 rombank;
    quint32 offset;

    quint8 size; // 0x00 -> 0x240 bytes for tilemap else 0x380 bytes
    quint8 music;
    quint8 tileset;
    quint16 time;

    bool switchData;
    bool addSpriteData;
    bool fullDataUpToDate;

    QByteArray rawTilemap;
    QByteArray displayTilemap;
    quint16 paletteIndex;

    QList<QDKSprite> sprites;

    QByteArray rawSwitchData; // 0x11 + 0x90 bytes
    QByteArray rawAddSpriteData; // 0x40 bytes

    QByteArray fullData;
};

struct QTileInfo
{
    quint8 w, h;
    quint8 type; // that's a bit vague - this is the pointer+N value
    quint8 count;
    quint8 fullCount;
    quint8 setSpecific;
    quint16 additionalTilesAt;
    quint32 romOffset;
    bool compressed;
};

class QDKEdit : public QTileEdit
{
    Q_OBJECT
public:
    explicit QDKEdit(QWidget *parent = 0);
    ~QDKEdit();
    bool loadAllLevels(QString romFile);
    bool saveAllLevels(QString romFile);
    QString getLevelInfo();

private:
    QByteArray LZSSDecompress(QDataStream *in, quint16 decompressedSize);
    QByteArray LZSSCompress(QByteArray *src);
    bool readLevel(QFile *src, quint8 id);
    bool readSGBPalettes(QFile *src);
    bool recompressLevel(quint8 id);
    bool expandRawTilemap(quint8 id);
    bool updateRawTilemap(quint8 id);
    void copyTileToSet(QFile *src, quint32 offset, QImage *img, quint16 tileID, quint8 tileSetID, bool compressed, quint8 tileCount, quint16 superOffset);
    bool getTileInfo(QFile *src);
    bool createTileSets(QFile *src, QGBPalette palette);
    bool createSprites(QFile *src, QGBPalette palette);
    void sortSprite(QImage *sprite, int id);
    void copyTile(QImage *img, int x1, int y1, int x2, int y2, bool mirror);
    void fillTile(QImage *img, int x, int y, int index);
    void swapTiles(QImage *img, int x1, int y1, int x2, int y2);
    QMap<QString, QPixmap *> spritePix;
    QMap<QString, QImage *> spriteImg;
    void updateTileset();
    quint8 getSpriteDefaultFlag(int id);
    void rebuildAddSpriteData(int id);

    QDKLevel levels[MAX_LEVEL_ID];
    QImage tilesets[MAX_TILESETS];
    quint8 tilesetBGP[MAX_TILESETS];
    QTileInfo tiles[256];
    QGBPalette sgbPal[512];
    int currentLevel;

    quint8 currentSize; // 0x00 -> 0x240 bytes for tilemap else 0x380 bytes
    quint8 currentMusic;
    quint8 currentTileset;
    quint16 currentTime;
    quint16 currentPalIndex;

    bool romLoaded;
    bool transparentSprites;
    static bool isSprite[256];

signals:
    void paletteChanged(int palette);
    void tilesetChanged(int set);
    void timeChanged(int time);
    void musicChanged(int music);
    void sizeChanged(int size);

private slots:
    void checkForLargeTile(int x, int y, int drawnTile);
    void updateSprite(int num);
    
public slots:
    void changeLevel(int id);
    void saveLevel();
    void changeTime(int time);
    void changePalette(int palette);
    void changeTileset(int tileset);
    void changeMusic(int music);
    void changeSize(int size);
    void changeSpriteTransparency(bool transparent);
    void addSprite(int id);
};

#endif // QDKEDIT_H


/* sprite tiles:

3a
44
47
48
4d
4e
4f
50
54
57
58
5a
5c
5e
64
6e
7a
7c
7f
80
84
86
88
8a
8e
90
92
94
96
98
9a
9d
a2
a4
a6
a8
aa
ac
ae
b0
b6
b8
ba
be
c0
c2
c6
c8
ca
cc

from add sprite data:
54
5C
5E
6E
70 <-- new; but elevator "tiles"
72 <-- new; but elevator "tiles"
7F
80
84
98
9A
B8
CC
 */
