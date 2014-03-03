#include "QDKEdit.h"
#include <QtCore/QDebug>
#include "MainWindow.h"
#include <QtCore/QDir>

QDKEdit::QDKEdit(QWidget *parent) :
    QTileEdit(parent), romLoaded(false)
{
    currentLevel = -1;
    dataIsChanged = false;
    tileDataIs16bit = true;

    lvlData.resize(0x280*2);
    for (int i = 0; i < 0x280*2; i+=2)
    {
        lvlData[i] = 0xFF;
        lvlData[i+1] = 0x00;
    }

    lvlDataStart = 0;
    lvlDataLength = 0x280;

    //spriteMode = true;

    setLevelDimension(32, 18);
    setTileSize(8, 8);

    // read all tilesets
    // these are just the "fallback" colours
    QGBPalette palette;
    palette[0] = Qt::white;
    palette[1] = Qt::black;
    palette[2] = Qt::yellow;
    palette[3] = Qt::red;

    QFile baseRom(BASE_ROM);
    baseRom.open(QIODevice::ReadOnly);
    getTileInfo(&baseRom);
    createTileSets(&baseRom, palette);
    baseRom.close();

    loadTileSet("tiles/tileset_00.png", 0xFF, 704);

    getMouse(true);
    connect(this, SIGNAL(singleTileChanged(int,int)), this, SLOT(checkForLargeTile(int,int)));
}

QDKEdit::~QDKEdit()
{
/*    for (int i = 0; i < MAX_LEVEL_ID; i++)
    {
        for (int j = 0; j < levels[i].sprites.size();)
    }*/
}

bool QDKEdit::saveAllLevels(QString romFile)
{
    QFile rom(romFile);
    if (!rom.exists())
        QFile::copy(BASE_ROM, romFile);

    rom.open(QIODevice::ReadWrite);

    QDataStream out(&rom);
    out.setByteOrder(QDataStream::LittleEndian);

    // make sure current "opened" level is writen back
    changeLevel(currentLevel);

    // get used rom banks and limits
    quint8 rombank[3];
    quint8 rombankLimit[2];
    quint8 currentBank;
    quint16 pointer;

    rombank[0] = ROMBANK_1;

    rom.seek(ROMBANK_POS_2);
    out >> rombank[1];

    rom.seek(ROMBANK_POS_3);
    out >> rombank[2];

    rom.seek(POINTER_TABLE + (MAX_LEVEL_ID * 2));
    currentBank = 0;

    for (int i = 0; i < LAST_LEVEL; i++)
    {
        // recompress level data - this checks wheather it was changed
        recompressLevel(i);

        // check if the rom bank is full
        pointer = (rom.pos() % 0x4000) + 0x4000;

        if ((pointer + levels[i].fullData.size()) > 0x8000)
        {
            if (currentBank > 1)
            {
                qWarning() << QString("Level %1: No more free space for level data! Aborting!").arg(i);
                rom.close();
                return false;
            }

            // remember new rombank limit
            rombankLimit[currentBank] = i;
            currentBank++;

            rom.seek(rombank[currentBank] * 0x4000);
            pointer = (rom.pos() % 0x4000) + 0x4000;
        }

        // update rom bank and offset
        levels[i].rombank = rombank[currentBank];
        levels[i].offset = rom.pos();

        // write level data
        if (rom.write(levels[i].fullData) != levels[i].fullData.size())
        {
            qWarning() << QString("Writing level %1 failed! Aborting!").arg(i);
            rom.close();
            return false;
        }
    }

    // write new pointers back to the table
    rom.seek(POINTER_TABLE);
    for (int i = 0; i < LAST_LEVEL; i++)
        out << (quint16)(levels[i].offset % 0x4000 + 0x4000);

    for (int i = LAST_LEVEL; i < MAX_LEVEL_ID; i++)
        out << (quint16)(levels[LAST_LEVEL].offset % 0x4000 + 0x4000);

    // correct rombank limits
    rom.seek(COMPARE_POS_1);
    out << rombankLimit[0];

    rom.seek(COMPARE_POS_2);
    out << rombankLimit[1];

    rom.close();

    return true;
}

