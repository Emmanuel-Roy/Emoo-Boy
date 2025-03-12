#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "DMG.h"

extern SDL_Window *window;
extern SDL_Renderer *renderer;
extern SDL_Texture* texture;
extern SDL_AudioSpec audio;
extern SDL_AudioSpec audio2;
extern SDL_AudioDeviceID audioDevice;

extern int SCALE;
extern int TargetFPS;


void DMGTick(DMG *DMG) {
    //M Cycle = 4 Ticks

    //Tick CPU
    CPUTick(&DMG->DMG_CPU, &DMG->DMG_MMU);
    
    //Update PPU (Later on potentially set rendering to happen after all ticks are done and the system in mode 3 instead of only on a scanline by scanline basis.)
    PPUTick(&DMG->DMG_PPU, &DMG->DMG_MMU);
        
    //Update DMA
    DMATick(&DMG->DMG_MMU);
        
    //Update Timer
    TimerTick(&DMG->DMG_Timer, &DMG->DMG_MMU);
        
    //Update APU (Every 64 Ticks)
    APUTick(&DMG->DMG_APU, &DMG->DMG_MMU);
}



void DMGInit(DMG *DMG) {
    //Set up SDL Window
    DMGGraphicsInit();
    //Set up CPU
    CPUInit(&DMG->DMG_CPU);
    //Set up system memory
    MMUInit(&DMG->DMG_MMU);
    //Load ROM
    MMULoadFile(&DMG->DMG_MMU);
    //Set up PPU, APU and Timers.
    TimerInit(&DMG->DMG_Timer, &DMG->DMG_MMU);
    PPUInit(&DMG->DMG_PPU, &DMG->DMG_MMU);
    APUInit(&DMG->DMG_APU, &DMG->DMG_MMU); 
    
    //Create a thread for rendering, this speeds up emulation considerably.
    SDL_Thread *thread = SDL_CreateThread(DMGRenderThread, "PPUPushPixelThread", (void*)&DMG->DMG_PPU);
}

void DMGGraphicsInit() {
    // SDL initialization and window + renderer creation
    SDL_Init(SDL_INIT_EVERYTHING);
    window = SDL_CreateWindow("Emoo-Boy", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, (160 * SCALE), (144 * SCALE), SDL_WINDOW_ALLOW_HIGHDPI);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, 160, 144);
    // Audio initialization
    audio.freq = 44100;
    audio.format = AUDIO_S32SYS;
    audio.channels = 2;
    audio.samples = 95; //Adjust to user preference, this is about a frames worth of audio.
    audio.callback = NULL;
    audio.userdata = NULL;

    audioDevice = SDL_OpenAudioDevice(NULL, 0, &audio, &audio2, 0);

    SDL_PauseAudioDevice(audioDevice, 0);
}

int DMGRenderThread(void *data) {
    PPU *ppu = (PPU*)data;
    int FrameStart;
    int FrameTime;
    while (1) {
        //Find the start of each frame.
        FrameStart = SDL_GetTicks();
        
        // Render a frame
        PPUPushPixel(ppu);
        
        //How long it takes to render each frame.
        FrameStart = SDL_GetTicks() - FrameStart;

        // Cap the frame rate to 60 FPS
        if (FrameTime < (1000.0 / TargetFPS)) {
            SDL_Delay((1000.0 / TargetFPS) - FrameTime);
        }
    }
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    return 0;
}