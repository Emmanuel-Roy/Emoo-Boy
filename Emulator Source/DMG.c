#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "DMG.h"

extern SDL_Window *window;
extern SDL_Renderer *renderer;
extern SDL_Texture* texture;
extern int SCALE;

void DMGTick(DMG *DMG) {
    //M Cycle = 4 Ticks

    //Tick CPU
    CPUTick(&DMG->DMG_CPU, &DMG->DMG_MMU);
    
    //Update PPU (Later on potentially set rendering to happen after all ticks are done and the system in mode 3 instead of only on a scanline by scanline basis.)
    PPUTick(&DMG->DMG_PPU, &DMG->DMG_MMU);
        
    //Update OAM in MMU
    MMUTick(&DMG->DMG_MMU);
        
    //Update Timer
    TimerTick(&DMG->DMG_Timer, &DMG->DMG_MMU);
        
    //Update APU
    //APUStep(&DMG->DMG_APU, &DMG->DMG_MMU);
    //SDL Delay for 16 ms (60 FPS) (The actual gameboy was around 59.7 FPS, but SDL delay only takes integers :/ )
}



void DMGInit(DMG *DMG) {
    //Set up SDL Window
    DMGGraphicsInit();
    //Load ROM into Memory, aswell as ram if the user wants to save it.
    CPUInit(&DMG->DMG_CPU);

    MMUInit(&DMG->DMG_MMU);
    MMULoadFile(&DMG->DMG_MMU);
    //Initialize the CPU, PPU, APU, and Timer
    TimerInit(&DMG->DMG_Timer, &DMG->DMG_MMU);
    PPUInit(&DMG->DMG_PPU, &DMG->DMG_MMU);
    //APUInit() //Not implemented yet
}

void DMGGraphicsInit() {
    // SDL initialization and window + renderer creation
    SDL_Init(SDL_INIT_EVERYTHING);
    window = SDL_CreateWindow("Emoo-Boy", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, (160 * SCALE), (144 * SCALE), SDL_WINDOW_ALLOW_HIGHDPI);
    renderer = SDL_CreateRenderer(window, -1, 0);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, 160, 144);
}