bool QDKEdit::loadAllLevels(QString romFile)
{
    bool allOkay = true;

    QFile rom(romFile);
    rom.open(QIODevice::ReadOnly);

    QDataStream in(&rom);
    in.setByteOrder(QDataStream::LittleEndian);

    // we first get the three used rom banks from the asm code
    // the first bank (0x05) contains MAX_LEVEL_ID 16bit pointers
    // everything else is available for level data

    //get used rom banks and limits
    quint8 rombank[3];
    quint8 rombankLimit[2];
    quint8 bank;
    quint16 pointer;

    rombank[0] = ROMBANK_1;

    rom.seek(ROMBANK_POS_2);
    in >> rombank[1];

    rom.seek(ROMBANK_POS_3);
    in >> rombank[2];

    rom.seek(COMPARE_POS_1);
    in >> rombankLimit[0];

    rom.seek(COMPARE_POS_2);
    in >> rombankLimit[1];

    //read pointers
    rom.seek(POINTER_TABLE);
    for (int i = 0; i < MAX_LEVEL_ID; i++)
    {
        if (i < rombankLimit[0])
            bank = 0;
        else if (i < rombankLimit[1])
            bank = 1;
        else
            bank = 2;

        in >> pointer;

        levels[i].id = i;
        levels[i].rombank = rombank[bank];
        levels[i].offset = (rombank[bank] - 1 ) * 0x4000 + pointer;
        levels[i].fullDataUpToDate = false;

        //qDebug() << QString("Level %1 -- Rombank %2 -- Offset 0x%3").arg(i).arg(levels[i].rombank).arg(levels[i].offset, 6, 16, QChar('0'));
    }

    //read raw data
    for (int i = 0; i < MAX_LEVEL_ID; i++)
        if (!readLevel(&rom, i))
            allOkay = false;

    rom.close();

    romLoaded = allOkay;

    return allOkay;
}

