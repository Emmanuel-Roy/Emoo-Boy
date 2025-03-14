#include <cstdio>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "MMU.h"
#include <time.h>

extern int ROMSize; 
extern int RAMSize; 
extern int LoadSaveFile;
extern char ROMFilePath[512]; 
extern char RAMFilePath[512]; 
extern int MBCType;
extern int Exit;
extern int RenderingSpeed;

//Memory Management Functions
void MMUInit(MMU *MMU) {
    MMU->ROMFile = (Uint8 *)malloc(ROMSize * sizeof(uint8_t));
    MMU->RAMFile = (Uint8 *)malloc(RAMSize * sizeof(uint8_t));

    
    //Initialize the System Memory (Set Everything to xFF)
    for (int i = 0; i < 0x10000; i++) {
        MMU->SystemMemory[i] = 0xFF;
    }
    
    MMU->SystemMemory[0xFF00] = 0x0F; //Set GamePad State to 0
    
    //Initialize the number of ROM and RAM Banks
    MMU->NumROMBanks = ROMSize / 0x4000;
    MMU->NumRAMBanks = RAMSize / 0x2000;
    MMU->MBC = MBCType;

    MMU->CurrentROMBank = 1;
    MMU->CurrentRAMBank = 0;
    MMU->Ticks = 0;
    MMU->PrevInstruct = 0;
    MMU->RTCMode = 0;
    MMU->DEBUGMODE = 0;

    MMU->DMASource = 0;
    MMU->DMADestination = 0;
    MMU->DMACount = 0;

    for (int i = 0; i < 8; i++) {
        MMU->GameBoyController[i] = 1;
    }
}
void MMUFree(MMU *MMU) {
    free(MMU->ROMFile);
    free(MMU->RAMFile);
}

//File Functions
void MMULoadFile(MMU *MMU) {
    //Read and Load Data from the Given ROM File
    FILE *romfile = fopen(ROMFilePath, "rb");
    fseek(romfile, 0, SEEK_END);
    size_t fileSize = ftell(romfile);
    fseek(romfile, 0, SEEK_SET);
    size_t bytesRead = fread(MMU->ROMFile, 1, fileSize, romfile);
    fclose(romfile);

    //Load the first two banks of ROM into the system memory
    memcpy(MMU->SystemMemory, MMU->ROMFile, 0x8000); //Load ROM Bank 0-1 into the system memory location 0x0000-0x7FFF

    //check if the user wanted to load a ram file, and if so, load the data.
    if ((RAMSize > 0) && (LoadSaveFile == 1)) {
        FILE *ramfile = fopen(RAMFilePath, "rb");
        size_t ramBytesRead = fread(MMU->RAMFile, 1, RAMSize, ramfile);
        fclose(ramfile);
        //Load the first bank of RAM into the system memory
        memcpy(MMU->SystemMemory + 0xA000, MMU->RAMFile, 0x2000); //Load External RAM Bank 0 into the system memory location 0xA000-0xBFFF
    }
    else {
        memset(MMU->SystemMemory + 0xA000, 0xFF, 0x2000); //fill External RAM Bank 0 with 0xFF
    }
}
void MMUSaveFile(MMU *MMU) {
    if (RAMSize > 0 && LoadSaveFile) {
        //Write Current RAM Bank to RAM File
        size_t CurrentBaseAddress = 0x2000 * MMU->CurrentRAMBank;
        memcpy(MMU->RAMFile + CurrentBaseAddress, MMU->SystemMemory + 0xA000, 0x2000);
        //Save the RAM data to the given file
        if (RAMSize > 0 && LoadSaveFile) {
            FILE *ramfile = fopen(RAMFilePath, "wb");
            fwrite(MMU->RAMFile, 1, RAMSize, ramfile);
            fclose(ramfile);
        }
    }
    else {
        printf("No RAM File Loaded.\n");
        return;
    }
}


//Banking Functions
void MMUSwapROMBank(MMU *MMU, int bank) {
    bank = bank & (MMU->NumROMBanks-1);
    if (bank == 0) {
        bank = 1; //Prevent accesses to the first bank of ROM. Bank 0 is always at 0x0000 - 0x3FFF
    }
    size_t BaseAddress = 0x4000 * bank;
    // Copy the bank data into the system memory at 0x4000-0x7FFF
    memcpy(MMU->SystemMemory + 0x4000, MMU->ROMFile + BaseAddress, 0x4000);
    // Update the current ROM bank
    MMU->CurrentROMBank = bank;
}
void MMUSwapRAMBank(MMU *MMU, int bank) {
    //Copy Current Bank to the RAMFile Pointer
    size_t CurrentBaseAddress = 0x2000 * MMU->CurrentRAMBank;
    memcpy(MMU->RAMFile + CurrentBaseAddress, MMU->SystemMemory + 0xA000, 0x2000);
    //Swap the RAM Bank into System Memory
    size_t BaseAddress = 0x2000 * bank;
    memcpy(MMU->SystemMemory + 0xA000, MMU->RAMFile + BaseAddress, 0x2000);
    // Update the current RAM bank
    MMU->CurrentRAMBank = bank;
}


