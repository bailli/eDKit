#ifndef QDKEDIT_H
#define QDKEDIT_H

#include "QTileEdit.h"

#include <QtCore/QFile>
#include <QtCore/QList>
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
#define SGB_SYSTEM_PAL 0x786F0 // decompressed size 0x1000
// the SGB packet is always 51.(quint16)lvlNum+0x80.E4 00.E5.00.E6 00.C1.00.00 00.00.00.00
// it is not the level number...
// asm @ 0x0E70
#define PAL_TABELLE 0x6093b

struct QDKSprite : QSprite
{
    //quint8 tile;
    quint16 ramPos;
    quint32 levelPos;
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

    QGBPalette lvlPalette;
    quint16 pal;

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
    void copyTileToSet(QFile *src, quint32 offset, QImage *img, quint16 tileID, bool compressed, quint8 tileCount, quint16 superOffset);
    bool getTileInfo(QFile *src);
    bool createTileSets(QFile *src, QGBPalette palette);

    QDKLevel levels[MAX_LEVEL_ID];
    QImage tilesets[MAX_TILESETS];
    QTileInfo tiles[256];
    QGBPalette sgbPal[512];
    int currentLevel;
    bool romLoaded;

    //void paintEvent(QPaintEvent *e);
    void morePaint(QPainter *painter);
    
signals:

private slots:
    void checkForLargeTile(int x, int y);
    
public slots:
    void changeLevel(int id);
    
};

#endif // QDKEDIT_H