bool QDKEdit::readLevel(QFile *src, quint8 id)
{    
    QDataStream in(src);
    in.setByteOrder(QDataStream::LittleEndian);

    quint8 byte, flag;

    // set level palette
    levels[id].lvlPalette[0] = Qt::white;
    levels[id].lvlPalette[1] = Qt::black;
    levels[id].lvlPalette[2] = Qt::yellow;
    levels[id].lvlPalette[3] = Qt::red;

    src->seek(levels[id].offset);

    in >> levels[id].size;
    in >> levels[id].music;
    in >> levels[id].tileset;
    in >> levels[id].time;

    // switch data
    levels[id].rawSwitchData.clear();
    in >> byte;
    if (byte == 0x00)
        levels[id].switchData = false;
    else
    {
        levels[id].switchData = true;
        levels[id].rawSwitchData.append(byte);
        for (int i = 0; i < 0x10; i++) // 1 byte already read!
        {
            in >> byte;
            levels[id].rawSwitchData.append(byte);
        }

        //0x90 bytes codiert
        bool secondRound = false;

        while (levels[id].rawSwitchData.size() < 0xA1)
        {
            if (secondRound)
                in >> byte;
            else
                secondRound = true;

            flag = byte;
            byte = 0x00;
            if (flag < 0x80)
                for (int i = 0; i < flag; i++)
                    levels[id].rawSwitchData.append(byte);
            else
            {
                flag &= 0x7F;
                for (int i = 0; i < flag; i++)
                {
                    in >> byte;
                    levels[id].rawSwitchData.append(byte);
                }
            }
        }

        if (levels[id].rawSwitchData.size() != 0xA1)
            qWarning() << QString("Level %1: incorrect size of switch data! size: %2").arg(id).arg(levels[id].rawSwitchData.size());
    }

    // additional sprite data?
    levels[id].rawAddSpriteData.clear();
    in >> byte;
    if (byte == 0x00)
        levels[id].addSpriteData = false;
    else
    {
        levels[id].addSpriteData = true;

        bool secondRound = false;

        //0x40 bytes codiert
        while (levels[id].rawAddSpriteData.size() < 0x40)
        {
            if (secondRound)
                in >> byte;
            else
                secondRound = true;

            flag = byte;
            byte = 0x00;
            if (flag < 0x80)
                for (int i = 0; i < flag; i++)
                    levels[id].rawAddSpriteData.append(byte);
            else
            {
                flag &= 0x7F;
                for (int i = 0; i < flag; i++)
                {
                    in >> byte;
                    levels[id].rawAddSpriteData.append(byte);
                }
            }
        }

        if (levels[id].rawAddSpriteData.size() != 0x40)
            qWarning() << QString("Level %1: incorrect size of add. sprite data! size: %2").arg(id).arg(levels[id].rawAddSpriteData.size());
    }

    //LZSS compressed tilemap
    quint16 uncompressedSize;

    if (levels[id].size == 0x00)
        uncompressedSize = 0x240;
    else
        uncompressedSize = 0x380;

    levels[id].rawTilemap = LZSSDecompress(&in, uncompressedSize);
    expandRawTilemap(id);

    //sprite data
    levels[id].sprites.clear();
    in >> byte;
    while (byte != 0x00)
    {
        QDKSprite sprite;
        sprite.id = byte;
        in >> sprite.ramPos;
        sprite.levelPos = sprite.ramPos - 0xDA75;
        sprite.pixelPerfect = false;
        sprite.x = sprite.levelPos % 32;
        sprite.y = sprite.levelPos / 32;
        sprite.sprite = new QPixmap(8*tiles[byte].w, 8*tiles[byte].h);
        sprite.sprite->fill(Qt::black);
        sprite.size = QSize(tiles[byte].w, tiles[byte].h);
        levels[id].sprites.append(sprite);

        in >> byte;
    }

    if (levels[id].sprites.size() > MAX_SPRITES)
        qWarning() << QString("Level %1: too many sprites! count: %2").arg(id).arg(levels[id].sprites.size());

    //copy the full raw data
    //no need to recompress if level data needs to be relocated
    quint32 size = src->pos() - levels[id].offset;
    src->seek(levels[id].offset);

    levels[id].fullData.clear();
    for (quint32 i = 0; i < size; i++)
    {
        in >> byte;
        levels[id].fullData.append(byte);
    }

    levels[id].fullDataUpToDate = true;

    if ((quint8)levels[id].fullData[size-1] != (quint8)0x00)
        qWarning() << QString("Level %1: last byte of raw data is not 0x00! byte %2; size %3").arg(id).arg(levels[id].fullData[size-1], 2, 16, QChar('0')).arg(size);

    return true;
}

