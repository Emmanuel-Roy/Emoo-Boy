#ifndef PPU_H
#define PPU_H

#include <stdio.h>
#include "MMU.h"

typedef struct {
    uint8_t YPos;
    uint8_t XPos;
    uint8_t TileIndex;
    uint8_t Flags;
} Sprite;

typedef struct {
    uint8_t GameBoyDisplay[160][144];
    uint8_t BackgroundPixel;
    uint8_t WindowPixel;
    uint8_t NumSpritePixels; 

    Sprite SpriteMap[10]; //Max amount of sprites per scanline
    uint8_t CurrentSpriteNum;

    int CurrentX; //Indicates what pixel the PPU is drawing on the struct (Greater than 8 bits)
    uint8_t WindowLineCounter; 
    uint16_t Mode3Length;

    uint8_t haswindow;
    uint8_t ScanlineDelay;

    // CGB Color Support
    uint32_t GameBoyDisplayCGB[160][144]; // 32-bit ARGB output for CGB
    uint8_t BGPRAM[64]; // 8 BG Palettes (64 Bytes)
    uint8_t OBPRAM[64]; // 8 OBJ Palettes (64 Bytes)
    uint8_t BCPS;       // 0xFF68 BG Palette Index
    uint8_t BCPD;       // 0xFF69 BG Palette Data
    uint8_t OCPS;       // 0xFF6A OBJ Palette Index
    uint8_t OCPD;       // 0xFF6B OBJ Palette Data

} PPU;




void PPUInit(PPU *PPU, MMU *MMU);

void PPUUpdateMap(PPU *PPU, MMU *MMU, uint8_t MODE, uint8_t x, uint8_t y);
void PPUOAMSearch(PPU *PPU, MMU *MMU, uint8_t LY);

void PPUTick(PPU *PPU, MMU *MMU);
void PPUDraw(PPU *PPU, MMU *MMU, int x, int y);
void PPUPushPixel(PPU *PPU);
#endif // PPU_H