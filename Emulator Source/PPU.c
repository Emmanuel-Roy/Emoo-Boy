#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "MMU.h"
#include "PPU.h"

extern SDL_Window *window;
extern SDL_Renderer *renderer;
extern SDL_Texture* texture;
extern int SCALE;
extern int DMGPalette[12];
extern int RenderingSpeed;

/*
    LCDC = MMU->SystemMemory[0xFF40]; //LCD Control Register
    SCY = MMU->SystemMemory[0xFF42]; //Shows which area of Background is displayed 
    SCX = MMU->SystemMemory[0xFF43]; //Shows which area of Background is displayed 
    WY = MMU->SystemMemory[0xFF4A]; //Shows which area of Window is displayed 
    WX = MMU->SystemMemory[0xFF4B]; //Shows which area of Window is displayed  
*/

/*PPU Tick (Needs a ton of work, need to fix timing issues and account for Mode 3 Penalities.
  Time to Render One frame: 70224 dots or around 59.7 fps */
  //Roughly 456 cycles per scanline is around 114 CPU cycles per scanline.
void PPUTick(PPU *PPU, MMU *MMU) {
    
    uint8_t LY = MMU->SystemMemory[0xFF44]; //Current Scanline
    uint8_t LYC = MMU->SystemMemory[0xFF45]; //LY Compare
    uint8_t STAT = MMU->SystemMemory[0xFF41]; //STAT Register

    //Check if PPU is turned on (LCDC Bit 7)
    if ((MMU->SystemMemory[0xFF40] & (1 << 7)) == 0) {
        PPU->CurrentX = 0;
        PPU->WindowLineCounter = 0;

        MMU->SystemMemory[0xFF44] = 0;
        PPU->Mode3Length = 252;

        MMU->SystemMemory[0xFF41] = (MMU->SystemMemory[0xFF41] & ~0x03); //Set Mode to HBlank

        
        return; //Break if PPU is disabled
    } 


    //Check if LY == LYC
    if (LY == LYC) {
        STAT |= 0x04; //Set Flag
        if (STAT & 0x40) {
            MMU->SystemMemory[0xFF0F] |= 0x02; //Set STAT Interrupt
        }
    } 
    else {
        MMU->SystemMemory[0xFF0F] &= ~0x02; //Reset Flag
    }


    //Check if in VBlank (Mode 1)
    if (LY >= 144) {
        if ((LY == 144) && (PPU->CurrentX == 0)) {
            //VBlank Interrupt
            MMU->SystemMemory[0xFF0F] |= 0x01; //Set VBlank Interrupt

            if (STAT & 0x10) {
                MMU->SystemMemory[0xFF0F] |= 0x02; //Set STAT Interrupt
            }
        }

        MMU->SystemMemory[0xFF41] = (MMU->SystemMemory[0xFF41] & 0xFC) | (1 & 0x03);  //Set Mode to VBlank
        
        PPU->CurrentX++; //Increment X

        if (PPU->CurrentX == 456) { //End of Scanline
            //Move to next scanline
            PPU->CurrentX = 0;
            PPU->NumSpritePixels = 0; //Reset Sprite Pixel Count
            LY += 1;
            MMU->SystemMemory[0xFF44] = LY;
            PPU->Mode3Length = 252;
        }

        if (LY > 153) { //End of VBlank (4560 Cycles)
            PPU->CurrentX = 0;
            PPU->WindowLineCounter = 0;
            LY = 0;
            MMU->SystemMemory[0xFF44] = LY;
            PPU->Mode3Length = 252;
        }

        return; //Prevents from X from reseting until V-Blank ends.
    }

    if (PPU->CurrentX < 80) { //OAM Search (Always takes 80 Cycles) (Mode 2)
        if (PPU->CurrentX == 0) {
            if (STAT & 0x20) {
                MMU->SystemMemory[0xFF0F] |= 0x02; //Set STAT Interrupt
            }
        }
        if (PPU->CurrentX == 0) {
            PPUOAMSearch(PPU, MMU, LY); //Scan memory and add to sprite map during last tick of OAM Scan.
        }
        
        PPU->CurrentX += 1;
        MMU->SystemMemory[0xFF41] = (MMU->SystemMemory[0xFF41] & 0xFC) | (2 & 0x03);  //Set Mode to OAM Search
        return;
    }

    if (PPU->CurrentX >= 80 && PPU->CurrentX < PPU->Mode3Length) {  
        //Drawing Pixels (Always takes from 172-289 Cycles)
        //For Simplicities sake, I am assuming each drawing session takes 172 Cycles.
        //I will come back and account for timing penelties later.
        //Mode 3

        MMU->SystemMemory[0xFF41] = (MMU->SystemMemory[0xFF41] & 0xFC) | (3 & 0x03);  //Set Mode to Drawing Pixels (This way the MMU can block off access to VRAM)

        if (PPU->CurrentX < 240) { //Draw only the 160 pixels, not the additional 12 needed for cycle accuracy.
            PPUDraw(PPU, MMU, (PPU->CurrentX - 80), LY);
        }
        //Draw Each Pixel.
        PPU->CurrentX += 1;
        return;
    }
    
    if (PPU->CurrentX < 456) { //HBlank (Always takes from 87-204 Cycles)
        //Mode 0
        if (PPU->CurrentX == PPU->Mode3Length) {
            if (STAT & 0x08) {
                MMU->SystemMemory[0xFF0F] |= 0x02; //Set STAT Interrupt
            }
        }
        //We will assume this always takes the full 204 cycles.
        MMU->SystemMemory[0xFF41] = (MMU->SystemMemory[0xFF41] & 0xFC) | (0 & 0x03); //Set Mode to HBlank
        PPU->CurrentX += 1;
        return;
    }

    if (PPU->CurrentX == 456) { //End of Scanline
            //PPUPushPixel(PPU); //Render Scanline
        if (PPU->ScanlineDelay == RenderingSpeed) {
            SDL_Delay(1); //Delay for 1 ms
            PPU->ScanlineDelay = 0;
        } 
        else {
            PPU->ScanlineDelay++;
        }
        //if window pixels were drawn this scanline increment 
        if (PPU->haswindow > 0) {
            PPU->WindowLineCounter++;
        }
        PPU->haswindow = 0;
        
        /* Original Buggy Implementation
        if ((MMU->SystemMemory[0xFF40] & 0x20) && (LY >= MMU->SystemMemory[0xFF4A])) {
            PPU->WindowLineCounter++;
        }
        */
        
        //Move to the next scanline
        PPU->CurrentX = 0;
        PPU->NumSpritePixels = 0; //Reset Sprite Pixel Count
        LY += 1;
        MMU->SystemMemory[0xFF44] = LY;
        PPU->Mode3Length = 252;
    }
}