bool QDKEdit::recompressLevel(quint8 id)
{
    QDKLevel *lvl = &levels[id];
    quint8 byte;
    quint8 count;

    if (lvl->fullDataUpToDate)
        return true;

    // delete old data
    lvl->fullData.clear();

    // simple values
    lvl->fullData.append(lvl->size);
    lvl->fullData.append(lvl->music);
    lvl->fullData.append(lvl->tileset);
    byte = lvl->time % 0x100;
    lvl->fullData.append(byte);
    byte = lvl->time / 0x100;
    lvl->fullData.append(byte);

    // switch data
    if (!lvl->switchData)
        lvl->fullData.append(QChar(0x00));
    else
    {
        count = 0;
        byte = 0;

        // copy the first 0x11 bytes uncompressed
        for (int i = 0; i < 0x11; i++)
            lvl->fullData.append((quint8)lvl->rawSwitchData[i]);

        for (int i = 0x11; i < lvl->rawSwitchData.size(); i++) //compress the remaining bytes
        {
            count = 0;

            while (((quint8)lvl->rawSwitchData[i] == (quint8)0x00) && (count <= 0x7F) && (i < lvl->rawSwitchData.size()))
            {
                count++;
                i++;
            }

            if (count != 0)
            {
                lvl->fullData.append(count);

                i--;
                continue;
            }

            while (((quint8)lvl->rawSwitchData[i] != (quint8)0x00) && (count <= 0x80) && (i < lvl->rawSwitchData.size()))
            {
                count++;
                i++;
            }

            if (count != 0)
            {
                byte = 0x80 + count;
                lvl->fullData.append(byte);
                i--;

                for (int j = 0; j < count; j++)
                    lvl->fullData.append((quint8)lvl->rawSwitchData[i-count+1+j]);
            }

        }

    }

    if (!lvl->addSpriteData)
        lvl->fullData.append(QChar(0x00));
    else // compress additional sprite data (same as above)
    {
        count = 0;
        byte = 0;

        for (int i = 0; i < lvl->rawAddSpriteData.size(); i++)
        {
            count = 0;

            while (((quint8)lvl->rawAddSpriteData[i] == (quint8)0x00) && (count <= 0x7F) && (i < lvl->rawAddSpriteData.size()))
            {
                count++;
                i++;
            }

            if (count != 0)
            {
                lvl->fullData.append(count);

                i--;
                continue;
            }

            while (((quint8)lvl->rawAddSpriteData[i] != (quint8)0x00) && (count <= 0x80) && (i < lvl->rawAddSpriteData.size()))
            {
                count++;
                i++;
            }

            if (count != 0)
            {
                byte = 0x80 + count;
                lvl->fullData.append(byte);
                i--;

                for (int j = 0; j < count; j++)
                    lvl->fullData.append((quint8)lvl->rawAddSpriteData[i-count+1+j]);
            }

        }
    }

    // drop 16bit tiles
    updateRawTilemap(id);

    // compress tilemap
    lvl->fullData.append(LZSSCompress(&lvl->rawTilemap));

    // add sprite tiles+ram position
    for (int i = 0; i < lvl->sprites.size(); i++)
    {
        lvl->fullData.append((quint8)lvl->sprites.at(i).id);
        byte = lvl->sprites.at(i).ramPos % 0x100;
        lvl->fullData.append(byte);
        byte = lvl->sprites.at(i).ramPos / 0x100;
        lvl->fullData.append(byte);
    }

    // 0x00 marks level end
    lvl->fullData.append(QChar(0x00));

    lvl->fullDataUpToDate = true;

    /*QFile file("recompessed.lvl");
    file.open(QIODevice::WriteOnly);
    file.write(lvl->fullData);
    file.close();*/

    return true;
}

bool QDKEdit::updateRawTilemap(quint8 id)
{
    levels[id].rawTilemap.clear();
    for (int i = 0; i < levels[id].displayTilemap.size(); i+=2)
    {
        if ((quint8)levels[id].displayTilemap[i+1] == 0x00)
            levels[id].rawTilemap.append((quint8)levels[id].displayTilemap[i]);
        else
            levels[id].rawTilemap.append(QChar(0xFF));
    }

    return true;
}

void QDKEdit::checkForLargeTile(int x, int y)
{
    int i = y * levelDimension.width() + x;

    if ((int)lvlData[i*2] == emptyTile)
        return;

    if (tiles[tileToDraw].count > 1)
    {
        int tilePos = 0x100 + tiles[tileToDraw].additionalTilesAt;

        for (int j = 0; j < tiles[tileToDraw].h; j++)
            for (int k = 0; k < tiles[tileToDraw].w; k++)
            {
                if (!k && !j)
                    continue;

                lvlData[lvlDataStart + (i+k+(j*0x20))*2] = (tilePos & 0xFF);
                lvlData[lvlDataStart + (i+k+(j*0x20))*2 + 1] = (tilePos >> 8);
                tilePos++;
            }
    }
}

