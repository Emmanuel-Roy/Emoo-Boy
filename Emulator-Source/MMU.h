#ifndef MMU_H
#define MMU_H

#include <stdio.h>

typedef struct {
    /*Gameboy Memory Map
    0x0000 - 0x3FFF: 16KB ROM Bank 0
    0x4000 - 0x7FFF: 16KB ROM Bank 01-N (Cartidge can switch this bank)
    0x8000 - 0x9FFF: 8KB Video RAM (VRAM)
    0xA000 - 0xBFFF: 8KB External RAM (Cartridge can switch this bank)
    0xC000 - 0xCFFF: 4KB Work RAM Bank 0 (WRAM)
    0xD000 - 0xDFFF: 4KB Work RAM Bank 1 (Switchable bank 1–7 in CGB Mode)
    0xE000 - 0xFDFF: Echo RAM (mirror of C000–DDFF)
    0xFE00 - 0xFE9F: Object attribute memory (OAM)
    0xFEA0 - 0xFEFF: Not Usable
    0xFF00 - 0xFF7F: I/O Registers
    0xFF80 - 0xFFFE: High RAM (HRAM)
    0xFFFF: Interrupt Enable Register
    */
    uint8_t SystemMemory[0x10000];

    int CurrentROMBank;
    int CurrentRAMBank;
    
    uint8_t *ROMFile;
    uint8_t *RAMFile;
    
    uint8_t NumROMBanks;
    uint8_t NumRAMBanks;
    uint8_t MBC;
    
    int DMASource;
    int DMADestination;
    int DMACount;
    
    //Checks for the number of CPU Cycles that have passed since the last instruction
    uint8_t Ticks;
    uint8_t PrevInstruct;
    
    uint8_t RTCMode;
    uint8_t DEBUGMODE;

    // CGB (Game Boy Color) Support
    int isCGB;
    void *ppuPtr;              // Pointer to PPU struct for palette writes
    uint8_t VRAM[2][0x2000];  // 2 VRAM Banks (8KB each)
    uint8_t WRAM[8][0x1000];  // 8 WRAM Banks (4KB each)
    uint8_t VBK;               // 0xFF4F VRAM Bank Select
    uint8_t SVBK;              // 0xFF70 WRAM Bank Select
    uint8_t HDMA1, HDMA2, HDMA3, HDMA4, HDMA5; // CGB DMA Registers

    //KeyMap
    int GameBoyController[8]; //Up, Down, Left, Right, A, B, Start, Select
    int GameBoyKeyMap[8];
} MMU;

//Setup Functions
void MMUInit(MMU *MMU); //Creates space for ROM Data.
void MMUFree(MMU *MMU); //Frees the space for ROM Data.

//Load File Data Functions
void MMULoadFile(MMU *MMU); //Loads the ROM and RAM data from the given file, and also loads the first two bans of ROM and the first external RAM bank into the system memory.
void MMUSaveFile(MMU *MMU); //Saves the data in the RAMFile Pointer to the Given Save File.


//MBC Functions
void MMUSwapROMBank(MMU *MMU, int bank); //Swaps selected ROM Bank into the system memory.
void MMUSwapRAMBank(MMU *MMU, int bank); //Swaps selected RAM Bank into the system memory.

//Read and Write Functions
uint8_t MMURead(MMU *MMU, uint16_t address); //Reads a byte from the given address in the system memory.
void MMUWrite(MMU *MMU, uint16_t address, uint8_t value); //Writes a byte to the given address in the system memory.

//DMA Functions
void DMATick(MMU *MMU); //Ticks the MMU, and if a DMA transfer is in progress, it will transfer the next byte of data.

//Update Gamepad Functions (Kept in this file because the CPU uses it for HALT and STOP instructions)
void DMGUpdateGamePad(MMU *MMU);

#endif // MMU_H