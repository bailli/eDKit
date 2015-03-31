#include "QDKEdit.h"
#include <QtCore/QDebug>
#include "MainWindow.h"
#include "QTileSelector.h"
#include <QtCore/QDir>
#include <QtGui/QBitmap>
#include <QtGui/QMouseEvent>

bool QDKEdit::isSprite[] = {
    false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false,
    false, false, true , false, false, false, false, false,
    false, false, false, false, true , false, false, true ,
    true , false, false, false, false, true , true , true ,
    true , false, false, false, true , false, false, true ,
    true , false, true , false, true , false, true , false,
    false, false, false, false, true , false, false, false,
    false, false, false, false, false, false, true , false,
    true, false, true, false, false, false, false, false,
    false, false, true , false, true , false, false, true ,
    true , false, false, false, true , false, true , false,
    true , false, true , false, false, false, true , false,
    true , false, true , false, true , false, true , false,
    true , false, true , false, false, true , false, false,
    false, false, true , false, true , false, true , false,
    true , false, true , false, true , false, true , false,
    true , false, false, false, false, false, true , false,
    true , false, true , false, false, false, true , false,
    true , false, true , false, false, false, true , false,
    true , false, true , false, true , false, false, false,
    false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false
};


QDKEdit::QDKEdit(QWidget *parent) :
    QTileEdit(parent), switchMode(false), switchToEdit(-1), romLoaded(false)
{
    currentLevel = -1;
    dataIsChanged = false;
    tileDataIs16bit = true;
    spriteContext = false;

    fillTileNames();
    fillSpriteNames();

    lvlData.resize(0x280*2);
    for (int i = 0; i < 0x280*2; i+=2)
    {
        lvlData[i] = 0xFF;
        lvlData[i+1] = 0x00;
    }

    lvlDataStart = 0;
    lvlDataLength = 0x280;

    setLevelDimension(32, 18);
    setTileSize(8, 8);

    // read all tilesets
    // these are just the "fallback" colours
    QGBPalette palette;
    palette[0] = Qt::white;
    palette[1] = Qt::black;
    palette[2] = Qt::yellow;
    palette[3] = Qt::red;

    transparentSprites = true;

    QFile baseRom(BASE_ROM);
    baseRom.open(QIODevice::ReadOnly);
    getTileInfo(&baseRom);
    createTileSets(&baseRom, palette);
    createSprites(&baseRom, palette);
    readSGBPalettes(&baseRom);
    baseRom.close();

    loadTileSet("tiles/tileset_00.png", 0xFF, 704);

    getMouse(true);
    connect(this, SIGNAL(singleTileChanged(int,int,int)), this, SLOT(checkForLargeTile(int,int,int)));
    connect(this, SIGNAL(flagByteChanged(int)), this, SLOT(updateSprite(int)));
}

QDKEdit::~QDKEdit()
{
    qDeleteAll(spritePix);
    qDeleteAll(spriteImg);
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

    //write SGB palettes for all levels
    for (int i = 0; i < LAST_LEVEL; i++)
        if (i < 4) // this may cause the game to freeze for "high" palette values !
        {
            rom.seek(PAL_ARCADE + (i * 6));
            out << (quint8)(levels[i].paletteIndex - 0x180 + 0xC8);
        }
        else
        {
            rom.seek(PAL_TABLE + ((i-4) * 6));
            out << (quint8)(levels[i].paletteIndex - 0x180);
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
    quint16 address;

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
        sprite.sprite = NULL;
        sprite.rotate = BOTTOM;
        sprite.flagByte = getSpriteDefaultFlag(byte);
        sprite.size = QSize(tiles[byte].w, tiles[byte].h);

        if (sprite.id == 0x54)
            sprite.drawOffset.setX(-0.5f);

        levels[id].sprites.append(sprite);

        in >> byte;
    }

    if (levels[id].sprites.size() > MAX_SPRITES)
        qWarning() << QString("Level %1: too many sprites! count: %2").arg(id).arg(levels[id].sprites.size());

    if (levels[id].addSpriteData)
    {
        levels[id].switches.clear();
        for (int i = 0; i < levels[id].rawAddSpriteData.size(); i+=4)
        {
            byte = (quint8)levels[id].rawAddSpriteData[i];

            //asm @ 0x46E6 rombank 0x0C
            //missing keyholes from lvl95 0xB8 - flag only 00 or 01?
            //missing DK sprites 9A 6E CC - flag only 00 or 01 ?
            // 9A handled like 80 98 84
            // 6E CC handled identical

            // the original game contains much garbage btw...

            if ((byte != 0x7F) && (byte != 0x98) && (byte != 0x80) && (byte != 0x54) && (byte != 0x70) && (byte != 0x72) &&
                (byte != 0x84) && (byte != 0xB8) && (byte != 0x9A) && (byte != 0x6E) && (byte != 0xCC))
                continue;

            address = (quint8)levels[id].rawAddSpriteData[i+1] + (0x100 * (quint8)levels[id].rawAddSpriteData[i+2]);
            flag = (quint8)levels[id].rawAddSpriteData[i+3];

            // elevator actually correspondes to a tile
            if ((byte == 0x70) || (byte == 0x72))
            {
                if (byte == (quint8)levels[id].rawTilemap[address - 0xD44D])
                {
                    //we found a correct tile
                    //so we need a new sprite
                    QDKSprite sprite;
                    sprite.id = byte;
                    sprite.ramPos = address;
                    sprite.levelPos = sprite.ramPos - 0xD44D;
                    sprite.pixelPerfect = false;
                    sprite.x = sprite.levelPos % 32;
                    sprite.y = sprite.levelPos / 32;
                    sprite.sprite = NULL;
                    sprite.rotate = BOTTOM;
                    sprite.flagByte = flag;
                    sprite.size = QSize(tiles[byte].w, tiles[byte].h);
                    levels[id].sprites.append(sprite);

                }

                continue;
            }

            // these sprites may get mirrored
            for (int j = 0; j < levels[id].sprites.size(); j++)
                if (levels[id].sprites.at(j).ramPos == address)
                {
                    levels[id].sprites[j].flagByte = flag;

                    //actually the game engine takes 0x7F sprite properties
                    //without checking the RAM address...
                    if (byte == 0x7F)
                        levels[id].sprites[j].rotate = flag;
                    else if ((byte == 0x80) || (byte == 0x98))
                        levels[id].sprites[j].rotate = ((flag+1) & 1);
                    break;
                }
        }
    }

    if (levels[id].switchData)
    {
        for (int i = 8; i > 0; i--)
        {
            if (!(levels[id].rawSwitchData[0] & (1 << (i-1))))
                continue;

            QDKSwitch newSwitch;
            quint8 connectedObjectFlags = levels[id].rawSwitchData[i];
            newSwitch.state = levels[id].rawSwitchData[8+i];
            newSwitch.ramPos = (quint8)levels[id].rawSwitchData[17 + ((i-1) * 18)] + (0x100 * (quint8)levels[id].rawSwitchData[17 + ((i-1) * 18) + 1]);
            newSwitch.levelPos = newSwitch.ramPos - 0xD44D;
            newSwitch.x = newSwitch.levelPos % 0x20;
            newSwitch.y = newSwitch.levelPos / 0x20;

            for (int j = 8; j > 0; j--)
            {
                if (!(connectedObjectFlags & (1 << (j-1))))
                    continue;

                QDKSwitchObject newObj;
                newObj.ramPos = (quint8)levels[id].rawSwitchData[17 + ((i-1) * 18) + (2*j)] + (0x100 * (quint8)levels[id].rawSwitchData[17 + ((i-1) * 18) + (2*j) + 1]);
                if (newObj.ramPos < 0xDA00) //tile
                {
                    newObj.isSprite = false;
                    newObj.levelPos = newObj.ramPos - 0xD44D;
                }
                else
                {
                    newObj.isSprite = true;
                    newObj.levelPos = newObj.ramPos - 0xDA75;
                }

                newObj.x = newObj.levelPos % 0x20;
                newObj.y = newObj.levelPos / 0x20;

                newSwitch.connectedTo.append(newObj);
            }

            levels[id].switches.append(newSwitch);
        }
    }

    //copy the full raw data
    //no need to recompress if level data needs to be relocated
    quint32 size = src->pos() - levels[id].offset;
    src->seek(levels[id].offset);

    //get palette number
    levels[id].fullData.clear();
    for (quint32 i = 0; i < size; i++)
    {
        in >> byte;
        levels[id].fullData.append(byte);
    }

    if (id < 4)
    {
        src->seek(PAL_ARCADE + (id * 6));
        in >> byte;
        byte -= 0xC8;
    }
    else
    {
        src->seek(PAL_TABLE + (id-4)*8);
        in >> byte;
    }

    levels[id].paletteIndex = 0x180 + byte;

    if (levels[id].paletteIndex >= 0x200)
    {
        if (id <= LAST_LEVEL)
            qWarning() << QString("Level %1: invalid SGB palette 0x%2! default to 0x180").arg(id).arg(levels[id].paletteIndex, 4, 16, QChar('0'));
        levels[id].paletteIndex = 0x180;
    }

    levels[id].fullDataUpToDate = true;

    if ((quint8)levels[id].fullData[size-1] != (quint8)0x00)
        qWarning() << QString("Level %1: last byte of raw data is not 0x00! byte %2; size %3").arg(id).arg(levels[id].fullData[size-1], 2, 16, QChar('0')).arg(size);

    return true;
}

bool QDKEdit::readSGBPalettes(QFile *src)
{
    src->seek(SGB_SYSTEM_PAL);
    QDataStream in(src);
    in.setByteOrder(QDataStream::LittleEndian);

    quint16 color;
    int rgb;
    QColor tmp;

//    Bit 0-4   - Red Intensity   (0-31)
//    Bit 5-9   - Green Intensity (0-31)
//    Bit 10-14 - Blue Intensity  (0-31)
//    Bit 15    - Not used (zero)

    QByteArray decompressed = LZSSDecompress(&in, 0x1000);

    QDataStream pal(decompressed);
    pal.setByteOrder(QDataStream::LittleEndian);

    for (int i = 0; i < 512; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            pal >> color;
            rgb = (color & 0x1F)* 8;
            if (rgb == 248)
                rgb = 255;
            tmp.setRed(rgb);
            rgb = ((color >> 5) & 0x1F) * 8;
            if (rgb == 248)
                rgb = 255;
            tmp.setGreen(rgb);
            rgb = ((color >> 10) & 0x1F) * 8;
            if (rgb == 248)
                rgb = 255;
            tmp.setBlue(rgb);
            sgbPal[i][j] = tmp;
        }
    }

    return true;
}