void PPUInit(PPU *PPU, MMU *MMU) {
    //Initialize the PPU Registers
    MMU->SystemMemory[0xFF40] = 0x91; //LCDC
    MMU->SystemMemory[0xFF41] = 0x81; //STAT
    MMU->SystemMemory[0xFF42] = 0x00; //SCY
    MMU->SystemMemory[0xFF43] = 0x00; //SCX
    MMU->SystemMemory[0xFF44] = 0x91; //LY
    MMU->SystemMemory[0xFF45] = 0x00; //LYC
    MMU->SystemMemory[0xFF47] = 0xFC; //BGP
    MMU->SystemMemory[0xFF48] = 0xFF; //OBP0
    MMU->SystemMemory[0xFF49] = 0xFF; //OBP1
    MMU->SystemMemory[0xFF4A] = 0x00; //WY
    MMU->SystemMemory[0xFF4B] = 0x00; //WX


    PPU->CurrentX = 252;
    //Clear Displays.
    PPU->BackgroundPixel = 0;
    PPU->WindowPixel = 0;
    PPU->Mode3Length = 252;
    PPU->NumSpritePixels = 0;
    PPU->CurrentX = 0;
    PPU->DEBUG = 0;
    PPU->WindowLineCounter = 0;
    PPU->haswindow = 0;


    PPU->ScanlineDelay = 0; //Delay every 9th scanline.

    for (int i = 0; i < 160; i++)
    {
        for (int j = 0; j < 144; j++)
        {
            PPU->GameBoyDisplay[i][j] = 0;
        }
    }

    return;
}