bool QDKEdit::expandRawTilemap(quint8 id)
{
    levels[id].displayTilemap.clear();

    //expand to 16bit for additional tiles
    for (int i = 0; i < levels[id].rawTilemap.size(); i++)
    {
        levels[id].displayTilemap.append((quint8)levels[id].rawTilemap[i]);
        levels[id].displayTilemap.append(QChar(0x00));
    }   

    quint8 tileID;
    quint16 tilePos;    

    // expand super tiles
    for (int i = 0; i < levels[id].rawTilemap.size(); i++)
    {
        tileID = levels[id].rawTilemap[i];
        if (tileID == 0xFF)
            continue;

        if (tiles[tileID].count > 1)
        {
            tilePos = 0x100 + tiles[tileID].additionalTilesAt;

            for (int j = 0; j < tiles[tileID].h; j++)
                for (int k = 0; k < tiles[tileID].w; k++)
                {
                    if (!k && !j)
                        continue;

                    levels[id].displayTilemap[(i+k+(j*0x20))*2] = (tilePos & 0xFF);
                    levels[id].displayTilemap[(i+k+(j*0x20))*2 + 1] = (tilePos >> 8);
                    tilePos++;
                }
        }
    }

    return true;
}

void QDKEdit::copyTileToSet(QFile *src, quint32 offset, QImage *img, quint16 tileID, bool compressed = false, quint8 tileCount = 1, quint16 superOffset = 0)
{
    src->seek(offset);
    QDataStream *in;

    quint16 tileX, tileY;

    quint8 low, high, pixel;

    QByteArray decompressed;

    if (compressed)
    {
        QDataStream decomp(src);
        decompressed = LZSSDecompress(&decomp, 0x10*tileCount);
        in = new QDataStream(decompressed);
    }
    else
        in = new QDataStream(src);


    in->setByteOrder(QDataStream::LittleEndian);

    for (int t = 0; t < tileCount; t++)
    {
        tileY = (tileID / 16) * 8;
        tileX = (tileID % 16) * 8;

        for (int i = 0; i < 8; i++)
        {
            (*in) >> low;
            (*in) >> high;

            for (int j = 0; j < 8; j++)
            {
                pixel = low & 0x80;
                pixel >>= 1;
                pixel |= high & 0x80;
                pixel >>= 6;

                low <<= 1;
                high <<= 1;

                img->setPixel(tileX+j, tileY+i, pixel);
            }
        }

        tileID = 0x100 + superOffset + t;
    }

    delete in;
}