void QDKEdit::rebuildSwitchData(int id)
{
    QDKLevel *lvl = &levels[id];

    lvl->rawSwitchData.clear();

    if (lvl->switches.isEmpty())
    {
        lvl->switchData = false;
        return;
    }

    lvl->switchData = true;

    lvl->rawSwitchData.fill((quint8)0x00, 0x11 + 0x90);

    int switchCount, objCount;
    switchCount = lvl->switches.size();

    if (switchCount > 8)
    {
        qWarning() << QString("Level %1: More than 8 switches found! Truncated to 8!").arg(id);
        switchCount = 8;
    }

    for (int i = 0; i < switchCount; i++)
    {
        //switch state
        lvl->rawSwitchData[i + 9] = lvl->switches.at(switchCount-i-1).state;
        //switch address
        lvl->rawSwitchData[17 + (i * 18)] = (quint8)(lvl->switches.at(switchCount-i-1).ramPos % 0x100);
        lvl->rawSwitchData[17 + (i * 18) + 1] = (quint8)(lvl->switches.at(switchCount-i-1).ramPos / 0x100);

        objCount = lvl->switches.at(switchCount-i-1).connectedTo.size();
        if (objCount > 8)
        {
            qWarning() << QString("Level %1: More than 8 connected objects for switch %2! Truncated to 8!").arg(id).arg(switchCount-i-1);
            objCount = 8;
        }

        for (int j = 0; j < objCount; j++)
        {
            //object addresses
            lvl->rawSwitchData[17 + (i * 18) + (2*j) + 2] = (quint8)(lvl->switches.at(switchCount-i-1).connectedTo.at(objCount-j-1).ramPos % 0x100);
            lvl->rawSwitchData[17 + (i * 18) + (2*j) + 3] = (quint8)(lvl->switches.at(switchCount-i-1).connectedTo.at(objCount-j-1).ramPos / 0x100);
        }
        //object use flag
        lvl->rawSwitchData[i + 1] = (1 << lvl->switches.at(switchCount-i-1).connectedTo.size()) - 1;
    }
    //switch use flag
    lvl->rawSwitchData[0] = (1 << lvl->switches.size()) - 1;
}

void QDKEdit::rebuildAddSpriteData(int id)
{
    QDKLevel *lvl = &levels[id];

    lvl->rawAddSpriteData.clear();
    lvl->rawAddSpriteData.fill((quint8)0x00, 0x40);
    lvl->addSpriteData = true;

    int flagCount = 0;

    for (int i = lvl->sprites.size() - 1; i >= 0; i--)
    {
        if (lvl->sprites.at(i).flagByte != getSpriteDefaultFlag(lvl->sprites.at(i).id))
        {
            if (flagCount >= 0x20)
            {
                qWarning() << QString("Level %1: More than 32 sprites with initialized flag byte! Truncated to 32!").arg(id);
                break;
            }

            //check for elevator in tilemap
            if ((lvl->sprites.at(i).id == 0x70) || (lvl->sprites.at(i).id == 0x72))
                if (lvl->rawTilemap[lvl->sprites.at(i).levelPos] != lvl->sprites.at(i).id)
                    continue;

            //IdLo HiFb
            lvl->rawAddSpriteData[flagCount * 4] = (quint8)lvl->sprites.at(i).id;
            lvl->rawAddSpriteData[flagCount * 4 + 1] = (quint8)(lvl->sprites.at(i).ramPos % 0x100);
            lvl->rawAddSpriteData[flagCount * 4 + 2] = (quint8)(lvl->sprites.at(i).ramPos / 0x100);
            lvl->rawAddSpriteData[flagCount * 4 + 3] = (quint8)lvl->sprites.at(i).flagByte;

            flagCount++;
        }
    }

    if (!flagCount)
    {
        lvl->rawAddSpriteData.clear();
        lvl->addSpriteData = false;
    }
}

