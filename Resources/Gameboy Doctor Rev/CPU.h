#ifndef CPU_H
#define CPU_H

#include <stdint.h> 
#include "MMU.h"

typedef struct {
    //Registers
    uint8_t RegA;
    uint8_t RegF;
    
    uint8_t RegB;
    uint8_t RegC;
    
    uint8_t RegD;
    uint8_t RegE;

    uint8_t RegH;
    uint8_t RegL;

    uint16_t SP;
    uint16_t PC;

    // Flags
    uint8_t HALT;
    uint8_t IME; // Interrupt Master Enable Flag
} CPU;

void CPUInit(CPU *CPU);
void CPUTick(CPU *CPU, MMU *MMU);
uint8_t CPUExecuteInstruction(CPU *CPU, MMU *MMU);
uint8_t CPUExecuteCB(CPU *CPU, MMU *MMU);

// Register Functions

uint16_t GetAF(CPU *CPU);
void UpdateAF(CPU *CPU, uint16_t value);

uint16_t GetBC(CPU *CPU);
void UpdateBC(CPU *CPU, uint16_t value);

uint16_t GetDE(CPU *CPU);
void UpdateDE(CPU *CPU, uint16_t value);

uint16_t GetHL(CPU *CPU);
void UpdateHL(CPU *CPU, uint16_t value);

//Gameboy Doctor Log
void CPULOG(CPU *CPU, MMU *MMU);

#endif 