//Scans OAM memory for sprites
void PPUOAMSearch(PPU *PPU, MMU *MMU, uint8_t LY) {
    PPU->CurrentSpriteNum = 0;
    for (int z = 0; z < 40; z++) { //Max sprites.
        if (PPU->CurrentSpriteNum >= 10) {
            break;
        }

        uint8_t YPos = MMU->SystemMemory[0xFE00 + z * 4];
        uint8_t spriteHeight;
        
        if (MMU->SystemMemory[0xFF40] & 0x04) {
            spriteHeight = 16;
        } 
        else {
            spriteHeight = 8;
        
        } 

        // Check if the sprite is on this scanline
        if (LY >= (YPos - 16) && LY < (YPos - 16 + spriteHeight)) {
            PPU->SpriteMap[PPU->CurrentSpriteNum].YPos = YPos;
            PPU->SpriteMap[PPU->CurrentSpriteNum].XPos = MMU->SystemMemory[0xFE01 + z * 4];
            PPU->SpriteMap[PPU->CurrentSpriteNum].TileIndex = MMU->SystemMemory[0xFE02 + z * 4];
            if (spriteHeight == 16){
                PPU->SpriteMap[PPU->CurrentSpriteNum].TileIndex &= 0xFE;
            }
            PPU->SpriteMap[PPU->CurrentSpriteNum].Flags = MMU->SystemMemory[0xFE03 + z * 4];
            PPU->CurrentSpriteNum++;
        }
    }
}

/*Update Background or Window Maps
    This function essentially updates the background and window map that the Gameboy uses to render tiles based on the current pixel.
*/
void PPUUpdateMap(PPU *PPU, MMU *MMU, uint8_t MODE, uint8_t x, uint8_t y) { //0 for background, 1 for Window

    
    //Initialize Variables
    uint16_t TMAPLocationStart, TDATALocationStart, TileLocation;
    uint8_t ChoiceMask;
    
    uint8_t BackgroundPortX = MMU->SystemMemory[0xFF43];
    uint8_t BackgroundPortY = MMU->SystemMemory[0xFF42];
    
    int8_t WindowPortX = MMU->SystemMemory[0xFF4B]-7;
    //Ensure handling of Negative Numbers
    if (WindowPortX < 0) {
            WindowPortX = 0;
    }
    int8_t WindowPortY = MMU->SystemMemory[0xFF4A];


    //Calculate the actualX and Y value of the pixel based on the MODE
    uint8_t pixelX;
    uint8_t pixelY;

    if (MODE == 0) {
        ChoiceMask = 0x08; //Affects the Background Tile Map Location
        pixelX = (x + BackgroundPortX) % 256;
        pixelY = (y + BackgroundPortY) % 256;
    }
    else {
        ChoiceMask = 0x40; //Affects the Window Tile Map Location
        pixelX = (x + WindowPortX) % 256;
        WindowPortY = PPU->WindowLineCounter;
        pixelY = PPU->WindowLineCounter;
    }

    //Check which tile data and map to use
    if (MMU->SystemMemory[0xFF40] & 0x10) {
        TDATALocationStart = 0x8000;
    } else {
        TDATALocationStart = 0x8800;
    }

    if (MMU->SystemMemory[0xFF40] & ChoiceMask) {
        TMAPLocationStart = 0x9C00;
    } else {
        TMAPLocationStart = 0x9800;
    }
    
    //Checks which tile it is on, on the 32x32 grid
    uint8_t tileX = (pixelX / 8) % 32; 
    uint8_t tileY = (pixelY / 8) % 32;
    uint8_t DisplaypixelX = pixelX % 8;
    uint8_t DisplaypixelY = pixelY % 8;


    //Find tile and memory locations
    uint8_t MemLocation; 
    uint16_t OFFSET;
    uint16_t TIleIndexLoc;

    if (MODE == 0) {
        OFFSET = ((32 * (PPU->WindowLineCounter / 8) + (x+7-(BackgroundPortX+7))/8)  & 0x3FFF);
        TIleIndexLoc = TMAPLocationStart + ((32 * (PPU->WindowLineCounter / 8) + (x+7-(BackgroundPortX+7))/8)  & 0x3FFF);
        MemLocation = MMU->SystemMemory[TMAPLocationStart  + tileX + tileY * 32];
    }
    else {
        OFFSET = ((32 * (PPU->WindowLineCounter / 8) + (x+7-(WindowPortY+7))/8)  & 0x3FFF);
        TIleIndexLoc = TMAPLocationStart + ((32 * (PPU->WindowLineCounter / 8) + (x+7-(WindowPortY+7))/8)  & 0x3FFF);
        MemLocation = MMU->SystemMemory[TMAPLocationStart + ((32 * (PPU->WindowLineCounter / 8) + (x+7-(WindowPortX+7))/8)  & 0x3FFF)];
    }

    if (TDATALocationStart == 0x8000) {
        TileLocation = TDATALocationStart + (MemLocation * 16);
    } 
    else {
        TileLocation = 0x9000 + ((int8_t)MemLocation * 16);
    }

    uint8_t LowByte;
    uint8_t HighByte;

    if (MODE == 0) {
        LowByte = MMU->SystemMemory[TileLocation + (((y + BackgroundPortY) % 8) * 2)];
        HighByte = MMU->SystemMemory[TileLocation + (((y + BackgroundPortY) % 8) * 2) + 1];
    }
    else {
        LowByte = MMU->SystemMemory[TileLocation + (((PPU->WindowLineCounter) % 8) * 2)];
        HighByte = MMU->SystemMemory[TileLocation + (((PPU->WindowLineCounter) % 8) * 2) + 1];
    }
    
    //Calculate Color Value based on these bytes
    uint8_t pixelValue = ((HighByte >> (7 - DisplaypixelX)) & 1) << 1 | ((LowByte >> (7 - DisplaypixelX)) & 1);

    pixelValue = MMU->SystemMemory[0xFF47] >> (pixelValue * 2) & 0x03; //Fix Background Palettes.

    //Update the correct map
    if (MODE == 1) {
        PPU->WindowPixel = pixelValue;
    } 
    else {
        PPU->BackgroundPixel = pixelValue;
    }

    if (PPU->DEBUG) {
        /*
        printf("LCDC: %x \n", MMU->SystemMemory[0xFF40]);
        printf("ViewPortX: %d, ViewPortY: %d\n", ViewPortX, ViewPortY);
        printf("pixelX: %d, pixelY: %d\n", pixelX, pixelY);
        printf("DisplaypixelX: %d, DisplaypixelY: %d\n", DisplaypixelX, DisplaypixelY);
        printf("tileX: %d, tileY: %d\n", tileX, tileY);
        printf("\n");
        printf("TMAPLocationStart: %04X, TDATALocationStart: %04X\n", TMAPLocationStart, TDATALocationStart);
        */
        printf("MemLocation: %02X\n", MemLocation);
        printf("TileLocation: %04X\n", TileLocation);
        printf("0x9C00 val is %x \n",MMU->SystemMemory[0x9C00]);
        printf("OFFSET: %04X\n", OFFSET);
        printf("TIleIndexLoc: %04X\n", TIleIndexLoc);
        printf("x: %d, y: %d\n", x, y);
        printf("\n");
        printf("Window Line Counter: %d\n", PPU->WindowLineCounter);
        /*
        printf("LowByte: %02X, HighByte: %02X\n", LowByte, HighByte);
        printf("\n");
        printf("pixelValue: %d\n", pixelValue);
        printf("WindowPixel: %d\n", PPU->WindowPixel);
        printf("BackgroundPixel: %d\n", PPU->BackgroundPixel);
        */
        printf("Mode: %d\n", MODE);
    }
}

