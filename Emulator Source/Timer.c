#include <cstdio>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "MMU.h"
#include "Timer.h"


void TimerInit(Timer *Timer, MMU *MMU) {
    //Initialize the Timer Registers
    MMU->SystemMemory[0xFF04] = 0x00; //DIV
    MMU->SystemMemory[0xFF05] = 0x00; //TIMA
    MMU->SystemMemory[0xFF06] = 0x0; //TMA
    MMU->SystemMemory[0xFF07] = 0xF8; //TAC

    //Initialize internal Counters
    Timer->DivTickCount = 0;
    Timer->TimaTickCount = 0;

    return;
}

void TimerTick(Timer *Timer, MMU *MMU) {
    //Div is updated every 256 ticks (64 M-Cycles) (Same as TAC mode 11)
    if (Timer->DivTickCount >= 256) {
        MMU->SystemMemory[0xFF04]++;
        Timer->DivTickCount = 0;
    }
    else {
        Timer->DivTickCount++;
    }

    //TAC Controls the Timer
    //M Cycle = 4 Ticks
    //00 - 256 M-Cycles, 01 - 4 M-Cycles, 10 - 16 M-Cycles, 11 - 64 M-Cycles.
    //00 - 256 * 4 = 1024 Ticks , 01 - 4 * 4 = 16 Ticks, 10 - 16 * 4 = 64 Ticks, 11 - 64 * 4 = 256 Ticks. (What we really care about)
    //00 - 4096 Hz, 01 - 262144 Hz, 10 - 65536 Hz, 11 - 16384 Hz.

    // Update TIMA register based on TAC settings
    if (MMU->SystemMemory[0xFF07] & 0x04) { // Check if TAC is enabled
        
        uint16_t Threshold;
        switch (MMU->SystemMemory[0xFF07] & 0x03) {
            case 0x00: Threshold = 1024; break;
            case 0x01: Threshold = 16; break;
            case 0x02: Threshold = 64; break;
            case 0x03: Threshold = 256; break;
        }

        Timer->TimaTickCount++;

        if (Timer->TimaTickCount >= Threshold) {
            if (MMU->SystemMemory[0xFF05] == 0xFF) { // Check for overflow
                MMU->SystemMemory[0xFF05] = MMU->SystemMemory[0xFF06]; // Set TIMA to TMA
                MMU->SystemMemory[0xFF0F] |= 0x04; // Set Timer Overflow Flag
            } 
            else {
                MMU->SystemMemory[0xFF05]++; // Increment TIMA
            }
            Timer->TimaTickCount = 0;
        }
    }

    return;
}