bool QDKEdit::recompressLevel(quint8 id)
{
    QDKLevel *lvl = &levels[id];
    quint8 byte;
    quint8 count;

    if (lvl->fullDataUpToDate)
        return true;

    // drop 16bit tiles
    updateRawTilemap(id);

    //rebuild sprite properties aka additional sprite data
    rebuildAddSpriteData(id);

    //rebuild switch connections
    rebuildSwitchData(id);

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

            while (((quint8)lvl->rawSwitchData[i] == (quint8)0x00) && (count < 0x7F) && (i < lvl->rawSwitchData.size()))
            {
                count++;
                i++;
            }

            if (count != 0)
            {
                lvl->fullData.append(count);
                count = 0;
                i--;
                continue;
            }

            while (((quint8)lvl->rawSwitchData[i] != (quint8)0x00) && (count < 0x7F) && (i < lvl->rawSwitchData.size()))
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

                count = 0;
            }

        }
        if (count != 0)
            qWarning() << QString("Level %1: recompressing switch data; count == %2").arg(id).arg(count);
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

            while (((quint8)lvl->rawAddSpriteData[i] == (quint8)0x00) && (count < 0x7F) && (i < lvl->rawAddSpriteData.size()))
            {
                count++;
                i++;
            }

            if (count != 0)
            {
                lvl->fullData.append(count);
                count = 0;
                i--;
                continue;
            }

            while (((quint8)lvl->rawAddSpriteData[i] != (quint8)0x00) && (count < 0x7F) && (i < lvl->rawAddSpriteData.size()))
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

                count = 0;
            }
        }
        if (count != 0)
            qWarning() << QString("Level %1: recompressing sprite flag; count == %2").arg(id).arg(count);
    }

    // compress tilemap
    lvl->fullData.append(LZSSCompress(&lvl->rawTilemap));

    // add sprite tiles+ram position
    for (int i = 0; i < lvl->sprites.size(); i++)
    {
        // do not add the pseudo elevator sprite
        if ((lvl->sprites.at(i).id == 0x70) || (lvl->sprites.at(i).id == 0x72))
            continue;

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

void QDKEdit::checkForLargeTile(int x, int y, int drawnTile)
{
    if (drawnTile == emptyTile)
        return;

    int i = y * levelDimension.width() + x;

    if ((x < 0) || (y < 0) || (i*2 >= lvlData.size()) || ((int)lvlData[i*2] == emptyTile))
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

void QDKEdit::copyTileToSet(QFile *src, quint32 offset, QImage *img, quint16 tileID, quint8 tileSetID = 0, bool compressed = false, quint8 tileCount = 1, quint16 superOffset = 0)
{       
    QDataStream *in;
    quint16 tileX, tileY, pointer;
    quint8 low, high, pixel, firstSet, secondSet;
    QByteArray decompressed;

    if (tiles[tileID].setSpecific)
    {
        QDataStream tmp(src);

        // get the two sub tilesets offsets
        // rombank 0xC offset 0x4EEB is a table of two offsets for every tileset
        tmp.setByteOrder(QDataStream::LittleEndian);
        src->seek(SUBTILESET_TABLE + (tileSetID*2));

        tmp >> firstSet;
        tmp >> secondSet;

        // this is finally the last pointer before the actual tile data...
        // here are the "same" tiles from different tilesets grouped together
        if ((tileID < 0xCD) || (tileID == 0xFD))
            src->seek(offset + firstSet);
        else
            src->seek(offset + secondSet);

        tmp >> pointer;

        src->seek(offset + pointer);
    }
    else
        src->seek(offset);

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
        tilesCount = 0;

        // the pointer points to some meta data for every "tile"
        // like tiles count and size of "super tiles"

        if (isSprite[i] && (i != 0x54))
        {
            // at last try pointer+3 for sprite tiles like DK and Mario
            // the game code checks these tiles earlier but my code
            // corrupts some tiles like 0x41 the plant in level 14
            src->seek(pointer+3);
            in >> tilesCount;
            tiles[i].type = 3;
        }

        if (tilesCount == 0)
        {
            // first try pointer+4 for count
            src->seek(pointer+4);
            in >> tilesCount;
            tiles[i].type = 4;
        }

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

        // get data needed for super tiles
        //tiles[i].count = tilesCount;
        src->seek(pointer+8);
        in >> tiles[i].h;
        in >> tiles[i].w;
        tiles[i].count = tiles[i].h * tiles[i].w; // just get enough tiles to display the full super tile - no animations
        tiles[i].fullCount = tilesCount;

        if (tiles[i].count == 0)
            continue;

        if (tiles[i].count > 1)
        {
            tiles[i].additionalTilesAt = addOffset;
            addOffset += (tiles[i].count - 1);
        }

        if (i == 0x8A)
        {
            tiles[i].h = 4;
        }

        if (i == 0x8E)
        {
            tiles[i].h = 2;
            tiles[i].w = 2;
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

            // quick hack to get the right offset for the mario sprite
            if (i == 0x7F)
            {
                tiles[i].romOffset = 0x106F0;
                tiles[i].fullCount = 16;
            }

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



bool QDKEdit::createSprites(QFile *src, QGBPalette palette)
{
    QDataStream *in;
    quint16 tileX, tileY, pointer;
    quint8 low, high, pixel, firstSet, secondSet;
    QByteArray decompressed;

    QDir dir;
    if (!dir.exists("sprites"))
        dir.mkdir("sprites");

    QImage *sprite;
    int setsToGo = 1;

    for (int id = 0; id < 256; id++)
    {
        if (isSprite[id])
        {
            if (tiles[id].setSpecific)
                setsToGo = MAX_TILESETS;
            else
                setsToGo = 1;

            for (int set = 0; set < setsToGo; set++)
            {
                if (setsToGo == 1)
                {
                    if (QFile::exists(QString("sprites/sprite_%1.png").arg(id, 2, 16, QChar('0'))))
                    {
                        spriteImg.insert(QString("sprite_%1.png").arg(id, 2, 16, QChar('0')), new QImage(QString("sprites/sprite_%1.png").arg(id, 2, 16, QChar('0'))));
                        continue;
                    }
                }
                else
                {
                    if (QFile::exists(QString("sprites/sprite_%1_set_%2.png").arg(id, 2, 16, QChar('0')).arg(set, 2, 16, QChar('0'))))
                    {
                        spriteImg.insert(QString("sprite_%1_set_%2.png").arg(id, 2, 16, QChar('0')).arg(set, 2, 16, QChar('0')), new QImage(QString("sprites/sprite_%1_set_%2.png").arg(id, 2, 16, QChar('0')).arg(set, 2, 16, QChar('0'))));
                        continue;
                    }
                }

                sprite = new QImage(8*tiles[id].w, 8*tiles[id].h, QImage::Format_Indexed8);
                sprite->setColor(0, palette[0].rgb());
                sprite->setColor(1, palette[1].rgb());
                sprite->setColor(2, palette[2].rgb());
                sprite->setColor(3, palette[3].rgb());

                if (tiles[id].setSpecific)
                {
                    QDataStream tmp(src);

                    // get the two sub tilesets offsets
                    // rombank 0xC offset 0x4EEB is a table of two offsets for every tileset
                    tmp.setByteOrder(QDataStream::LittleEndian);
                    src->seek(SUBTILESET_TABLE + (set*2));

                    tmp >> firstSet;
                    tmp >> secondSet;

                    // this is finally the last pointer before the actual tile data...
                    // here are the "same" tiles from different tilesets grouped together
                    if ((id < 0xCD) || (id == 0xFD))
                        src->seek(tiles[id].romOffset + firstSet);
                    else
                        src->seek(tiles[id].romOffset + secondSet);

                    tmp >> pointer;

                    src->seek(tiles[id].romOffset + pointer);
                }
                else
                    src->seek(tiles[id].romOffset);

                if (tiles[id].compressed)
                {
                    QDataStream decomp(src);
                    quint16 decompSize;
                    if (id == 0xC2)
                        decompSize = 0x10*(tiles[id].count+2);
                    else if (id == 0x8E)
                        decompSize = 0x40;
                    else
                        decompSize = 0x10*tiles[id].count;
                    decompressed = LZSSDecompress(&decomp, decompSize);
                    in = new QDataStream(decompressed);
                }
                else
                    in = new QDataStream(src);

                in->setByteOrder(QDataStream::LittleEndian);

                sprite->fill(3);

                for (int i = 0; i < tiles[id].w; i++)
                    for (int j = 0; j < tiles[id].h; j++)
                    {
                        tileY = j * 8;
                        tileX = i * 8;

                        //skip empty tiles for specific sprites
                        if (((id == 0xC2) && (i == 0) && (j == 0)) ||
                            ((id == 0xC2) && (i == 0) && (j == 1)))
                            for (int z = 0; z < 16; z++)
                                (*in) >> low;

                        for (int ii = 0; ii < 8; ii++)
                        {
                            (*in) >> low;
                            (*in) >> high;

                            for (int jj = 0; jj < 8; jj++)
                            {
                                pixel = low & 0x80;
                                pixel >>= 1;
                                pixel |= high & 0x80;
                                pixel >>= 6;

                                low <<= 1;
                                high <<= 1;

                                sprite->setPixel(tileX+jj, tileY+ii, pixel);
                            }
                        }

                    }

                sortSprite(sprite, id);

                if (tiles[id].setSpecific)
                    sprite->save(QString("sprites/sprite_%1_set_%2.png").arg(id, 2, 16, QChar('0')).arg(set, 2, 16, QChar('0')));
                else
                    sprite->save(QString("sprites/sprite_%1.png").arg(id, 2, 16, QChar('0')));

                /*QPixmap *tmp;
                tmp = new QPixmap();
                tmp->convertFromImage(*sprite);
                delete sprite;*/

                if (tiles[id].setSpecific)
                    spriteImg.insert(QString("sprite_%1_set_%2.png").arg(id, 2, 16, QChar('0')).arg(set, 2, 16, QChar('0')), sprite);
                else
                    spriteImg.insert(QString("sprite_%1.png").arg(id, 2, 16, QChar('0')), sprite);

                delete in;
            }
        }
    }

    return true;
}

void QDKEdit::sortSprite(QImage *sprite, int id)
{
    /* normal bin
      A1 B1      A1 1A
      A2 B2  =>  A2 2A
    */
    if (id == 0xAE)
    {
        copyTile(sprite, 0, 0, 1, 0, true);
        copyTile(sprite, 0, 1, 1, 1, true);
    }

    /* moving bin
      A1 B2    00 00
      A2 C1    C1 1C
      B1 C2 => C2 2C
    */
    if (id == 0xAC)
    {
        /*copyTile(sprite, 0, 1, 1, 2, true);
        copyTile(sprite, 0, 1, 0, 2, false);
        copyTile(sprite, 0, 0, 1, 1, true);
        copyTile(sprite, 0, 0, 0, 1, false);*/
        copyTile(sprite, 1, 1, 0, 1, false);
        copyTile(sprite, 1, 2, 0, 2, false);
        copyTile(sprite, 0, 1, 1, 1, true);
        copyTile(sprite, 0, 2, 1, 2, true);
        fillTile(sprite, 0, 0, 0);
        fillTile(sprite, 1, 0, 0);
    }

    /* Pauline
      1 3    1 4
      4 5    2 5
      2 6 => 3 6
    */
    if (id == 0xC2)
    {
        swapTiles(sprite, 0, 1, 1, 0);
        swapTiles(sprite, 0, 1, 0, 2);
    }

    /* hiding monkey
      A1 B1    A1 1A
      A2 B2 => A2 2A
    */
    if (id == 0xC8)
    {
        copyTile(sprite, 0, 0, 1, 0, true);
        copyTile(sprite, 0, 1, 1, 1, true);
    }

    /* DK
      Z1 Z5 Z9    Z1 Z3 Z1
      Z2 Z6 ZA    Z2 Z4 2Z
      Z3 Z7 ZB    Z5 Z7 5Z
      Z4 Z8 ZC => Z6 Z8 6Z
    */
    if ((id == 0x6E) || (id == 0x44) || (id == 0x90) || (id == 0x5E) ||
        (id == 0x9A) || (id == 0xCC) || (id == 0x3A) || (id == 0xA6) ||
        (id == 0xAA) || (id == 0xBE) || (id == 0x92) || (id == 0xA4) || (id == 0x8A))
    {
        copyTile(sprite, 0, 1, 2, 1, true); //Z2 => mZA
        copyTile(sprite, 1, 0, 2, 2, true); //Z5 => mZB
        copyTile(sprite, 1, 1, 2, 3, true); //Z6 => mZC
        copyTile(sprite, 0, 0, 2, 0, false); //Z1 => Z9
        copyTile(sprite, 0, 2, 1, 0, false); //Z3 => Z5
        copyTile(sprite, 2, 2, 0, 2, true); //5Z => Z3
        copyTile(sprite, 0, 3, 1, 1, false); //Z4 => Z6
        copyTile(sprite, 2, 3, 0, 3, true); //6Z => Z4
    }

    /* moving board
      A1 A2 => A1 1A
    */
    if (id == 0x54)
    {
        copyTile(sprite, 0, 0, 1, 0, true);
    }

    /* switches
      A1 A2 => A1 B1
      B1 B2 => A2 B2
    */
    if ((id == 0x20) || (id == 0x1A) || (id == 0x2E))
    {
        swapTiles(sprite, 0, 1, 1, 0);
    }

    /* Giant DK Fight ???

    */
    if (id == 0x7A)
    {

    }
}

void QDKEdit::copyTile(QImage *img, int x1, int y1, int x2, int y2, bool mirror)
{
    int pixel;
    for (int j = 0; j < 8; j++)
        for (int i = 0; i < 8; i++)
        {
            pixel = img->pixelIndex(x1*8 + i, y1*8 + j);
            if (!mirror)
                img->setPixel(x2*8 + i, y2*8 + j, pixel);
            else
                img->setPixel((x2+1)*8 - 1 - i, y2*8 + j, pixel);
        }
}

void QDKEdit::swapTiles(QImage *img, int x1, int y1, int x2, int y2)
{
    int pixel1, pixel2;
    for (int j = 0; j < 8; j++)
        for (int i = 0; i < 8; i++)
        {
            pixel1 = img->pixelIndex(x1*8 + i, y1*8 + j);
            pixel2 = img->pixelIndex(x2*8 + i, y2*8 + j);
            img->setPixel(x1*8 + i, y1*8 + j, pixel2);
            img->setPixel(x2*8 + i, y2*8 + j, pixel1);
        }

}

void QDKEdit::fillTile(QImage *img, int x, int y, int index)
{
    for (int j = 0; j < 8; j++)
        for (int i = 0; i < 8; i++)
            img->setPixel(x*8 + i, y*8 + j, index);
}

bool QDKEdit::createTileSets(QFile *src, QGBPalette palette)
{
    QDataStream in(src);
    in.setByteOrder(QDataStream::LittleEndian);

    quint8 tmp;

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
        tmp = id & 0x0F;
        if ((id == 0x20) || (tmp == 0x02) || (tmp == 0x0C) || (tmp == 0x0E))
            tilesetBGP[id] = 0x6C;
        else
            tilesetBGP[id] = 0x9C;

        if (QFile::exists(QString("tiles/tileset_%1.png").arg(id, 2, 16, QChar('0'))))
        {
            tilesets[id].load(QString("tiles/tileset_%1.png").arg(id, 2, 16, QChar('0')));
            continue;
        }

        fullSet.fill(0);

        for (int i = 0; i < 255; i++) // since 0xFF is the empty tile we just skip it altogether
            copyTileToSet(src, tiles[i].romOffset, &fullSet, i, id, tiles[i].compressed, tiles[i].count, tiles[i].additionalTilesAt);

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
                if (start == 0)
                    qDebug() << decompressed.size();
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

void QDKEdit::updateTileset()
{
    // adjust color table
    quint8 mask;
    quint8 bgp = tilesetBGP[currentTileset];
    for (int i = 0; i < 4; i++)
    {
        mask = bgp & 0x03;
        bgp >>= 2;
        tilesets[currentTileset].setColor(i, sgbPal[currentPalIndex][mask].rgb());
    }

    tileSet.convertFromImage(tilesets[currentTileset]);

    if (selector)
        selector->changeTilePixmap(tileSet);

    qDeleteAll(spritePix);
    QPixmap *tmp;
    QImage *img;
    QMap<QString, QImage *>::const_iterator i = spriteImg.constBegin();
    while (i != spriteImg.constEnd())
    {
        img = i.value();
        // some sprites should use 0x9C as OBP
        // hammer for example
        // but we ignore that (at least for now)
        bgp = 0x1E;
        for (int j = 0; j < 4; j++)
        {
            mask = bgp & 0x03;
            bgp >>= 2;
            img->setColor(j, sgbPal[currentPalIndex][mask].rgb());
        }

        tmp = new QPixmap();
        tmp->convertFromImage(*img);
        if (transparentSprites)
            tmp->setMask(tmp->createMaskFromColor(img->color(0)));
        spritePix.insert(i.key(), tmp);
        ++i;
    }

    for (int i = 0; i < sprites.size(); i++)
        if (tiles[sprites[i].id].setSpecific)
            sprites[i].sprite = spritePix[QString("sprite_%1_set_%2.png").arg(sprites.at(i).id, 2, 16, QChar('0')).arg(currentTileset, 2, 16, QChar('0'))];
        else
            sprites[i].sprite = spritePix[QString("sprite_%1.png").arg(sprites.at(i).id, 2, 16, QChar('0'))];
}

void QDKEdit::changeMusic(int music)
{
    if ((music >= 0) && (music <= 0x23) && (music != currentMusic))
    {
        currentMusic = music;
        dataIsChanged = true;

        emit musicChanged(music);
    }
}

void QDKEdit::changePalette(int palette)
{
    if ((palette >= 0x180) && (palette <= 511) && (palette != currentPalIndex))
    {
        currentPalIndex = palette;
        dataIsChanged = true;

        updateTileset();
        update();
        emit paletteChanged(palette);
    }
}

void QDKEdit::changeSize(int size)
{
    if ((size >= 0) && (size <= 4) && (size != currentSize))
    {
        currentSize = size;
        dataIsChanged = true;
        int oldLength = lvlDataLength;

        if (size == 0x00)
        {
            setLevelDimension(32, 18);
            lvlDataLength = 32*18*2;
        }
        else
        {
            setLevelDimension(32, 28);
            lvlDataLength = 32*28*2;
        }
        lvlData.resize(lvlDataLength);

        //init new space to emptyTile
        for (int i = oldLength; i < lvlData.size(); i+=2)
        {
            lvlData[i] = (quint8)emptyTile;
            lvlData[i+1] = 0x00;
        }

        update();
        emit sizeChanged(size);
    }
}

void QDKEdit::changeTileset(int tileset)
{
    if ((tileset >= 0) && (tileset < MAX_TILESETS) && (tileset != currentTileset))
    {
        currentTileset = tileset;
        dataIsChanged = true;

        updateTileset();
        update();
        emit tilesetChanged(tileset);
    }
}

void QDKEdit::changeTime(int time)
{
    if ((time >= 0) && (time <= 999) && (time != currentTime))
    {
        currentTime = time;
        dataIsChanged = true;

        emit changeTime(time);
    }
}

void QDKEdit::changeSpriteTransparency(bool transparent)
{
    if (transparentSprites != transparent)
    {
        transparentSprites = transparent;
        updateTileset();
        update();
    }
}

void QDKEdit::addSprite(int id)
{
    if (!isSprite[id])
        return;

    dataIsChanged = true;
    QDKSprite sprite;
    sprite.id = id;
    sprite.levelPos = 0;
    sprite.ramPos = 0xDA75;
    sprite.pixelPerfect = false;
    sprite.x = 0;
    sprite.y = 0;
    sprite.rotate = BOTTOM;
    sprite.flagByte = getSpriteDefaultFlag(id);
    sprite.size = QSize(tiles[id].w, tiles[id].h);

    if (id == 0x54)
        sprite.drawOffset.setX(-0.5f);

    if (tiles[id].setSpecific)
       sprite.sprite = spritePix[QString("sprite_%1_set_%2.png").arg(id, 2, 16, QChar('0')).arg(currentTileset, 2, 16, QChar('0'))];
   else
       sprite.sprite = spritePix[QString("sprite_%1.png").arg(id, 2, 16, QChar('0'))];

    sprites.append(sprite);
    emit spriteAdded(spriteNumToString(id), id);
    update();
}

quint8 QDKEdit::getSpriteDefaultFlag(int id)
{
    switch (id)
    {
        case 0x80: case 0x98: return 0x03; // walking friend+enemy
        case 0x70: case 0x72: return 0x05; // elevator up/down
        case 0x54: case 0x84: return 0x00;
        case 0x7F:            return 0x00; // Mario
        case 0xB8:            return 0xFF; // key/hole 0/1 ? WTF?!
        //different DKs
        /*set in game: 6E 00
                       9A 01 00
                       CC 01
                       B8 00 01 */
        case 0x6E: return 0xFF; // probably 0x00                               // Lvl0-1, 9-5 barrels
        case 0x9A: return 0xFF; // even more guessed 0x00                      // Lvl1-4, 5-4, 7-8, 9-1 avalanche
        case 0xCC: return 0xFF; // this one at least is never set explicitly   // Lvl1-8, 2-12, 3-8, 6-8, 7-12 pick-up barrels
        default: return 0x00;
    }
}

void QDKEdit::updateSprite(int num)
{
    if (num >= sprites.size())
        return;

    dataIsChanged = true;

    //check for rotation
    if (sprites.at(num).id == 0x7F)
        sprites[num].rotate = sprites.at(num).flagByte;
    else if ((sprites.at(num).id == 0x80) || (sprites.at(num).id == 0x98))
        sprites[num].rotate = (sprites.at(num).flagByte + 1) & 1;

    update();
}

void QDKEdit::fillTileNames()
{
    tileNames.insert(0x00, "I-tile");
    tileNames.insert(0x01, "ground");
    tileNames.insert(0x02, "ground");
    tileNames.insert(0x03, "ice");
    tileNames.insert(0x04, "dissolving ground");
    tileNames.insert(0x05, "spikes");
    tileNames.insert(0x06, "ground");
    tileNames.insert(0x07, "ladder");
    tileNames.insert(0x08, "climbing pole");
    tileNames.insert(0x09, "conveyor left");
    tileNames.insert(0x0a, "conveyor right");
    tileNames.insert(0x0b, "ground");
    tileNames.insert(0x0c, "board track");
    tileNames.insert(0x0d, "board track");
    tileNames.insert(0x0e, "board track");
    tileNames.insert(0x0f, "board track");
    tileNames.insert(0x10, "board track");
    tileNames.insert(0x11, "board track");
    tileNames.insert(0x12, "board track");
    tileNames.insert(0x13, "board track");
    tileNames.insert(0x14, "board track");
    tileNames.insert(0x15, "board track");
    tileNames.insert(0x16, "board track");
    tileNames.insert(0x17, "board track");
    tileNames.insert(0x18, "spring board");
    tileNames.insert(0x19, "0x19 ???");
    tileNames.insert(0x1a, "on-off switch");
    tileNames.insert(0x1b, "board track bumper");
    tileNames.insert(0x1c, "ground");
    tileNames.insert(0x1d, "high wire");
    tileNames.insert(0x1e, "high wire angled up");
    tileNames.insert(0x1f, "high wire angled down");
    tileNames.insert(0x20, "DK switch");
    tileNames.insert(0x21, "conveyor left end");
    tileNames.insert(0x22, "conveyor right end");
    tileNames.insert(0x23, "structure support (0-4)");
    tileNames.insert(0x24, "elevator bottom - unused");
    tileNames.insert(0x25, "mushroom - do not use");
    tileNames.insert(0x26, "spikes");
    tileNames.insert(0x27, "ground");
    tileNames.insert(0x28, "ground off-center");
    tileNames.insert(0x29, "shutter");
    tileNames.insert(0x2a, "retractable ground");
    tileNames.insert(0x2b, "hanging spike");
    tileNames.insert(0x2c, "moving ladder");
    tileNames.insert(0x2d, "dissolving ground (enemy)");
    tileNames.insert(0x2e, "3-pos switch");
    tileNames.insert(0x2f, "pole tip");
    tileNames.insert(0x30, "pole");
    tileNames.insert(0x31, "waves");
    tileNames.insert(0x32, "current right");
    tileNames.insert(0x33, "current left");
    tileNames.insert(0x34, "conveyor (fake)");
    tileNames.insert(0x35, "current up");
    tileNames.insert(0x36, "water");
    tileNames.insert(0x37, "water fall");
    tileNames.insert(0x38, "0x38 ???");
    tileNames.insert(0x39, "elevetor top - unused");
    tileNames.insert(0x3a, "sprite (DK)");
    tileNames.insert(0x3b, "ground off-center");
    tileNames.insert(0x3c, "0x3c ???");
    tileNames.insert(0x3d, "0x3d ???");
    tileNames.insert(0x3e, "0x3e ???");
    tileNames.insert(0x3f, "0x3f ???");
    tileNames.insert(0x40, "plant spitting left");
    tileNames.insert(0x41, "plant spitting right");
    tileNames.insert(0x42, "poisonous water");
    tileNames.insert(0x43, "falling ground block");
    tileNames.insert(0x44, "sprite (DK)");
    tileNames.insert(0x45, "2-UP heart");
    tileNames.insert(0x46, "0x46 ???");
    tileNames.insert(0x47, "sprite (crushing stone)");
    tileNames.insert(0x48, "sprite (octopus)");
    tileNames.insert(0x49, "broken ladder");
    tileNames.insert(0x4a, "pole tip");
    tileNames.insert(0x4b, "fake exit");
    tileNames.insert(0x4c, "ground");
    tileNames.insert(0x4d, "sprite (squid)");
    tileNames.insert(0x4e, "sprite (fish)");
    tileNames.insert(0x4f, "sprite (bat)");
    tileNames.insert(0x50, "sprite (walrus)");
    tileNames.insert(0x51, "winning ladder");
    tileNames.insert(0x52, "ground");
    tileNames.insert(0x53, "umbrella");
    tileNames.insert(0x54, "sprite (board)");
    tileNames.insert(0x55, "ground");
    tileNames.insert(0x56, "smashable block");
    tileNames.insert(0x57, "sprite (hammer)");
    tileNames.insert(0x58, "sprite (skull)");
    tileNames.insert(0x59, "ground off-center+ladder");
    tileNames.insert(0x5a, "sprite (crab)");
    tileNames.insert(0x5b, "0x5b ???");
    tileNames.insert(0x5c, "sprite (oil flames)");
    tileNames.insert(0x5d, "spring board");
    tileNames.insert(0x5e, "sprite (DK)");
    tileNames.insert(0x5f, "ground off-center+ladder");
    tileNames.insert(0x60, "X-tile");
    tileNames.insert(0x61, "liana right");
    tileNames.insert(0x62, "X-tile");
    tileNames.insert(0x63, "liana right bottom");
    tileNames.insert(0x64, "sprite (enemy)");
    tileNames.insert(0x65, "hat");
    tileNames.insert(0x66, "X-tile");
    tileNames.insert(0x67, "bird's nest");
    tileNames.insert(0x68, "cannon left");
    tileNames.insert(0x69, "cannon right");
    tileNames.insert(0x6a, "cannon left up");
    tileNames.insert(0x6b, "cannon up (left)");
    tileNames.insert(0x6c, "cannon right up");
    tileNames.insert(0x6d, "cannon up (right)");
    tileNames.insert(0x6e, "sprite (DK)");
    tileNames.insert(0x6f, "ground");
    tileNames.insert(0x70, "elevator up - bottom");
    tileNames.insert(0x71, "elevator up - top");
    tileNames.insert(0x72, "elevator down - bottom");
    tileNames.insert(0x73, "elevator down - top");
    tileNames.insert(0x74, "0x74 ???");
    tileNames.insert(0x75, "placable block");
    tileNames.insert(0x76, "placable ladder (broken do not use)");
    tileNames.insert(0x77, "placeable spring board");
    tileNames.insert(0x78, "placable question mark (broken do not use)");
    tileNames.insert(0x79, "exit");
    tileNames.insert(0x7a, "sprite (DK)");
    tileNames.insert(0x7b, "elevator track");
    tileNames.insert(0x7c, "sprite (flame)");
    tileNames.insert(0x7d, "winning tile");
    tileNames.insert(0x7e, "0x7e ???");
    tileNames.insert(0x7f, "sprite (Mario)");
    tileNames.insert(0x80, "sprite (friend)");
    tileNames.insert(0x81, "ground off-center");
    tileNames.insert(0x82, "0x82 ???");
    tileNames.insert(0x83, "ground off-center");
    tileNames.insert(0x84, "sprite (platform)");
    tileNames.insert(0x85, "ground off-center");
    tileNames.insert(0x86, "sprite (enemy)");
    tileNames.insert(0x87, "ground off-center");
    tileNames.insert(0x88, "sprite (knight)");
    tileNames.insert(0x89, "ground off-center");
    tileNames.insert(0x8a, "sprite (DK)");
    tileNames.insert(0x8b, "ground off-center+ladder");
    tileNames.insert(0x8c, "X-tile");
    tileNames.insert(0x8d, "ground off-center+ladder");
    tileNames.insert(0x8e, "sprite (wind)");
    tileNames.insert(0x8f, "ground off-center+ladder");
    tileNames.insert(0x90, "sprite (DK)");
    tileNames.insert(0x91, "ground off-center+ladder");
    tileNames.insert(0x92, "sprite (DK)");
    tileNames.insert(0x93, "ground off-center+ladder");
    tileNames.insert(0x94, "sprite (monkey)");
    tileNames.insert(0x95, "ground off-center+ladder");
    tileNames.insert(0x96, "sprite (icile)");
    tileNames.insert(0x97, "ground off-center+ladder");
    tileNames.insert(0x98, "sprite (enemy)");
    tileNames.insert(0x99, "ground off-center+ladder");
    tileNames.insert(0x9a, "sprite (DK)");
    tileNames.insert(0x9b, "ground off-center+ladder");
    tileNames.insert(0x9c, "0x9c ???");
    tileNames.insert(0x9d, "sprite (fruit)");
    tileNames.insert(0x9e, "key");
    tileNames.insert(0x9f, "ground off-center+ladder");
    tileNames.insert(0xa0, "0xa0 ???");
    tileNames.insert(0xa1, "ground off-center+ladder");
    tileNames.insert(0xa2, "sprite (frog)");
    tileNames.insert(0xa3, "ground off-center+ladder");
    tileNames.insert(0xa4, "sprite (DK)");
    tileNames.insert(0xa5, "liana left");
    tileNames.insert(0xa6, "sprite (DK)");
    tileNames.insert(0xa7, "liana left end");
    tileNames.insert(0xa8, "sprite (DK Jr)");
    tileNames.insert(0xa9, "oil drum");
    tileNames.insert(0xaa, "sprite (DK)");
    tileNames.insert(0xab, "1-UP heart");
    tileNames.insert(0xac, "sprite (enemy)");
    tileNames.insert(0xad, "water");
    tileNames.insert(0xae, "sprite (trash)");
    tileNames.insert(0xaf, "handbag");
    tileNames.insert(0xb0, "sprite (block)");
    tileNames.insert(0xb1, "3-UP heart");
    tileNames.insert(0xb2, "cannon #2 right");
    tileNames.insert(0xb3, "cannon #2 left");
    tileNames.insert(0xb4, "turret left");
    tileNames.insert(0xb5, "turret right");
    tileNames.insert(0xb6, "sprite (klaptrap)");
    tileNames.insert(0xb7, "ground off-center");
    tileNames.insert(0xb8, "sprite (key)");
    tileNames.insert(0xb9, "bird's nest");
    tileNames.insert(0xba, "sprite (oil can)");
    tileNames.insert(0xbb, "expanding ground");
    tileNames.insert(0xbc, "expanding ladder");
    tileNames.insert(0xbd, "placable block (not working)");
    tileNames.insert(0xbe, "sprite (DK)");
    tileNames.insert(0xbf, "mushroom - do not use");
    tileNames.insert(0xc0, "sprite (Panser)");
    tileNames.insert(0xc1, "ground off-center+ladder");
    tileNames.insert(0xc2, "sprite (Pauline)");
    tileNames.insert(0xc3, "ground off-center+ladder");
    tileNames.insert(0xc4, "super hammer");
    tileNames.insert(0xc5, "ground off-center+ladder");
    tileNames.insert(0xc6, "sprite (DK Jr)");
    tileNames.insert(0xc7, "ground off-center+ladder");
    tileNames.insert(0xc8, "sprite (monkey)");
    tileNames.insert(0xc9, "ground off-center+ladder");
    tileNames.insert(0xca, "sprite (DK Jr)");
    tileNames.insert(0xcb, "ground off-center+ladder");
    tileNames.insert(0xcc, "sprite (DK)");
    tileNames.insert(0xcd, "background");
    tileNames.insert(0xce, "background");
    tileNames.insert(0xcf, "background");
    tileNames.insert(0xd0, "background");
    tileNames.insert(0xd1, "background");
    tileNames.insert(0xd2, "background");
    tileNames.insert(0xd3, "background");
    tileNames.insert(0xd4, "background");
    tileNames.insert(0xd5, "background");
    tileNames.insert(0xd6, "background");
    tileNames.insert(0xd7, "background");
    tileNames.insert(0xd8, "background");
    tileNames.insert(0xd9, "background");
    tileNames.insert(0xda, "background");
    tileNames.insert(0xdb, "background");
    tileNames.insert(0xdc, "background");
    tileNames.insert(0xdd, "background");
    tileNames.insert(0xde, "background");
    tileNames.insert(0xdf, "background");
    tileNames.insert(0xe0, "background");
    tileNames.insert(0xe1, "background");
    tileNames.insert(0xe2, "background");
    tileNames.insert(0xe3, "background");
    tileNames.insert(0xe4, "background");
    tileNames.insert(0xe5, "background");
    tileNames.insert(0xe6, "background");
    tileNames.insert(0xe7, "background");
    tileNames.insert(0xe8, "background");
    tileNames.insert(0xe9, "background");
    tileNames.insert(0xea, "background");
    tileNames.insert(0xeb, "background");
    tileNames.insert(0xec, "background");
    tileNames.insert(0xed, "background");
    tileNames.insert(0xee, "background");
    tileNames.insert(0xef, "background");
    tileNames.insert(0xf0, "background");
    tileNames.insert(0xf1, "background");
    tileNames.insert(0xf2, "background");
    tileNames.insert(0xf3, "background");
    tileNames.insert(0xf4, "background");
    tileNames.insert(0xf5, "background");
    tileNames.insert(0xf6, "background");
    tileNames.insert(0xf7, "background");
    tileNames.insert(0xf8, "background");
    tileNames.insert(0xf9, "background");
    tileNames.insert(0xfa, "background");
    tileNames.insert(0xfb, "background");
    tileNames.insert(0xfc, "background");
    tileNames.insert(0xfd, "ground");
    tileNames.insert(0xfe, "X-tile");
    tileNames.insert(0xff, "empty tile");
}

void QDKEdit::fillSpriteNames()
{
    spriteNames.insert(0x1a, "On/off switch");
    spriteNames.insert(0x20, "Enemy switch");
    spriteNames.insert(0x2e, "3-pos switch");
    spriteNames.insert(0x3a, "Donkey Kong (Klaptraps)");
    spriteNames.insert(0x44, "Donkey Kong (Lvl 0-2)");
    spriteNames.insert(0x47, "Crushing stone");
    spriteNames.insert(0x48, "Octopus");
    spriteNames.insert(0x4d, "Squid");
    spriteNames.insert(0x4e, "Fish");
    spriteNames.insert(0x4f, "Bat");
    spriteNames.insert(0x50, "Walrus");
    spriteNames.insert(0x54, "Board");
    spriteNames.insert(0x57, "Hammer");
    spriteNames.insert(0x58, "Skull");
    spriteNames.insert(0x5a, "Hermit crab");
    spriteNames.insert(0x5c, "Oil drum flames");
    spriteNames.insert(0x5e, "Donkey Kong (fireballs)");
    spriteNames.insert(0x64, "Enemy");
    spriteNames.insert(0x6e, "Donkey Kong (barrels)");
    spriteNames.insert(0x70, "Elevator up");
    spriteNames.insert(0x72, "Elevator down");
    spriteNames.insert(0x7a, "Giant DK");
    spriteNames.insert(0x7c, "Flame");
    spriteNames.insert(0x7f, "Mario");
    spriteNames.insert(0x80, "Wall-walking friend");
    spriteNames.insert(0x84, "Floating platform");
    spriteNames.insert(0x86, "Walking enemy");
    spriteNames.insert(0x88, "Knight");
    spriteNames.insert(0x8a, "Donkey Kong (Egyptian/rock guys)");
    spriteNames.insert(0x8e, "Wind");
    spriteNames.insert(0x90, "Donkey Kong (spring)");
    spriteNames.insert(0x92, "Donkey Kong (barrels left/right)");
    spriteNames.insert(0x94, "Climbable monkey");
    spriteNames.insert(0x96, "Falling icicle");
    spriteNames.insert(0x98, "Wall-walking enemy");
    spriteNames.insert(0x9a, "Donkey Kong (avalanche)");
    spriteNames.insert(0x9d, "Fruit");
    spriteNames.insert(0xa2, "Frog");
    spriteNames.insert(0xa4, "Donkey Kong (boulders)");
    spriteNames.insert(0xa6, "Donkey Kong (switch)");
    spriteNames.insert(0xa8, "DK Junior (mushrooms/screen edge)");
    spriteNames.insert(0xaa, "Donkey Kong (mushrooms)");
    spriteNames.insert(0xac, "Trash can enemy");
    spriteNames.insert(0xae, "Trash can");
    spriteNames.insert(0xb0, "Walking block");
    spriteNames.insert(0xb6, "Klaptrap");
    spriteNames.insert(0xb8, "Key");
    spriteNames.insert(0xba, "Oil can");
    spriteNames.insert(0xbe, "Donkey Kong (pick-up barrels)");
    spriteNames.insert(0xc0, "Panser");
    spriteNames.insert(0xc2, "Pauline");
    spriteNames.insert(0xc6, "DK Junior (switch)");
    spriteNames.insert(0xc8, "Sleeping monkey");
    spriteNames.insert(0xca, "DK Junior (mushrooms)");
    spriteNames.insert(0xcc, "Donkey Kong (pick-up barrels)");
}

void QDKEdit::saveLevel()
{
    if ((currentLevel != -1) && (dataIsChanged))
    {
        levels[currentLevel].fullDataUpToDate = false;
        levels[currentLevel].displayTilemap.clear();
        levels[currentLevel].displayTilemap.append(lvlData);
        updateRawTilemap(currentLevel);
        expandRawTilemap(currentLevel);
        levels[currentLevel].sprites.clear();
        for (int i = 0; i < sprites.size(); i++)
        {
            levels[currentLevel].sprites.append((QDKSprite&)sprites[i]);
            levels[currentLevel].sprites[i].levelPos = levels[currentLevel].sprites[i].y*levelDimension.width() + levels[currentLevel].sprites[i].x;
            levels[currentLevel].sprites[i].ramPos = levels[currentLevel].sprites[i].levelPos + 0xDA75;
        }

        levels[currentLevel].switches.clear();
        for (int i = 0; i < currentSwitches.size(); i++)
        {
            levels[currentLevel].switches.append(currentSwitches[i]);
            levels[currentLevel].switches[i].levelPos = levels[currentLevel].switches[i].y * levelDimension.width() + levels[currentLevel].switches[i].x;
            levels[currentLevel].switches[i].ramPos = levels[currentLevel].switches[i].levelPos + 0xD44D;
            for (int j = 0; j < levels[currentLevel].switches[i].connectedTo.size(); j++)
            {
                levels[currentLevel].switches[i].connectedTo[j].levelPos = levels[currentLevel].switches[i].connectedTo[j].y * levelDimension.width() + levels[currentLevel].switches[i].connectedTo[j].x;
                if (levels[currentLevel].switches[i].connectedTo[j].isSprite)
                    levels[currentLevel].switches[i].connectedTo[j].ramPos = levels[currentLevel].switches[i].connectedTo[j].levelPos + 0xDA75;
                else
                    levels[currentLevel].switches[i].connectedTo[j].ramPos = levels[currentLevel].switches[i].connectedTo[j].levelPos + 0xD44D;
            }
        }

        levels[currentLevel].size = currentSize;
        levels[currentLevel].time = currentTime;
        levels[currentLevel].tileset = currentTileset;
        levels[currentLevel].music = currentMusic;
        levels[currentLevel].paletteIndex = currentPalIndex;
    }

    dataIsChanged = false;
}

void QDKEdit::changeLevel(int id)
{
    if (!romLoaded)
        return;

    dataIsChanged = false;
    currentLevel = id;
    swObjToMove = -1;
    spriteToMove = -1;

    lvlData.clear();
    lvlData.append(levels[currentLevel].displayTilemap);

    currentSize = levels[currentLevel].size;
    currentTime = levels[currentLevel].time;
    currentTileset = levels[currentLevel].tileset;
    currentMusic = levels[currentLevel].music;
    currentPalIndex = levels[currentLevel].paletteIndex;


    lvlDataStart = 0;
    lvlDataLength = lvlData.size();
    spriteSelection = QRect();
    selectedSprite = -1;
    emit spriteSelected(-1);

    if (levels[currentLevel].size == 0x00)
        setLevelDimension(32, 18);
    else
        setLevelDimension(32, 28);

    updateTileset();

    for (int i = sprites.size()-1; i >= 0; i--)
        emit spriteRemoved(i);

    for (int i = currentSwitches.size()-1; i >= 0; i--)
        emit switchRemoved(i);

    switchToEdit = -1;

    sprites.clear();
    currentSwitches.clear();
    for (int i = 0; i < levels[currentLevel].sprites.size(); i++)
    {
        sprites.append(levels[currentLevel].sprites.at(i));
        if (tiles[sprites[i].id].setSpecific)
           sprites[i].sprite = spritePix[QString("sprite_%1_set_%2.png").arg(sprites.at(i).id, 2, 16, QChar('0')).arg(currentTileset, 2, 16, QChar('0'))];
        else
           sprites[i].sprite = spritePix[QString("sprite_%1.png").arg(sprites.at(i).id, 2, 16, QChar('0'))];
        emit spriteAdded(spriteNumToString(sprites[i].id), sprites[i].id);
    }

    for (int i = 0; i < levels[currentLevel].switches.size(); i++)
    {
        currentSwitches.append(levels[currentLevel].switches.at(i));
        emit switchAdded(&levels[currentLevel].switches[i]);
    }

    update();

    emit dataChanged();
    emit paletteChanged(levels[currentLevel].paletteIndex);
    emit tilesetChanged(levels[currentLevel].tileset);
    emit timeChanged(levels[currentLevel].time);
    emit musicChanged(levels[currentLevel].music);
    emit sizeChanged(levels[currentLevel].size);
}

QString QDKEdit::getLevelInfo()
{
    QString str = "";
    int id = currentLevel;

    str += QString("Size %1; Tileset %2; Music %3; Time %4; full size %5; Palette: 0x%6").arg(levels[id].size, 2, 16, QChar('0')).arg(levels[id].tileset, 2, 16, QChar('0')).arg(levels[id].music, 2, 16, QChar('0')).arg(levels[id].time).arg(levels[id].fullData.size()).arg(levels[id].paletteIndex, 4, 16, QChar('0'));

    if (!levels[id].switchData)
        str += QString("; No switch data");

    if (!levels[id].addSpriteData)
        str += QString("; No add. Sprite data");

    str += QString("\n");

    QString tmp;

    QDKSwitch *sw;
    QDKSwitchObject *obj;
    for (int i = 0; i < levels[id].switches.size(); i++)
    {
        sw = &levels[id].switches[i];
        str += QString("Switch %1 (state %2); pos %4x%5; connected to\n--- ").arg(i).arg(sw->state).arg(sw->x).arg(sw->y);
        tmp = "";
        for (int j = 0; j < sw->connectedTo.size(); j++)
        {
            obj = &sw->connectedTo[j];
            if (!obj->isSprite)
                tmp += QString("tile at %1x%2; ").arg(obj->x). arg(obj->y);
            else
                tmp += QString("sprite at %1x%2; ").arg(obj->x). arg(obj->y);
        }
        str += tmp + "\n";
    }
/*
    if (levels[id].switchData) // 0x11 + 0x90 bytes
    {
        tmp = "RAW switch data:";
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

    rebuildSwitchData(id);

    if (levels[id].switchData) // 0x11 + 0x90 bytes
    {
        tmp = "RAW switch data:";
        for (int i = 0; i < levels[id].rawSwitchData.size(); i++)
        {
            if (i % 16 == 0)
                tmp += QString("\n%1: ").arg(i, 2, 16, QChar('0'));
            tmp += QString("%1").arg((quint8)levels[id].rawSwitchData[i], 2, 16, QChar('0'));

            if (i % 2 == 1)
                tmp += " ";
        }
        str += tmp + "\n";
    }*/

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

    //rebuildAddSpriteData(id);

    /*if (levels[id].addSpriteData) // 0x40 bytes
    {
        tmp = "Add. sprite data (after):";
        for (int i = 0; i < levels[id].rawAddSpriteData.size(); i++)
        {
            if (i % 16 == 0)
                tmp += QString("\n%1: ").arg(i, 2, 16, QChar('0'));
            tmp += QString("%1").arg((quint8)levels[id].rawAddSpriteData[i], 2, 16, QChar('0'));

            if (i % 2 == 1)
                tmp += " ";
        }
        str += tmp + "\n";
    }*/

    return str;
}

void QDKEdit::selectSwitch(int num)
{
    switchToEdit = num;
    update();
}

void QDKEdit::deleteCurrentSwitch()
{
    if (switchToEdit < 0)
        return;

    currentSwitches[switchToEdit].connectedTo.clear();
    currentSwitches.removeAt(switchToEdit);
    dataIsChanged = true;
    emit switchRemoved(switchToEdit);
    swObjToMove = -1;
    switchToEdit = -1;
    update();
}

void QDKEdit::deleteSwitchObj(int num)
{
    if ((switchToEdit < 0) || (switchToEdit >= currentSwitches.size()) || (num >= currentSwitches.at(switchToEdit).connectedTo.size()))
        return;

    currentSwitches[switchToEdit].connectedTo.removeAt(num);
    dataIsChanged = true;
    swObjToMove = -1;
    update();
    emit switchUpdated(switchToEdit, &currentSwitches[switchToEdit]);
}

void QDKEdit::toggleSwitchMode(bool enabled)
{
    switchMode = enabled;
    if (enabled)
        spriteMode = true;
}

void QDKEdit::toggleSwitchMode(int enabled)
{
    toggleSwitchMode(enabled > 0);
}


void QDKEdit::mouseMoveEvent(QMouseEvent *e)
{
    if (!switchMode)
    {
        QTileEdit::mouseMoveEvent(e);
        return;
    }

    if ((e->x()+1 > scaledSize.width()) || (e->x() < 0) || (e->y()+1 > scaledSize.height()) || (e->y() < 0))
        return;

    // check for specific tiles and sprites
    // tiles
    // 0x09 0x0A conveyer belt
    // 0x1A 0x20 0x2E switches
    // 0x29 shutter
    // 0x2A floor
    // 0x70 0x72 elevator up/down
    // 0xB9 bird's nest

    //sprites
    // 0x54 moving board

    QRect newSelection = QRect(0,0,0,0);
    QRect spriteRect;

    //check tiles
    int xTile = (float)e->x() / (float)tileSize.width() / scaleFactorX;
    int yTile = (float)e->y() / (float)tileSize.height() / scaleFactorY;
    int tileNum = getTile(xTile, yTile);
    int spriteNum;
    int tmp;

    if (!mousePressed)
    {
        if (switchToEdit > -1)
        {
            //check for current (misplaced) switch
            if ((currentSwitches.at(switchToEdit).x == xTile) && (currentSwitches.at(switchToEdit).y == yTile))
            {
                if ((tileNum == 0x1A) || (tileNum == 0x20) || (tileNum == 0x2E))
                    newSelection = QRect(xTile * tileSize.width(), yTile * tileSize.height(), tileSize.width()*2-1, tileSize.height()*2-1);
                else
                    newSelection = QRect(xTile * tileSize.width(), yTile * tileSize.height(), tileSize.width()-1, tileSize.height()-1);;
            }
            else // check for current (misplaced) switch objects
            {
                for (tmp = 0; tmp < currentSwitches.at(switchToEdit).connectedTo.size(); tmp++)
                    if ((currentSwitches.at(switchToEdit).connectedTo.at(tmp).x == xTile) && (currentSwitches.at(switchToEdit).connectedTo.at(tmp).y == yTile))
                        break;

                if (tmp != currentSwitches.at(switchToEdit).connectedTo.size())
                {
                    //tiles
                    if (!currentSwitches.at(switchToEdit).connectedTo.at(tmp).isSprite)
                    {
                        if ((tileNum == 0x09) || (tileNum == 0x0A) || (tileNum == 0x1A) || (tileNum == 0x20) || (tileNum == 0x2E)
                                || (tileNum == 0x29) || (tileNum == 0x2A) || (tileNum == 0x70) || (tileNum == 0x72) || (tileNum == 0xB9))
                            newSelection = QRect(xTile * tileSize.width(), yTile * tileSize.height(), tiles[tileNum].w * tileSize.width()-1, tiles[tileNum].h * tileSize.height()-1);
                        else
                            newSelection = QRect(xTile * tileSize.width(), yTile * tileSize.height(), tileSize.width()-1, tileSize.height()-1);
                    }
                    else // sprites
                    {
                        int j;
                        for (j = 0; j < sprites.size(); j++)
                            if ((sprites.at(j).id == 0x54) && (sprites.at(j).x == currentSwitches.at(switchToEdit).connectedTo.at(tmp).x) && (sprites.at(j).y == currentSwitches.at(switchToEdit).connectedTo.at(tmp).y))
                                break;

                        if (j != sprites.size())
                            newSelection = QRect(currentSwitches.at(switchToEdit).connectedTo.at(tmp).x*tileSize.width()-tileSize.width()/2, currentSwitches.at(switchToEdit).connectedTo.at(tmp).y*tileSize.height(), tileSize.width()*2-1, tileSize.height()-1);
                        else
                            newSelection = QRect(xTile * tileSize.width(), yTile * tileSize.height(), tileSize.width()-1, tileSize.height()-1);
                    }
                }
            }
        }
        else
        {
            for (tmp = 0; tmp < currentSwitches.size(); tmp++)
                if ((currentSwitches.at(tmp).x == xTile) && (currentSwitches.at(tmp).y == yTile))
                    break;

            if (tmp != currentSwitches.size())
                if ((tileNum == 0x1A) || (tileNum == 0x20) || (tileNum == 0x2E))
                    newSelection = QRect(xTile * tileSize.width(), yTile * tileSize.height(), tileSize.width()*2-1, tileSize.height()*2-1);
                else
                    newSelection = QRect(xTile * tileSize.width(), yTile * tileSize.height(), tileSize.width()-1, tileSize.height()-1);;
        }

        // change xTile, yTile for large tiles
        if (tileNum > 0xFF)
        {
            tmp = tileNum - 0x100;
            if ((tmp >= tiles[0x1A].additionalTilesAt) && (tmp < tiles[0x1A].additionalTilesAt+tiles[0x1A].count-1))
            {
                tmp -= tiles[0x1A].additionalTilesAt - 1;
                xTile -= tmp % tiles[0x1A].w;
                yTile -= tmp / tiles[0x1A].w;
                if (getTile(xTile, yTile) == 0x1A)
                    tileNum = 0x1A;
            }

            if ((tmp >= tiles[0x20].additionalTilesAt) && (tmp < tiles[0x20].additionalTilesAt+tiles[0x20].count-1))
            {
                tmp -= tiles[0x20].additionalTilesAt - 1;
                xTile -= tmp % tiles[0x20].w;
                yTile -= tmp / tiles[0x20].w;
                if (getTile(xTile, yTile) == 0x20)
                    tileNum = 0x20;
            }

            if ((tmp >= tiles[0x2E].additionalTilesAt) && (tmp < tiles[0x2E].additionalTilesAt+tiles[0x2E].count-1))
            {
                tmp -= tiles[0x2E].additionalTilesAt - 1;
                xTile -= tmp % tiles[0x2E].w;
                yTile -= tmp / tiles[0x2E].w;
                if (getTile(xTile, yTile) == 0x2E)
                    tileNum = 0x2E;
            }

            if ((tmp >= tiles[0x70].additionalTilesAt) && (tmp < tiles[0x70].additionalTilesAt+tiles[0x70].count-1))
            {
                tmp -= tiles[0x70].additionalTilesAt - 1;
                xTile -= tmp % tiles[0x70].w;
                yTile -= tmp / tiles[0x70].w;
                if (getTile(xTile, yTile) == 0x70)
                    tileNum = 0x70;
            }

            if ((tmp >= tiles[0x72].additionalTilesAt) && (tmp < tiles[0x72].additionalTilesAt+tiles[0x72].count-1))
            {
                tmp -= tiles[0x72].additionalTilesAt - 1;
                xTile -= tmp % tiles[0x72].w;
                yTile -= tmp / tiles[0x72].w;
                if (getTile(xTile, yTile) == 0x72)
                    tileNum = 0x72;
            }

            if ((tmp >= tiles[0xB9].additionalTilesAt) && (tmp < tiles[0xB9].additionalTilesAt+tiles[0xB9].count-1))
            {
                tmp -= tiles[0xB9].additionalTilesAt - 1;
                xTile -= tmp % tiles[0xB9].w;
                yTile -= tmp / tiles[0xB9].w;
                if (getTile(xTile, yTile) == 0xB9)
                    tileNum = 0xB9;
            }
        }

        // check for valid unused objects
        if (newSelection == QRect(0,0,0,0))
        {
            if ((tileNum == 0x09) || (tileNum == 0x0A) || (tileNum == 0x1A) || (tileNum == 0x20) || (tileNum == 0x2E)
             || (tileNum == 0x29) || (tileNum == 0x2A) || (tileNum == 0x70) || (tileNum == 0x72) || (tileNum == 0xB9))
            {
                if (!((switchToEdit < 0) && (tileNum != 0x1A) && (tileNum != 0x20) && (tileNum != 0x2E)))
                    newSelection = QRect(xTile * tileSize.width(), yTile * tileSize.height(), tiles[tileNum].w * tileSize.width()-1, tiles[tileNum].h * tileSize.height()-1);
            }
            else
            {
                spriteNum = getSpriteAtXY((float)e->x() / scaleFactorX, (float)e->y() /  scaleFactorY, &spriteRect);
                if ((spriteNum != -1) && (sprites.at(spriteNum).id == 0x54))
                    newSelection = spriteRect;
            }
        }

        if (newSelection != QRect(0,0,0,0))
        {
            mouseOverTile = newSelection;
            update();
        }
        else if (mouseOverTile != QRect())
        {
            mouseOverTile = QRect();
            update();
        }
        swObjToMove = -1;
    }

    if (e->buttons() != Qt::LeftButton)
        return;

    // box gets moved
    // calculate new x,y
    int newX, newY;

    newX = (float)e->x() / (float)tileSize.width() / scaleFactorX;
    newY = (float)e->y() / (float)tileSize.height() / scaleFactorY;
    xTile = mouseOverTile.x() / tileSize.width();
    yTile = mouseOverTile.y() / tileSize.height();

    if ((newX != xTile) || (newY != yTile))
    {
/*        int i;
        if (switchToEdit < 0)
        {
            for (i = 0; i < currentSwitches.size(); i++)
                if ((currentSwitches.at(i).x == xTile) && (currentSwitches.at(i).y == yTile))
                    break;

            if (i == currentSwitches.size())
            {
                qWarning() << "moved unkown switch?!" << mouseOverTile;
                return;
            }

            currentSwitches[i].x = newX;
            currentSwitches[i].y = newY;
        }*/
        if (switchToEdit > -1)
        {
            if (switchToEdit >= currentSwitches.size())
                return;

            if (swObjToMove == -1)
                    return;

            if (swObjToMove > currentSwitches.at(switchToEdit).connectedTo.size())
                    return;

            if (swObjToMove == currentSwitches.at(switchToEdit).connectedTo.size())
            {
                if ((currentSwitches.at(switchToEdit).x != newX) || (currentSwitches.at(switchToEdit).y != newY))
                {
                        currentSwitches[switchToEdit].x = newX;
                        currentSwitches[switchToEdit].y = newY;
                        mouseOverTile = QRect(newX * tileSize.width(), newY * tileSize.height(), tileSize.width()-1, tileSize.height()-1);
                        dataIsChanged = true;
                        emit switchUpdated(switchToEdit, &currentSwitches[switchToEdit]);
                        update();
                }
            }
            else if ((currentSwitches.at(switchToEdit).connectedTo.at(swObjToMove).x != newX) || (currentSwitches.at(switchToEdit).connectedTo.at(swObjToMove).y != newY))
            {
                    currentSwitches[switchToEdit].connectedTo[swObjToMove].x = newX;
                    currentSwitches[switchToEdit].connectedTo[swObjToMove].y = newY;
                    mouseOverTile = QRect(newX * tileSize.width(), newY * tileSize.height(), tileSize.width()-1, tileSize.height()-1);
                    dataIsChanged = true;
                    emit switchUpdated(switchToEdit, &currentSwitches[switchToEdit]);
                    update();
            }
        }

    }

}

void QDKEdit::mousePressEvent(QMouseEvent *e)
{
    if (!switchMode)
    {
        QTileEdit::mousePressEvent(e);
        return;
    }

    if (e->type() == QEvent::MouseButtonPress)
        mousePressed = true;

    if ((e->button() == Qt::LeftButton) || (e->button() == Qt::RightButton) || (e->button() == Qt::MiddleButton))
    {
        if (mouseOverTile != QRect())
        {
            int xTile = mouseOverTile.x() / tileSize.width();
            int yTile = mouseOverTile.y() / tileSize.height();

            if (switchToEdit < 0)
            {
                int i;
                for (i = 0; i < currentSwitches.size(); i++)
                    if ((currentSwitches.at(i).x == xTile) && (currentSwitches.at(i).y == yTile))
                    {
                        switchToEdit = i;
                        swObjToMove = currentSwitches.at(i).connectedTo.size();
                        break;
                    }

                if ((e->button() == Qt::LeftButton) && (i == currentSwitches.size()))
                {
                    // add a new switch
                    QDKSwitch newSwitch;
                    newSwitch.state = 0;
                    newSwitch.x = xTile;
                    newSwitch.y = yTile;
                    newSwitch.levelPos = yTile * levelDimension.width() + xTile;
                    newSwitch.ramPos = newSwitch.levelPos + 0xD44D;

                    currentSwitches.append(newSwitch);
                    switchToEdit = currentSwitches.size() - 1;

                    dataIsChanged = true;
                    emit switchAdded(&currentSwitches[currentSwitches.size()-1]);
                    update();
                }
            }
            else
            {
                int i;
                for (i = 0; i < currentSwitches.at(switchToEdit).connectedTo.size(); i++)
                    if ((currentSwitches.at(switchToEdit).connectedTo.at(i).x == xTile) && (currentSwitches.at(switchToEdit).connectedTo.at(i).y == yTile))
                    {
                        swObjToMove = i;
                        break;
                    }

                // additional check for moving boards --- xTile is incorrect because of drawOffset
                if (i == currentSwitches.at(switchToEdit).connectedTo.size())
                {
                    for (i = 0; i < currentSwitches.at(switchToEdit).connectedTo.size(); i++)
                        if (currentSwitches.at(switchToEdit).connectedTo.at(i).isSprite && (currentSwitches.at(switchToEdit).connectedTo.at(i).x == xTile+1) && (currentSwitches.at(switchToEdit).connectedTo.at(i).y == yTile))
                        {
                            swObjToMove = i;
                            break;
                        }
                }

                if ((i == currentSwitches.at(switchToEdit).connectedTo.size()) && (currentSwitches.at(switchToEdit).x == xTile) && (currentSwitches.at(switchToEdit).y == yTile))
                    swObjToMove = currentSwitches.at(switchToEdit).connectedTo.size();

                if ((e->button() == Qt::LeftButton) && (i == currentSwitches.at(switchToEdit).connectedTo.size()) && ((currentSwitches.at(switchToEdit).x != xTile) || (currentSwitches.at(switchToEdit).y != yTile)))
                {
                    // add a new object to switchToEdit
                    QDKSwitchObject newObj;
                    newObj.x = xTile;
                    newObj.y = yTile;

                    int tileNum = getTile(xTile, yTile);
                    if ((tileNum == 0x09) || (tileNum == 0x0A) || (tileNum == 0x1A) || (tileNum == 0x20) || (tileNum == 0x2E)
                     || (tileNum == 0x29) || (tileNum == 0x2A) || (tileNum == 0x70) || (tileNum == 0x72) || (tileNum == 0xB9))
                    {
                        //tile
                        newObj.isSprite = false;
                        newObj.levelPos = yTile * levelDimension.width() + xTile;
                        newObj.ramPos = newObj.levelPos + 0xD44D;
                    }
                    else //sprite
                    {
                        newObj.isSprite = true;
                        newObj.x += 1;
                        newObj.levelPos = yTile * levelDimension.width() + xTile + 1;
                        newObj.ramPos = newObj.levelPos + 0xDA75;
                    }

                    currentSwitches[switchToEdit].connectedTo.append(newObj);
                    swObjToMove = currentSwitches[switchToEdit].connectedTo.size() - 1;
                    dataIsChanged = true;
                    emit switchUpdated(switchToEdit, &currentSwitches[switchToEdit]);
                    update();
                }
            }

            update();
        }
        else
        {
            switchToEdit = -1;
            swObjToMove = -1;
            update();
            mouseMoveEvent(e);
        }
    }

    if (e->button() == Qt::RightButton)
    {
        if (swObjToMove != -1)
        {
            // check for switch
            if (swObjToMove == currentSwitches.at(switchToEdit).connectedTo.size())
            {
                currentSwitches[switchToEdit].connectedTo.clear();
                currentSwitches.removeAt(switchToEdit);
                dataIsChanged = true;
                emit switchRemoved(switchToEdit);
                swObjToMove = -1;
                switchToEdit = -1;
            }
            else // object
            {
                currentSwitches[switchToEdit].connectedTo.removeAt(swObjToMove);
                swObjToMove = -1;
                dataIsChanged = true;
                emit switchUpdated(switchToEdit, &currentSwitches[switchToEdit]);
            }
        }
    }

    if ((e->button() == Qt::MiddleButton) && (switchToEdit != -1) && (swObjToMove != -1) && (swObjToMove == currentSwitches.at(switchToEdit).connectedTo.size()))
    {
        if (currentSwitches.at(switchToEdit).state < 2)
            currentSwitches[switchToEdit].state++;
        else
            currentSwitches[switchToEdit].state = 0;

        dataIsChanged = true;
        emit switchUpdated(switchToEdit, &currentSwitches[switchToEdit]);
    }
}


void QDKEdit::paintLevel(QPainter *painter)
{
    QTileEdit::paintLevel(painter);

    int rawPos;

    if (!switchMode)
        return;

    if (switchToEdit < 0) // draw all switch object positions
    {
        painter->setPen(Qt::blue);
        for (int i = 0; i < currentSwitches.size(); i++)
        {
            rawPos = (currentSwitches.at(i).y * levelDimension.width() + currentSwitches.at(i).x) * 2;

            if ((lvlData[rawPos] == 0x1A) || (lvlData[rawPos] == 0x20) || (lvlData[rawPos] == 0x2E))
            {
                painter->setPen(Qt::blue);
                painter->drawRect(QRect(currentSwitches.at(i).x*tileSize.width(), currentSwitches.at(i).y*tileSize.height(), tileSize.width()*2-1, tileSize.height()*2-1));
            }
            else
            {
                painter->setPen(Qt::red);
                painter->drawRect(QRect(currentSwitches.at(i).x*tileSize.width(), currentSwitches.at(i).y*tileSize.height(), tileSize.width()-1, tileSize.height()-1));
            }
        }
    }
    else // draw only selected switch and its connected objects
    {
        rawPos = (currentSwitches.at(switchToEdit).y * levelDimension.width() + currentSwitches.at(switchToEdit).x) * 2;

        if ((lvlData[rawPos] == 0x1A) || (lvlData[rawPos] == 0x20) || (lvlData[rawPos] == 0x2E))
        {
            painter->setPen(Qt::red);
            painter->drawRect(QRect(currentSwitches.at(switchToEdit).x*tileSize.width(), currentSwitches.at(switchToEdit).y*tileSize.height(), tileSize.width()*2-1, tileSize.height()*2-1));
        }
        else
        {
            painter->setPen(Qt::magenta);
            painter->drawRect(QRect(currentSwitches.at(switchToEdit).x*tileSize.width(), currentSwitches.at(switchToEdit).y*tileSize.height(), tileSize.width()-1, tileSize.height()-1));
        }

        for (int  i = 0; i < currentSwitches.at(switchToEdit).connectedTo.size(); i++)
        {
            rawPos = (currentSwitches.at(switchToEdit).connectedTo.at(i).y * levelDimension.width() + currentSwitches.at(switchToEdit).connectedTo.at(i).x) * 2;

            painter->setPen(Qt::blue);
            if (!currentSwitches.at(switchToEdit).connectedTo.at(i).isSprite)
            {
                if ((lvlData[rawPos] == 0x09) || (lvlData[rawPos] == 0x0A)
                 || (lvlData[rawPos] == 0x1A) || (lvlData[rawPos] == 0x20) || (lvlData[rawPos] == 0x2E)
                 || (lvlData[rawPos] == 0x29) || (lvlData[rawPos] == 0x2A) || (lvlData[rawPos] == 0x70)
                 || (lvlData[rawPos] == 0x72) || (lvlData[rawPos] == 0xB9))
                {
                    painter->setPen(Qt::green);
                    painter->drawRect(QRect(currentSwitches.at(switchToEdit).connectedTo.at(i).x*tileSize.width(), currentSwitches.at(switchToEdit).connectedTo.at(i).y*tileSize.height(), (tileSize.width()*tiles[(quint8)lvlData[rawPos]].w)-1, (tileSize.height()*tiles[(quint8)lvlData[rawPos]].h)-1));
                }
                else
                {
                    painter->setPen(Qt::blue);
                    painter->drawRect(QRect(currentSwitches.at(switchToEdit).connectedTo.at(i).x*tileSize.width(), currentSwitches.at(switchToEdit).connectedTo.at(i).y*tileSize.height(), tileSize.width()-1, tileSize.height()-1));
                }

            }
            else
            {
                painter->setPen(Qt::blue);
                int j;
                for (j = 0; j < sprites.size(); j++)
                    if ((sprites.at(j).id == 0x54) && (sprites.at(j).x == currentSwitches.at(switchToEdit).connectedTo.at(i).x) && (sprites.at(j).y == currentSwitches.at(switchToEdit).connectedTo.at(i).y))
                    {
                        painter->setPen(Qt::green);
                        painter->drawRect(QRect(currentSwitches.at(switchToEdit).connectedTo.at(i).x*tileSize.width()-tileSize.width()/2, currentSwitches.at(switchToEdit).connectedTo.at(i).y*tileSize.height(), tileSize.width()*2-1, tileSize.height()-1));
                        break;
                    }
                if (j >= sprites.size())
                    painter->drawRect(QRect(currentSwitches.at(switchToEdit).connectedTo.at(i).x*tileSize.width(), currentSwitches.at(switchToEdit).connectedTo.at(i).y*tileSize.height(), tileSize.width()-1, tileSize.height()-1));
            }
        }
    }

    // redraw selection since it might get covered
    painter->setPen(Qt::gray);
    painter->drawRect(mouseOverTile.x(), mouseOverTile.y(), mouseOverTile.width(), mouseOverTile.height());
}