//PPU Draw (Searches for Sprites as well) (works by each pixel)
void PPUDraw(PPU *PPU, MMU *MMU, int x, int y) {
    //Gameboy Memory Array-
    //From Dr Mario ROM during Title Screen.
    uint8_t isWindowPixel = 0;
    uint8_t currentPixel = 0xFF;
    uint8_t spriteDrawn = 0;
    //Run code in loop,.
    //Update Background Window Data
    //Using both of these for loops to observe function speed
    /**/
    if (MMU->SystemMemory[0xFF40] & 0x01) {
        //Draw Background.
        PPUUpdateMap(PPU, MMU, 0, x, y);
        currentPixel = PPU->BackgroundPixel;
        PPU->GameBoyDisplay[x][y] = currentPixel;
    }
    
    // Check if the window is enabled
    if (MMU->SystemMemory[0xFF40] & 0x20) {
        // Check if the window should be drawn on this scanline
        if (y >= MMU->SystemMemory[0xFF4A] && x >= (MMU->SystemMemory[0xFF4B] - 7)) {
            PPUUpdateMap(PPU, MMU, 1, x, y);
            // Draw the window pixel, overriding the background pixel if visible.
                currentPixel = PPU->WindowPixel;
                PPU->GameBoyDisplay[x][y] = currentPixel;
                isWindowPixel = 1;
        }
    }

    //Sprite Drawing
     if (MMU->SystemMemory[0xFF40] & 0x02) {
        // Check OAM Memory and draw sprites
        for (int z = 0; z < PPU->CurrentSpriteNum; z++) {
            //Bytes 0-3
            uint8_t YPos = PPU->SpriteMap[z].YPos;
            uint8_t XPos = PPU->SpriteMap[z].XPos;
            uint8_t TileIndex = PPU->SpriteMap[z].TileIndex;
            uint8_t Flags = PPU->SpriteMap[z].Flags;
            uint8_t spriteY = y - (YPos - 16);
            uint8_t spriteX = x - (XPos - 8);
            
            uint8_t spriteHeight;
            uint8_t pixel;
            uint8_t PaletteColor;

            //Check if SPrite is a duplicate.
            //if (spriteX < 0 || spriteX >= 8) continue;
            if (spriteX >= 8) continue;
            
            // Check if the sprite is 8x16 or 8x8
            if (MMU->SystemMemory[0xFF40] & 0x04) {
                spriteHeight = 16;
                TileIndex &= 0xFE;
            }
            else {
                spriteHeight = 8;
            }
                        
            // Check if Sprite needs to be flipped Vertically.
            if (Flags & 0x40) {
                spriteY = spriteHeight - 1 - spriteY;
            }

            // Check if Sprite needs to be flipped horizontally.
            if (Flags & 0x20) {
                spriteX = 7 - spriteX;
            }

            //Long formula that finds the value of each pixel
            pixel = (((MMU->SystemMemory[0x8000 + TileIndex * 16 + spriteY * 2 + 1] >> (7 - spriteX)) & 1) << 1) | ((MMU->SystemMemory[0x8000 + TileIndex * 16 + spriteY * 2] >> (7 - spriteX)) & 1);
                            
            //Get the proper Palette Color
            if (Flags & 0x10) {
                PaletteColor = ((MMU->SystemMemory[0xFF49] >> (pixel * 2)) & 0x03) + 8; // Use OBP1
            }
            else {
                PaletteColor = ((MMU->SystemMemory[0xFF48] >> (pixel * 2)) & 0x03) + 4; // Use OBP0
            }

            //Draw Sprite over if not transparent.
            if ((pixel != 0)) {
                //Check for OAM overlap, SpriteDrawn is just used to account for OAM Priority
                if ((!spriteDrawn) || (z == 0 || XPos < PPU->SpriteMap[z - 1].XPos)) {
                    //Check if Background or Window is Transparent.
                    if ((Flags & 0x80) == 0 || currentPixel == 0) {
                        PPU->GameBoyDisplay[x][y] = PaletteColor;
                        currentPixel = PaletteColor;
                        spriteDrawn = 1;
                        isWindowPixel = 0;
                    }
                }
            }
        }
    }

    if (isWindowPixel == 1) {
        PPU->haswindow++;
    }
}

