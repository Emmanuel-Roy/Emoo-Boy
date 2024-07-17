#ifndef TIMER_H
#define TIMER_H

#include <stdio.h>

/*
    Timer Register Cheat Sheet.
    MMU->SystemMemory[0xFF04] //DIV
    MMU->SystemMemory[0xFF05] //TIMA
    MMU->SystemMemory[0xFF06] //TMA
    MMU->SystemMemory[0xFF07] //TAC

*/
typedef struct {
    int DivTickCount;
    int TimaTickCount;
} Timer;

void TimerInit(Timer *Timer, MMU *MMU);
void TimerTick(Timer *Timer, MMU *MMU);

#endif // TIMER_H