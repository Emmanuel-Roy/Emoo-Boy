#include <cstdio>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "MMU.h"

extern int ROMSize; 
extern int RAMSize; 
extern int LoadSaveFile;
extern char ROMFilePath[512]; 
extern char RAMFilePath[512]; 
extern int MBCType;
extern int Exit;

//Memory Management Functions
void MMUInit(MMU *MMU) {
    MMU->ROMFile = (Uint8 *)malloc(ROMSize * sizeof(uint8_t));
    MMU->RAMFile = (Uint8 *)malloc(RAMSize * sizeof(uint8_t));

    
    //Initialize the System Memory (Set Everything to xFF)
    for (int i = 0; i < 0x10000; i++) {
        MMU->SystemMemory[i] = 0x00;
    }
    
    MMU->SystemMemory[0xFF00] = 0x0F; //Set GamePad State to 0
    
    //Initialize the number of ROM and RAM Banks
    MMU->NumROMBanks = ROMSize / 0x4000;
    MMU->NumRAMBanks = RAMSize / 0x2000;
    MMU->MBC = MBCType;

    MMU->CurrentROMBank = 1;
    MMU->CurrentRAMBank = 0;
    MMU->Ticks = 0;

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
        //Save the RAM data to the given file
        MMU->RAMFile[0] = 0x2E;
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
        bank = 1; //Prevent accesses to the first bank of ROM.
    }
    size_t BaseAddress = 0x4000 * bank;
    // Copy the bank data into the system memory at 0x4000-0x7FFF
    memcpy(MMU->SystemMemory + 0x4000, MMU->ROMFile + BaseAddress, 0x4000);
    // Update the current ROM bank
    MMU->CurrentROMBank = bank;
}
void MMUSwapRAMBank(MMU *MMU, int bank) {
    //Write Current RAM Bank to RAM File
    bank = bank & (MMU->NumRAMBanks-1);
    if (bank == 0) {
        bank = 1; //Prevent accesses to the first bank of ROM.
    }
    size_t CurrentBaseAddress = 0x2000 * MMU->CurrentRAMBank;
    memcpy(MMU->RAMFile + CurrentBaseAddress, MMU->SystemMemory + 0xA000, 0x2000);


    size_t BaseAddress = 0x2000 * bank;
    // Copy the bank data into the system memory at 0xA000-0xBFFF
    memcpy(MMU->SystemMemory + 0xA000, MMU->RAMFile + BaseAddress, 0x2000);
    // Update the current RAM bank
    MMU->CurrentRAMBank = bank;
}


//Read Write functions.
uint8_t MMURead(MMU *MMU, uint16_t address) { //Used to prevent CPU Screwups with reads (VRAM).
    //Check MBC Type and Read from the correct location. (This is needed for ROM Bank Functions)

    //Echo RAM
    if (address >= 0xE000 && address <= 0xFDFF) {
        return MMU->SystemMemory[address - 0x2000];
    }
    /* PPU Timing too inaccurate to implement this properly. (Maybe later)
    //If PPU Mode is 3, return 0xFF to VRAM. (Prevents CPU from doing unintended writes to VRAM.)
    if (address >= 0x8000 && address <= 0x9FFF) {
        if (MMU->SystemMemory[0xFF40] & 0x80) {
            return 0xFF;
        }
    }
    */
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
void MMUWrite(MMU *MMU, uint16_t address, uint8_t value) { //Used to prevent CPU Screwups with Writes.
    //Check MBC Type and Write to the correct location. (This is needed for ROM Bank Functions)

    //I'm not gonna bother implementing RAM enable for now, as it's not needed for the games I'm testing with.
    //If the user wants to play a game that uses RAM enable, I'll implement it then.


    if ((address >= 0x2000 & address <= 0x3FFF)) {
        if (MMU->NumROMBanks > 2) {
            MMUSwapROMBank(MMU, (value));
        }
        return;
    }
    
    else if (address >= 0x4000 & address <= 0x5FFF & (MMU->NumRAMBanks > 0)) {
        if (MMU->NumRAMBanks > 1) {
            MMUSwapRAMBank(MMU, value & 0x03);
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
        MMU->DMASource = value << 8;
        MMU->DMADestination = 0xFE00;
        MMU->DMACount = 0xA0;
    }

    /*/* PPU Timing too inaccurate to implement this properly. (Maybe later)
    //If PPU Mode is 3, don't write anything to VRAM. (Prevents CPU from doing unintended writes to VRAM.)
    if (address >= 0x8000 && address <= 0x9FFF) {
        if (MMU->SystemMemory[0xFF40] & 0x80) {
            return;
        }
    }
    */
    if (address > 0xFFFF) {
        return; //Prevent writes to invalid memory locations.
    }
    //Otherwise, just update the given address in the system memory.
    MMU->SystemMemory[address] = value;
}

//MMU Tick Function (DMA) 
void MMUTick(MMU *MMU) {
    if (MMU->DMACount > 0) {
        MMU->SystemMemory[MMU->DMADestination] = MMU->SystemMemory[MMU->DMASource];
        MMU->DMASource++;
        MMU->DMADestination++;
        MMU->DMACount--;
    }
}


//Update gamepad (Kept in this file because the CPU uses it for HALT and STOP instructions)
void DMGUpdateGamePad(MMU *MMU) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            MMUSaveFile(MMU);
            exit(0);
        }
        if (event.type == SDL_KEYDOWN) {
            for (int i = 0; i < 8; i++) {
                if (event.key.keysym.sym == MMU->GameBoyKeyMap[i]) {
                    MMU->GameBoyController[i] = 0;
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


//Debug Functions (Delete Later.)
void printFirstFewBytesOfROM(MMU *MMU, int numBytes) {
    printf("First %d bytes of ROM:\n", numBytes);
    for (int i = 0; i < numBytes && i < ROMSize; i++) {
        printf("%02X ", MMU->SystemMemory[i]);
    }
    printf("\n");
}

void printFirstFewBytesOfRAM(MMU *MMU, int numBytes) {
    printf("First %d bytes of RAM:\n", numBytes);
    for (int i = 0; i < numBytes && i < RAMSize; i++) {
        printf("%02X ", MMU->SystemMemory[0xA000 + i]);
    }
    printf("\n");
}

void MMUDumpVRAM(MMU *MMU) {
    FILE *f = fopen("vram.bin", "wb");
    fwrite(&MMU->SystemMemory[0x8000], 1, 0x2000, f);
    fclose(f);
}