//Read Write functions for the CPU.
uint8_t MMURead(MMU *MMU, uint16_t address) { 
    
    //RTC Functions
    if ((MMU->MBC == 0x10) && (address >= 0xA000 && address <= 0xBFFF)) {
        //RTC Register Locations
        if (MMU->RTCMode != 0) {
            //Get system time
            time_t t = time(NULL);
            struct tm tm = *localtime(&t);

            //Split day into two 8 bit values
            uint8_t DayLower = tm.tm_mday & 0xFF;
            uint8_t DayHigher = (tm.tm_mday & 0x100) >> 8; //Get the 9th bit of the day.

            //Worst RTC Implementation Ever!!!
            switch (MMU->RTCMode) {
                case 0x08:
                    return tm.tm_sec & 0xFF; //Seconds
                case 0x09:
                    return tm.tm_min & 0xFF; //Minutes
                case 0x0A:
                    return tm.tm_hour & 0xFF; //Hours
                case 0x0B:
                    return DayLower & 0xFF; //Day Lower 8 Bits
                case 0x0C:
                    return DayHigher & 0xFF; //Day Higher 1 Bit, Halt, Day Carry
                default:
                    return 0xFF;
            }
        }
    }

    if (address >= 0xE000 && address <= 0xFDFF) {
        return MMU->SystemMemory[address - 0x2000];
    }

    /* PPU Timing too inaccurate to implement MODE 3 Blocking. */

    if (address == 0xFF00) {
        uint8_t GamepadState = 0xFF;

        // Check action buttons
        if (!(MMU->SystemMemory[0xFF00] & 0x20)) {
            if (!MMU->GameBoyController[4]) GamepadState &= ~0x01; // A
            if (!MMU->GameBoyController[5]) GamepadState &= ~0x02; // B
            if (!MMU->GameBoyController[6]) GamepadState &= ~0x08; // Start
            if (!MMU->GameBoyController[7]) GamepadState &= ~0x04; // Select
        }
        // Check direction buttons
        if (!(MMU->SystemMemory[0xFF00] & 0x10)) {
            if (!MMU->GameBoyController[0]) GamepadState &= ~0x04; // Up
            if (!MMU->GameBoyController[1]) GamepadState &= ~0x08; // Down
            if (!MMU->GameBoyController[2]) GamepadState &= ~0x02; // Left
            if (!MMU->GameBoyController[3]) GamepadState &= ~0x01; // Right
        }
        return GamepadState;
    }

    if (address > 0xFFFF) {
        return 0xFF; //Prevent reads to invalid memory locations.
    }

    return MMU->SystemMemory[address];
}
void MMUWrite(MMU *MMU, uint16_t address, uint8_t value) { 
    if (address <= 0x1FFF) {
        return;
    }

    if ((address >= 0x2000 & address <= 0x3FFF)) {
        if (MMU->NumROMBanks > 2) {
            MMUSwapROMBank(MMU, (value));
        }
        return;
    }

    if (address >= 0x4000 & address <= 0x5FFF & (MMU->NumRAMBanks > 0)) {
        if (MMU->MBC == 0x10) {
            //Pokemon Annoyances
            if (value > 0x07) {
                MMU->RTCMode = value;
                return; 
            }
            else {
                MMU->RTCMode = 0;
            }
        }
        MMUSwapRAMBank(MMU, value & 0x03);
        return;
    }

    if (address >= 0x6000 && address <= 0x7FFF) {
        if (MMU->MBC == 0x10) {
            return; //Add support for latching here.
        }
        return;
    }

    //Echo RAM
    if (address >= 0xE000 && address <= 0xFDFF) {
        MMU->SystemMemory[address - 0x2000] = value;
        return;
    }
    
    //Reset DIV Register
    else if (address == 0xFF04) {
        MMU->SystemMemory[0xFF04] = 0; 
    }

    else if (address == 0xFF46) {
        //DMA Transfer
        MMU->DMASource = value * 0x100;

        //Reset Destination and DMA Counter
        MMU->DMADestination = 0xFE00;
        MMU->DMACount = 0xA0;
    }

    /* PPU Timing too inaccurate to implement MODE 3 Blocking */

    if (address > 0xFFFF) {
        return; //Prevent writes to invalid memory locations.
    }

    //Otherwise, just update the given address in the system memory.
    MMU->SystemMemory[address] = value;
}

/*
External Components that need the entire address bus.
Theoretically both of these could have there own implementation, but they are dead simple to implement as basic functions here.
*/

//DMA Transfer
void DMATick(MMU *MMU) {
    //Transfer 160 bytes of data from DMA Source to 0xFE00-0xFEA0
    if (MMU->DMACount > 0) {
        MMU->SystemMemory[MMU->DMADestination] = MMU->SystemMemory[MMU->DMASource];
        MMU->DMASource++;
        MMU->DMADestination++;
        MMU->DMACount--;
    }
    return;
}
//Update gamepad (Kept in this file instead of DMG.C because the CPU updates it after every instruction)
void DMGUpdateGamePad(MMU *MMU) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            MMUSaveFile(MMU);
            MMUFree(MMU);
            exit(0);
        }
        if (event.type == SDL_KEYDOWN) {
            for (int i = 0; i < 8; i++) {
                if (event.key.keysym.sym == MMU->GameBoyKeyMap[i]) {
                    MMU->GameBoyController[i] = 0;
                }
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    MMUSaveFile(MMU);
                    MMUFree(MMU);
                    exit(0);
                }
                if (event.key.keysym.sym == SDLK_q) {
                    RenderingSpeed--;
                }
                if (event.key.keysym.sym == SDLK_w) {
                    RenderingSpeed++;
                }
                if (event.key.keysym.sym == SDLK_e) {
                    RenderingSpeed = 13;
                }
            }
        }
        if (event.type == SDL_KEYUP) {
            for (int i = 0; i < 8; i++) {
                if (event.key.keysym.sym == MMU->GameBoyKeyMap[i]) {
                    MMU->GameBoyController[i] = 1;
                }
            }
        }
    }
}

