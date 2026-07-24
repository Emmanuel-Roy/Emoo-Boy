#include <cstdio>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "MMU.h"
#include "PPU.h"
#include <time.h>

extern int ROMSize; 
extern int RAMSize; 
extern int LoadSaveFile;
extern char ROMFilePath[512]; 
extern char RAMFilePath[512]; 
extern int MBCType;
extern int Exit;
extern int RenderingSpeed;
extern int TargetFPS;

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

    // CGB Initializations
    MMU->isCGB = 0;
    MMU->VBK = 0;
    MMU->SVBK = 1;
    MMU->HDMA1 = MMU->HDMA2 = MMU->HDMA3 = MMU->HDMA4 = 0;
    MMU->HDMA5 = 0xFF;
    for (int b = 0; b < 2; b++) {
        for (int i = 0; i < 0x2000; i++) MMU->VRAM[b][i] = 0x00;
    }
    for (int b = 0; b < 8; b++) {
        for (int i = 0; i < 0x1000; i++) MMU->WRAM[b][i] = 0x00;
    }

    int defaultKeys[8] = {SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT, SDLK_z, SDLK_x, SDLK_a, SDLK_s};
    for (int i = 0; i < 8; i++) {
        MMU->GameBoyController[i] = 1;
        MMU->GameBoyKeyMap[i] = defaultKeys[i];
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

    // CGB Header Detection (Byte 0x0143)
    if ((MMU->ROMFile[0x0143] & 0x80) != 0) {
        MMU->isCGB = 1;
        printf("[CGB MODE] Game Boy Color ROM Detected! Header Flag: 0x%02X\n", MMU->ROMFile[0x0143]);
    } else {
        MMU->isCGB = 0;
        printf("[DMG MODE] Game Boy Mono ROM Detected.\n");
    }

    //Load the first two banks of ROM into the system memory
    memcpy(MMU->SystemMemory, MMU->ROMFile, 0x8000); //Load ROM Bank 0-1 into the system memory location 0x0000-0x7FFF

    //check if the user wanted to load a ram file, and if so, load the data.
    if ((RAMSize > 0) && (LoadSaveFile == 1)) {
        FILE *ramfile = fopen(RAMFilePath, "rb");
        if (ramfile != NULL) {
            size_t ramBytesRead = fread(MMU->RAMFile, 1, RAMSize, ramfile);
            fclose(ramfile);
        } else {
            memset(MMU->RAMFile, 0xFF, RAMSize);
            printf("Save file %s not found. A new save file will be created on exit.\n", RAMFilePath);
        }
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
    if (MMU->MBC != 0x19) { // Unless MBC5
        if ((bank & 0x1F) == 0) bank |= 1;
    }
    if (MMU->NumROMBanks > 0) bank = bank % MMU->NumROMBanks;
    size_t BaseAddress = (size_t)0x4000 * bank;
    if (BaseAddress + 0x4000 <= (size_t)ROMSize) {
        memcpy(MMU->SystemMemory + 0x4000, MMU->ROMFile + BaseAddress, 0x4000);
    }
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

    // CGB VRAM Banking (0x8000 - 0x9FFF)
    if (address >= 0x8000 && address <= 0x9FFF) {
        if (MMU->isCGB) return MMU->VRAM[MMU->VBK & 1][address - 0x8000];
        return MMU->SystemMemory[address];
    }

    // CGB WRAM Banking (0xD000 - 0xDFFF)
    if (address >= 0xD000 && address <= 0xDFFF) {
        if (MMU->isCGB) {
            uint8_t bank = (MMU->SVBK & 0x07) ? (MMU->SVBK & 0x07) : 1;
            return MMU->WRAM[bank][address - 0xD000];
        }
        return MMU->SystemMemory[address];
    }

    if (address >= 0xE000 && address <= 0xFDFF) {
        return MMU->SystemMemory[address - 0x2000];
    }

    // CGB Registers
    if (MMU->isCGB) {
        if (address == 0xFF4F) return MMU->VBK | 0xFE;
        if (address == 0xFF70) return MMU->SVBK | 0xF8;
        if (address == 0xFF55) return MMU->HDMA5;
        if (MMU->ppuPtr) {
            PPU *ppu = (PPU*)MMU->ppuPtr;
            if (address == 0xFF68) return ppu->BCPS;
            if (address == 0xFF69) return ppu->BGPRAM[ppu->BCPS & 0x3F];
            if (address == 0xFF6A) return ppu->OCPS;
            if (address == 0xFF6B) return ppu->OBPRAM[ppu->OCPS & 0x3F];
        }
    }

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

    if (address >= 0x2000 && address <= 0x3FFF) {
        if (MMU->NumROMBanks > 2) {
            MMUSwapROMBank(MMU, value);
        }
        return;
    }

    if (address >= 0x4000 && address <= 0x5FFF && (MMU->NumRAMBanks > 0)) {
        if (MMU->MBC == 0x10) {
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
        return;
    }

    // CGB VRAM Write (0x8000 - 0x9FFF)
    if (address >= 0x8000 && address <= 0x9FFF) {
        MMU->SystemMemory[address] = value;
        MMU->VRAM[MMU->VBK & 1][address - 0x8000] = value;
        return;
    }

    // CGB WRAM Bank Write (0xD000 - 0xDFFF)
    if (address >= 0xD000 && address <= 0xDFFF) {
        MMU->SystemMemory[address] = value;
        if (MMU->isCGB) {
            uint8_t bank = (MMU->SVBK & 0x07) ? (MMU->SVBK & 0x07) : 1;
            MMU->WRAM[bank][address - 0xD000] = value;
        }
        return;
    }

    // CGB Registers
    if (address == 0xFF4F) { // VBK VRAM Bank
        MMU->VBK = value & 0x01;
        return;
    }
    if (address == 0xFF70) { // SVBK WRAM Bank
        MMU->SVBK = value & 0x07;
        return;
    }
    if (address == 0xFF51) { MMU->HDMA1 = value; return; }
    if (address == 0xFF52) { MMU->HDMA2 = value & 0xF0; return; }
    if (address == 0xFF53) { MMU->HDMA3 = value & 0x1F; return; }
    if (address == 0xFF54) { MMU->HDMA4 = value & 0xF0; return; }
    if (address == 0xFF55) { // CGB GDMA/HDMA Trigger
        MMU->HDMA5 = value;
        if ((value & 0x80) == 0) { // GDMA Transfer
            uint16_t src = (MMU->HDMA1 << 8) | MMU->HDMA2;
            uint16_t dst = 0x8000 | ((MMU->HDMA3 << 8) | MMU->HDMA4);
            uint16_t length = ((value & 0x7F) + 1) * 16;
            for (uint16_t i = 0; i < length; i++) {
                uint8_t val = MMURead(MMU, src + i);
                MMUWrite(MMU, dst + i, val);
            }
        }
        return;
    }

    if (MMU->ppuPtr) {
        PPU *ppu = (PPU*)MMU->ppuPtr;
        if (address == 0xFF68) { ppu->BCPS = value; return; }
        if (address == 0xFF69) {
            ppu->BGPRAM[ppu->BCPS & 0x3F] = value;
            if (ppu->BCPS & 0x80) {
                ppu->BCPS = (ppu->BCPS & 0x80) | ((ppu->BCPS + 1) & 0x3F);
            }
            return;
        }
        if (address == 0xFF6A) { ppu->OCPS = value; return; }
        if (address == 0xFF6B) {
            ppu->OBPRAM[ppu->OCPS & 0x3F] = value;
            if (ppu->OCPS & 0x80) {
                ppu->OCPS = (ppu->OCPS & 0x80) | ((ppu->OCPS + 1) & 0x3F);
            }
            return;
        }
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
            }
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                MMUSaveFile(MMU);
                MMUFree(MMU);
                exit(0);
            }
            if (event.key.keysym.sym == SDLK_q) {
                TargetFPS -= 15;
                if (TargetFPS < 15) TargetFPS = 15;
                printf("[EMULATOR SPEED] Slowing Down -> %d FPS (%.2fx speed)\n", TargetFPS, (float)TargetFPS / 60.0f);
            }
            if (event.key.keysym.sym == SDLK_w) {
                TargetFPS += 30;
                if (TargetFPS > 300) TargetFPS = 300;
                printf("[EMULATOR SPEED] Fast Forwarding -> %d FPS (%.2fx speed)\n", TargetFPS, (float)TargetFPS / 60.0f);
            }
            if (event.key.keysym.sym == SDLK_r || event.key.keysym.sym == SDLK_e) {
                TargetFPS = 60;
                printf("[EMULATOR SPEED] Reset Speed -> 60 FPS (1.00x speed)\n");
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

