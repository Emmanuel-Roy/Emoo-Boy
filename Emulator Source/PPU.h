#ifndef PPU_H
#define PPU_H

#include <stdio.h>
#include "MMU.h"

extern SDL_Window *window;
extern SDL_Renderer *renderer;
extern SDL_Texture* texture;
extern int SCALE;
extern int DMGPalette[12];
extern int RenderingSpeed;

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
    
    uint8_t DEBUG; //Checks whether to print to console.

    uint8_t haswindow;

    uint8_t ScanlineDelay;

} PPU;




void PPUInit(PPU *PPU, MMU *MMU);

void PPUUpdateMap(PPU *PPU, MMU *MMU, uint8_t MODE, uint8_t x, uint8_t y);
void PPUOAMSearch(PPU *PPU, MMU *MMU, uint8_t LY);

void PPUTick(PPU *PPU, MMU *MMU);
void PPUDraw(PPU *PPU, MMU *MMU, int x, int y);
void PPUPushPixel(PPU *PPU);
#endif // PPU_H