bool QDKEdit::getTileInfo(QFile *src)
{
    QDataStream in(src);
    in.setByteOrder(QDataStream::LittleEndian);

    quint16 pointer;
    quint8 bank, tilesCount;
    quint32 offset;
    quint16 addOffset = 0;
    quint8 byte;

    for (int i = 0; i < 255; i++)
    {
        // rombank 0x01 offset 0x60E5 is a table with pointers for all tiles
        src->seek(0x60E5 + 2*i);
        in >> pointer;

        tiles[i].setSpecific = false;

        // the pointer points to some meta data for every "tile"
        // like tiles count and size of "super tiles"

        // first try pointer+4 for count
        src->seek(pointer+4);
        in >> tilesCount;
        tiles[i].type = 4;

        if (tilesCount == 0)
        {
            // next try pointer+5 for count
            src->seek(pointer+5);
            in >> tilesCount;
            tiles[i].type = 5;
        }
        if (tilesCount == 0)
        {
            // the tiles count includes "animations"
            src->seek(pointer+2);
            in >> tilesCount;
            tiles[i].type = 2;

            // another exception...
            if ((i == 0x9A) || (i == 0x8A) || (i == 0x9D) || (i == 0x9E) || (i == 0x54) || (i == 0x43) || (i == 0x79) || (i == 0xAF) || (i == 0x53) || (i == 0x65))
            {
                src->seek(pointer+0xD);
                in >> byte;
                if (byte >= tilesCount)
                    tilesCount = 0;
                else
                    tilesCount -= byte;
            }
        }

        // which tiles are compressed and which are not? if tilesCount >= 4 ???
        tiles[i].compressed = (tilesCount >= 4);
/*
        if (tilesCount == 0)
        {
            // at last try pointer+3 for sprite tiles like DK and Mario
            // the game code checks these tiles earlier but my code
            // corrupts some tiles like 0x41 the plant in level 14
            src->seek(pointer+3);
            in >> tilesCount;
            tiles[i].type = 3;
        }
*/
        // get data needed for super tiles
        //tiles[i].count = tilesCount;
        src->seek(pointer+8);
        in >> tiles[i].h;
        in >> tiles[i].w;
        tiles[i].count = tiles[i].h * tiles[i].w; // just get enough tiles to display the full super tile - no animations

        if (tiles[i].count == 0)
            continue;

        if (tiles[i].count > 1)
        {
            tiles[i].additionalTilesAt = addOffset;
            addOffset += (tiles[i].count - 1);
        }

        src->seek(pointer+6);
        in >> pointer;
        // and another pointer which either points to a rombank | pointer table
        // or directly to the (compressed) tile data

        // why should everything be nicely order in a rom bank | pointer table ?
        // that would be too easy -- maybe it is <= instead of <
        if (pointer < 0x7E00)
        {
            if (i < 0x80)
                offset = 0xD*0x4000 + pointer;
            else if (i < 0xAF)
                offset = 0xC*0x4000 + pointer;
            else
                offset = 0x1B*0x4000 + pointer;

            tiles[i].romOffset = offset;

            continue;
        }

        src->seek(0xd*0x4000 + pointer);
        in >> bank;
        in >> pointer;


        offset = (bank-1)*0x4000 + pointer;

        tiles[i].setSpecific = true;
        tiles[i].romOffset = offset;
    }

    return true;
}

bool QDKEdit::createTileSets(QFile *src, QGBPalette palette)
{
    QDataStream in(src);
    in.setByteOrder(QDataStream::LittleEndian);

    quint8 firstSet, secondSet;
    quint16 pointer;
    quint32 offset;

    QImage fullSet(128, 352, QImage::Format_Indexed8);
    fullSet.setColor(0, palette[0].rgb());
    fullSet.setColor(1, palette[1].rgb());
    fullSet.setColor(2, palette[2].rgb());
    fullSet.setColor(3, palette[3].rgb());

    QDir dir;
    if (!dir.exists("tiles"))
        dir.mkdir("tiles");

    for (int id = 0; id < MAX_TILESETS; id++)
    {
        if (QFile::exists(QString("tiles/tileset_%1.png").arg(id, 2, 16, QChar('0'))))
        {
            tilesets[id].load(QString("tiles/tileset_%1.png").arg(id, 2, 16, QChar('0')));
            continue;
        }

        fullSet.fill(0);

        // get the two sub tilesets offsets
        // rombank 0xC offset 0x4EEB is a table of two offsets for every tileset
        src->seek(0xB*0x4000 + 0x4EEB + (id*2));
        in >> firstSet;
        in >> secondSet;

        for (int i = 0; i < 255; i++) // since 0xFF is the empty tile we just skip it altogether
        {
            if (tiles[i].setSpecific)
            {
                // this is finally the last pointer before the actual tile data...
                // here are the "same" tiles from different tilesets grouped together
                if ((i < 0xCD) || (i == 0xFD))
                    src->seek(tiles[i].romOffset + firstSet);
                else
                    src->seek(tiles[i].romOffset + secondSet);

                in >> pointer;

                offset = tiles[i].romOffset + pointer;
            }
            else
                offset = tiles[i].romOffset;

            copyTileToSet(src, offset, &fullSet, i, tiles[i].compressed, tiles[i].count, tiles[i].additionalTilesAt);
        }

        tilesets[id] = fullSet;
        fullSet.save(QString("tiles/tileset_%1.png").arg(id, 2, 16, QChar('0')));
    }

    return true;
}

