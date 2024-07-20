#ifndef DMG_H
#define DMG_H

#include "CPU.h"
#include "PPU.h"
#include "MMU.h"
#include "Timer.h"
//#include "APU

extern SDL_Window *window;
extern SDL_Renderer *renderer;
extern SDL_Texture* texture;
extern int SCALE;

typedef struct {
    CPU DMG_CPU;
    PPU DMG_PPU;
    MMU DMG_MMU;
    Timer DMG_Timer;
    //APU DMG_APU;
} DMG;


void DMGInit(DMG *DMG);
void DMGGraphicsInit();
void DMGTick(DMG *DMG);
int DMGRenderThread(void *data);

#endif