//Render a pixel to the screen, probably update the screen every scanline.
void PPUPushPixel(PPU *PPU) {

    uint32_t* pixels;
    int pitch;
    
    SDL_LockTexture(texture, NULL, (void**)&pixels, &pitch);
    
    for (int y = 0; y < 144; y++) {
        for (int x = 0; x < 160; x++) {
            uint32_t color;
            switch (PPU->GameBoyDisplay[x][y]) {
                case 0: color = DMGPalette[0]; break; // White
                case 1: color = DMGPalette[1]; break; // Light Gray
                case 2: color = DMGPalette[2]; break; // Dark Gray
                case 3: color = DMGPalette[3]; break; // Black
                //Add support for different OBJ Palettes
                //OBJ Palette 0
                case 4: color = DMGPalette[4]; break; // White
                case 5: color = DMGPalette[5]; break; // Light Gray
                case 6: color = DMGPalette[6]; break; // Dark Gray
                case 7: color = DMGPalette[7]; break; // Black
                //OBJ Palette 1
                case 8: color = DMGPalette[8]; break; // White
                case 9: color = DMGPalette[9]; break; // Light Gray
                case 10: color = DMGPalette[10]; break; // Dark Gray
                case 11: color = DMGPalette[11]; break; // Black
            }
            pixels[y * (pitch / sizeof(uint32_t)) + x] = color;
        }
    }
    SDL_UnlockTexture(texture);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    
    // Scale rendering
    SDL_Rect BaseImage = {0, 0, 160, 144};
    SDL_Rect UpscaledImage = {0, 0, (160 * SCALE), (144 * SCALE)};
    SDL_RenderCopy(renderer, texture, &BaseImage, &UpscaledImage);
    SDL_RenderPresent(renderer);
}