QByteArray QDKEdit::LZSSDecompress(QDataStream *in, quint16 decompressedSize)
{
    in->setByteOrder(QDataStream::LittleEndian);

    QByteArray decompressed;

    quint8 byte, flags, len;
    quint16 start;
    quint32 offset;

    while (decompressed.size() < decompressedSize)
    {
        (*in) >> flags;

        for (int i = 0; i < 8; i++)
        {
            if (flags & 0x1) // raw data
            {
                (*in) >> byte;
                decompressed.append(byte);
            }
            else // copy data
            {
                (*in) >> byte;
                (*in) >> len;

                //start is actually 12bit and length only 4bit
                start = byte + (0x100 * (len / 0x10));
                len = len % 0x10;

                len += 3;
                offset = decompressed.size() - start;
                //qDebug() << "start " << start << " length " << len;

                for (int j = 0; j < len; j++)
                    decompressed.append((quint8)decompressed.at(offset+j));

            }

            flags >>= 1;

            if (decompressed.size() >= decompressedSize)
                break;
        }
    }

    return decompressed;
}

QByteArray QDKEdit::LZSSCompress(QByteArray *src)
{
    QByteArray compressed;
    quint8 flagByte, flagBit, byte;
    qint32 srcPos, flagBytePos, matchLength, matchRelativePos, currentMatchLength, j, matchEnd;

    flagByte = 0x1;
    flagBytePos = 0;
    flagBit = 0x2;
    srcPos = 0;

    // add first flag byte (place holder) and add first raw data byte
    compressed.append(flagByte);
    compressed.append((quint8)src->at(srcPos++));

    while (srcPos < src->size())
    {
        // find match in previous data

        // start max 4096 bytes back from current pos
        if (srcPos <= 4096)
            matchEnd = 0;
        else
            matchEnd = srcPos - 4096;

        matchLength = 0;

        // try from current position
        //for (int i = matchEnd; i < srcPos; i++)
        for (int i = srcPos-1; i >= matchEnd; i--)
        {
            // start compare for every i
            j = 0;
            while ((*src)[i+j] == (*src)[srcPos+j])
            {
                j++;
                // make sure to stay inside 4 bits (+3) and inside byte array
                if ((j >= 18) || (i+j >= src->size()) || (srcPos+j >= src->size()))
                    break;
            }

            // remember best match
            currentMatchLength = j;
            if (matchLength < currentMatchLength)
            {
                matchLength = currentMatchLength;
                matchRelativePos = (srcPos - i);
            }

            // no point in looking further if this is max length
            if (currentMatchLength == 18)
                break;
        }

        // check if it is long enough
        if (matchLength < 3) // too short
        {
            flagByte |= flagBit; // raw data - not compressed
            compressed.append((quint8)src->at(srcPos++)); // add raw data byte
        }
        else
        {
            // nothing to do with the flag byte

            // move srcPos after length
            srcPos += matchLength;

            // adjust length
            matchLength -= 3;

            // add relative start and length to copy
            if (matchRelativePos > 255)
            {
                byte = matchRelativePos % 0x100;
                compressed.append(byte);
                byte = matchLength + (0x10 * (matchRelativePos / 0x100));
                compressed.append(byte);
            }
            else
            {
                compressed.append((quint8)matchRelativePos);
                compressed.append((quint8)matchLength);
            }
        }

        if (flagBit == 0x80) // flag byte fully populated
        {
            compressed[flagBytePos] = flagByte; // write correct flag byte back to original position

            // reset flag byte and counter bit
            flagByte = 0;
            flagBit = 0x1;

            // save new flag byte position
            flagBytePos = compressed.size();

            if (srcPos < src->size()) // make sure there is more data
                compressed.append((quint8)0x1D); // place holder for flag byte
        }
        else // shift flag bit
            flagBit <<= 1;
    }

    // write back last incomplete flag byte
    if (flagBit != 1)
        compressed[flagBytePos] = flagByte;

    return compressed;
}

void QDKEdit::changeLevel(int id)
{
    if (!romLoaded)
        return;

    if ((currentLevel != -1) && (dataIsChanged))
    {
        levels[currentLevel].fullDataUpToDate = false;
        levels[currentLevel].displayTilemap.clear();
        levels[currentLevel].displayTilemap.append(lvlData);
        levels[currentLevel].sprites.clear();
        for (int i = 0; i < sprites.size(); i++)
        {
            levels[currentLevel].sprites.append((QDKSprite&)sprites[i]);
            levels[currentLevel].sprites[i].levelPos = levels[currentLevel].sprites[i].y*levelDimension.width() + levels[currentLevel].sprites[i].x;
            levels[currentLevel].sprites[i].ramPos = levels[currentLevel].sprites[i].levelPos + 0xDA75;
        }
    }

    dataIsChanged = false;
    currentLevel = id;

    lvlData.clear();

    lvlData.append(levels[currentLevel].displayTilemap);

    lvlDataStart = 0;
    lvlDataLength = lvlData.size();

    if (levels[currentLevel].size == 0x00)
        setLevelDimension(32, 18);
    else
        setLevelDimension(32, 28);

    // adjust color table
    tilesets[levels[currentLevel].tileset].setColor(0, levels[id].lvlPalette[0].rgb());
    tilesets[levels[currentLevel].tileset].setColor(1, levels[id].lvlPalette[1].rgb());
    tilesets[levels[currentLevel].tileset].setColor(2, levels[id].lvlPalette[2].rgb());
    tilesets[levels[currentLevel].tileset].setColor(3, levels[id].lvlPalette[3].rgb());

    tileSet.convertFromImage(tilesets[levels[currentLevel].tileset]);

    sprites.clear();
    for (int i = 0; i < levels[currentLevel].sprites.size(); i++)
        sprites.append(levels[currentLevel].sprites.at(i));

    update();

    emit dataChanged();
}

void QDKEdit::morePaint(QPainter *painter)
{
    // paint all sprites as black box
    //if (drawSprites)
        for (int i = 0; i < levels[currentLevel].sprites.size(); i++)
        {
            int x,y;
            x = levels[currentLevel].sprites.at(i).levelPos % 32;
            y = levels[currentLevel].sprites.at(i).levelPos / 32;
            painter->setBrush(Qt::black);
            painter->drawRect(tileSize.width()*x, tileSize.height()*y, 8, 8);
        }
}

QString QDKEdit::getLevelInfo()
{
    QString str = "";
    int id = currentLevel;

    str += QString("Size %1; Tileset %2; Music %3; Time %4; full size %5").arg(levels[id].size, 2, 16, QChar('0')).arg(levels[id].tileset, 2, 16, QChar('0')).arg(levels[id].music, 2, 16, QChar('0')).arg(levels[id].time).arg(levels[id].fullData.size());

    if (!levels[id].switchData)
        str += QString("; No switch data");

    if (!levels[id].addSpriteData)
        str += QString("; No add. Sprite data");

    str += QString("\n");

    QString tmp;

    if (levels[id].switchData) // 0x11 + 0x90 bytes
    {
        tmp = "Switch data:";
        for (int i = 0; i < levels[id].rawSwitchData.size(); i++)
        {
            if (i % 16 == 0)
                tmp += QString("\n%1: ").arg(i, 2, 16, QChar('0'));
            tmp += QString("%1").arg((quint8)levels[id].rawSwitchData[i], 2, 16, QChar('0'));

            if (i % 2 == 1)
                tmp += " ";
        }
        str += tmp + "\n";
    }

    if (levels[id].addSpriteData) // 0x40 bytes
    {
        tmp = "Add. sprite data:";
        for (int i = 0; i < levels[id].rawAddSpriteData.size(); i++)
        {
            if (i % 16 == 0)
                tmp += QString("\n%1: ").arg(i, 2, 16, QChar('0'));
            tmp += QString("%1").arg((quint8)levels[id].rawAddSpriteData[i], 2, 16, QChar('0'));

            if (i % 2 == 1)
                tmp += " ";
        }
        str += tmp + "\n";
    }

    return str;
}