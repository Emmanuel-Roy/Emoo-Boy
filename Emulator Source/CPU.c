#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "CPU.h"
#include "MMU.h"

extern int LOG;

void CPUTick(CPU *CPU, MMU *MMU) {
    //If there are still ticks, count down
    if (MMU->Ticks > 0) {
        MMU->Ticks--;
        return;
    }
	
	if (CPU->LOG == 1) {
		CPULOG(CPU, MMU);
	}
	
    //If there is an interrupt, disable halt
    uint8_t IF = MMU->SystemMemory[0xFF0F]; //IF
    uint8_t IE = MMU->SystemMemory[0xFFFF]; //IE

    uint8_t Interupt = IF & IE;
    //If there is an interupt, disable HALT
    if (Interupt != 0) {
        CPU->HALT = 0;
    }

    //Handle Interupts
    //If IME is set, handle interrupts
    if (CPU->IME == 1) {
        if (Interupt != 0) {
            //Disable IME
            CPU->IME = 0;
            //Push PC to Stack
            CPU->SP -= 2;
            MMU->SystemMemory[CPU->SP] = CPU->PC & 0x00FF;
            MMU->SystemMemory[CPU->SP + 1] = (CPU->PC >> 8) & 0x00FF;
            //Jump to Interupt
            if (Interupt & 0x01) {
                CPU->PC = 0x0040;
                MMU->SystemMemory[0xFF0F] &= ~0x01;
            } 
            else if (Interupt & 0x02) {
                CPU->PC = 0x0048;
                MMU->SystemMemory[0xFF0F] &= ~0x02;
            } 
            else if (Interupt & 0x04) {
                CPU->PC = 0x0050;
                MMU->SystemMemory[0xFF0F] &= ~0x04;
            } 
            else if (Interupt & 0x08) {
                CPU->PC = 0x0058;
                MMU->SystemMemory[0xFF0F] &= ~0x08;
            } 
            else if (Interupt & 0x10) {
                CPU->PC = 0x0060;
                MMU->SystemMemory[0xFF0F] &= ~0x10;
            }
            MMU->Ticks = 20;
            return;
        }
    }

    //If Halt, set MMU->Ticks = 4 and return
    if (CPU->HALT == 1) {
        MMU->Ticks = 4;
        return;
    }
    //Execute Instruction and return
    MMU->Ticks = CPUExecuteInstruction(CPU, MMU);
    
    //Update GamePad
    DMGUpdateGamePad(MMU);
    
    return;
}


/*The current code uses a switch statement to execute instructions. 
  This is not the most efficient way to do this. 
  However, it is one of the easiest to implement and is somewhat similar to the actual system.
  I may potentially change this to a function pointer array in the future.
  In addition, the current implementation may be optimized to require less statements, rather than evaluate every single opcode in the switch statement.
*/

uint8_t CPUExecuteInstruction(CPU *CPU, MMU *MMU) {
    uint8_t opcode = MMURead(MMU, CPU->PC);
    MMU->PrevInstruct = opcode;
    CPU->PC++;

    //Values used by some opcodes
    uint16_t address;
    uint8_t n8;
    int8_t n8s;
    //Execute Instruction
    switch (opcode) {
        case (0x00): { //NOP
            return 4;
        }
        case (0x01): { //LD BC, imm16
            CPU->RegB = MMURead(MMU, CPU->PC + 1);
            CPU->RegC = MMURead(MMU, CPU->PC);
            CPU->PC += 2;
            return 12;
        }
        case (0x02): { //LD [BC], A
            MMUWrite(MMU, (CPU->RegB << 8 | CPU->RegC), CPU->RegA);
            return 8;
        }
        case (0x03): { //INC BC
            address = (CPU->RegB << 8 | CPU->RegC);
            address++;
            CPU->RegB = address >> 8 & 0xFF;
            CPU->RegC = address & 0xFF;
            return 8;
        }
        case (0x04): { //INC B
            if ((CPU->RegB & 0xF) == 0xF) {
                CPU->RegF |= 0x20;
            }
            else {
                CPU->RegF &= ~0x20;
            }
            CPU->RegB++;
            //Flags
            if (CPU->RegB == 0) {
                CPU->RegF |= 0x80; 
            }
            else {
                CPU->RegF &= ~0x80;
            }
            CPU->RegF &= ~0x40; 

            return 4;
        }
        case (0x05): { //DEC B
            CPU->RegB--;

            if (CPU->RegB == 0) {
                CPU->RegF |= 0x80;
            }
            else {
                CPU->RegF &= ~0x80;
            }
            CPU->RegF |= 0x40; 

            if ((CPU->RegB & 0xF) == 0xF) {
                CPU->RegF |= 0x20; 
            }
            else {
                CPU->RegF &= ~0x20;
            }

            return 4;
        }
        case (0x06): { //LD B, n8
            CPU->RegB = MMURead(MMU, CPU->PC);
            CPU->PC++;
            return 8;
        }
        case (0x07): { //RLCA
            uint8_t carryFlag = (CPU->RegA & 0x80) >> 7; 
            CPU->RegA = (CPU->RegA << 1) | carryFlag;    
            CPU->RegF &= ~(0xF0);           
            CPU->RegF |= (carryFlag << 4);  
            return 4; 
        }
        case (0x08): { //LD (imm16), SP
            address = MMURead(MMU, CPU->PC + 1) << 8 | MMURead(MMU, CPU->PC);
            MMUWrite(MMU, address, CPU->SP & 0xFF);
            MMUWrite(MMU, address + 1, (CPU->SP >> 8) & 0xFF);
            CPU->PC += 2;
            return 20;
        }
        case (0x09): { //ADD HL, BC
            uint32_t result = ((CPU->RegH << 8) | CPU->RegL) + ((CPU->RegB << 8) | CPU->RegC);

            CPU->RegF &= ~(0x40); 

            if (((((CPU->RegH << 8) | CPU->RegL) & 0xFFF) + (((CPU->RegB << 8) | CPU->RegC) & 0xFFF)) > 0xFFF) {
                CPU->RegF |= 0x20;
            }
            else {
                CPU->RegF &= ~(0x20);
            }

            if (result > 0xFFFF) {
                CPU->RegF |= 0x10;
            } 
            else {
                CPU->RegF &= ~(0x10);
            }

            CPU->RegH = (result >> 8) & 0xFF;
            CPU->RegL = result & 0xFF;

            return 8;
        }
        case (0x0A): { //LD A, [BC]
            CPU->RegA = MMURead(MMU, (CPU->RegB << 8 | CPU->RegC));
            return 8;
        }
        case (0x0B): { //DEC BC
            address = (CPU->RegB << 8 | CPU->RegC);
            address--;
            CPU->RegB = address >> 8 & 0xFF;
            CPU->RegC = address & 0xFF;
            return 8;
        }
        case (0x0C): { //INC C

            if ((CPU->RegC & 0xF) == 0xF) {
                CPU->RegF |= 0x20; 
            }
            else {
                CPU->RegF &= ~0x20;
            }
            CPU->RegC++;
            //Flags
            if (CPU->RegC == 0) {
                CPU->RegF |= 0x80;
            }
            else {
                CPU->RegF &= ~0x80;
            }
            CPU->RegF &= ~0x40;         

            return 4;
        }
        case (0x0D): { //DEC C
            if ((CPU->RegC & 0xF) == 0x0) {
                CPU->RegF |= 0x20; 
            }
            else {
                CPU->RegF &= ~0x20;
            }
            CPU->RegC--;
            //Flags
            if (CPU->RegC == 0) {
                CPU->RegF |= 0x80; 
            }
            else {
                CPU->RegF &= ~0x80;
            }
            
            CPU->RegF |= 0x40;     

            return 4;
        }
        case (0x0E): { //LD C, n8
            CPU->RegC = MMURead(MMU, CPU->PC);
            CPU->PC++;
            return 8;
        }
        case (0x0F): { //RRCA
            uint8_t newCarryFlag = CPU->RegA & 0x01;
            CPU->RegA = (CPU->RegA >> 1) | (newCarryFlag << 7);
            CPU->RegF &= ~(0xF0);
            CPU->RegF |= (newCarryFlag << 4);
            return 4;
        }
        case (0x10): { //STOP
            //Stop wasn't actually used on any listed games, so it is not implemented. However it would basically just check for gamepad input and halt everything until then.
            return 4;
        }
        case (0x11): { //LD DE, imm16
            CPU->RegD = MMURead(MMU, CPU->PC + 1);
            CPU->RegE = MMURead(MMU, CPU->PC);
            CPU->PC += 2;
            return 12;
        } 
        case (0x12): { //LD [DE], A
            MMUWrite(MMU, (CPU->RegD << 8 | CPU->RegE), CPU->RegA);
            return 8;
        }
        case (0x13): { //INC DE
            address = (CPU->RegD << 8 | CPU->RegE);
            address++;
            CPU->RegD = address >> 8 & 0xFF;
            CPU->RegE = address & 0xFF;

            return 8;
        }
        case (0x14): { //INC D
            if ((CPU->RegD & 0xF) == 0xF) {
                CPU->RegF |= 0x20; 
            }
            else {
                CPU->RegF &= ~0x20;
            }
            CPU->RegD++;
            //Flags
            if (CPU->RegD == 0) {
                CPU->RegF |= 0x80; 
            }
            else {
                CPU->RegF &= ~0x80;
            }
            CPU->RegF &= ~0x40;            

            return 4;
        }
        case (0x15): { //DEC D
            if ((CPU->RegD & 0xF) == 0x0) {
                CPU->RegF |= 0x20; 
            }
            else {
                CPU->RegF &= ~0x20;
            }
            CPU->RegD--;
            //Flags
            if (CPU->RegD == 0) {
                CPU->RegF |= 0x80; 
            }
            else {
                CPU->RegF &= ~0x80;
            }
            
            CPU->RegF |= 0x40;

            return 4;
        }
        case (0x16): { //LD D, n8
            CPU->RegD = MMURead(MMU, CPU->PC);
            CPU->PC++;
            return 8;
        }
        case (0x17): { //RLA
            uint8_t oldCarryFlag = (CPU->RegF & 0x10) >> 4;
            uint8_t newCarryFlag = (CPU->RegA & 0x80) >> 7;
            CPU->RegA = (CPU->RegA << 1) | oldCarryFlag;
            CPU->RegF &= ~(0xF0);
            CPU->RegF |= (newCarryFlag << 4);
            return 4;
        }
        case (0x18): { //JR n8
            n8s = (int8_t)MMURead(MMU, CPU->PC);
            CPU->PC++;
            CPU->PC = (CPU->PC + n8s) & 0xFFFF;
            return 12;
        }
        case (0x19): { //ADD HL, DE
            uint32_t result = ((CPU->RegH << 8) | CPU->RegL) + ((CPU->RegD << 8) | CPU->RegE);

            CPU->RegF &= ~(0x40); 

            if (((((CPU->RegH << 8) | CPU->RegL) & 0xFFF) + (((CPU->RegD << 8) | CPU->RegE) & 0xFFF)) > 0xFFF) {
                CPU->RegF |= 0x20;
            }
            else {
                CPU->RegF &= ~(0x20);
            }

            if (result > 0xFFFF) {
                CPU->RegF |= 0x10;
            } 
            else {
                CPU->RegF &= ~(0x10);
            }

            CPU->RegH = (result >> 8) & 0xFF;
            CPU->RegL = result & 0xFF;

            return 8;
        }
        case (0x1A): { //LD A, [DE]
            CPU->RegA = MMURead(MMU, (CPU->RegD << 8 | CPU->RegE));
            return 8;
        }
        case (0x1B): { //DEC DE
            address = (CPU->RegD << 8 | CPU->RegE);
            address--;
            CPU->RegD = address >> 8 & 0xFF;
            CPU->RegE = address & 0xFF;
            return 8;
        }
        case (0x1C): { //INC E
            if ((CPU->RegE & 0xF) == 0xF) {
                CPU->RegF |= 0x20; 
            }
            else {
                CPU->RegF &= ~0x20;
            }
            CPU->RegE++;
            //Flags
            if (CPU->RegE == 0) {
                CPU->RegF |= 0x80;
            }
            else {
                CPU->RegF &= ~0x80;
            }
            CPU->RegF &= ~0x40;         

            return 4;
        }
        case (0x1D): { //DEC E
            if ((CPU->RegE & 0xF) == 0x0) {
                CPU->RegF |= 0x20; 
            }
            else {
                CPU->RegF &= ~0x20;
            }

            CPU->RegE--;

            if (CPU->RegE == 0) {
                CPU->RegF |= 0x80;
            }
            else {
                CPU->RegF &= ~0x80;
            }
            
            CPU->RegF |= 0x40; 

            return 4;
        }
        case (0x1E): { //LD E, n8
            CPU->RegE = MMURead(MMU, CPU->PC);
            CPU->PC++;
            return 8;
        }
        case (0x1F): { //RRA
            n8 = CPU->RegF & 0x10;
            CPU->RegF = (CPU->RegA & 0x01) << 4; 
            CPU->RegA = (CPU->RegA >> 1) | (n8 << 3);
            return 4;
        }
        case (0x20): { //JR NZ, n8
            if (!(CPU->RegF & 0x80)) {
                n8s = (int8_t)MMURead(MMU, CPU->PC);
                CPU->PC++;
                CPU->PC = (CPU->PC + n8s) & 0xFFFF;
                return 12;
            }
            CPU->PC++;
            return 8;
        }
        case (0x21): { //LD HL, imm16
            CPU->RegH = MMURead(MMU, CPU->PC + 1);
            CPU->RegL = MMURead(MMU, CPU->PC);            
            CPU->PC += 2;
            return 12;
        } 
        case (0x22): { //LD [HL+], A
            address = (CPU->RegH << 8 | CPU->RegL);
            MMUWrite(MMU, address, CPU->RegA);
            address = (address + 1) & 0xFFFF;
            CPU->RegH = address >> 8 & 0xFF;
            CPU->RegL = address & 0xFF;
            return 8;

        }
        case (0x23): { //INC HL
            address = (CPU->RegH << 8 | CPU->RegL);
            address++;
            CPU->RegH = address >> 8 & 0xFF;
            CPU->RegL = address & 0xFF;
            return 8;
        }
        case (0x24): { //INC H
            if ((CPU->RegH & 0xF) == 0xF) {
                CPU->RegF |= 0x20; 
            }
            else {
                CPU->RegF &= ~0x20;
            }
            CPU->RegH++;
            //Flags
            if (CPU->RegH == 0) {
                CPU->RegF |= 0x80; 
            }
            else {
                CPU->RegF &= ~0x80;
            }
            CPU->RegF &= ~0x40;            

            return 4;
        }
        case (0x25): { //DEC H
            if ((CPU->RegH & 0xF) == 0x0) {
                CPU->RegF |= 0x20; 
            }
            else {
                CPU->RegF &= ~0x20;
            }

            CPU->RegH--;

            if (CPU->RegH == 0) {
                CPU->RegF |= 0x80; 
            }
            else {
                CPU->RegF &= ~0x80;
            }
            
            CPU->RegF |= 0x40; 
            return 4;
        }
        case (0x26): { //LD H, n8
            CPU->RegH = MMURead(MMU, CPU->PC);
            CPU->PC++;
            return 8;
        }
        case (0x27): { //DAA
            int n = CPU->RegA;

            if (CPU->RegF & 0x40) {
                if (CPU->RegF & 0x20) {
                    n = (n - 6) & 0xFF;
                }
                if (CPU->RegF & 0x10) {
                    n -= 0x60;
                }
            }
            else {
                if ((CPU->RegF & 0x20) || ((n & 0xF) > 9)) {
                    n += 0x06;
                }
                if (CPU->RegF & 0x10 || (n > 0x9F)) {
                    n += 0x60;

                }
            }

            CPU->RegA = n;
            
            CPU->RegF &= ~0x20; 
            
            if (CPU->RegA == 0) { 
                CPU->RegF |= 0x80;
            }
            else {
                CPU->RegF &= ~0x80;
            }

            if (n >= 0x100) {
                CPU->RegF |= 0x10;
            }

            return 4;
        } 
        case (0x28): { //JR Z, n8
            if (CPU->RegF & 0x80) {
                n8s = (int8_t)MMURead(MMU, CPU->PC);
                CPU->PC++;
                CPU->PC = (CPU->PC + n8s) & 0xFFFF;
                return 12;
            }
            CPU->PC++;
            return 8;
        }
        case (0x29): { //ADD HL, HL
            uint32_t result = ((CPU->RegH << 8) | CPU->RegL) + ((CPU->RegH << 8) | CPU->RegL);

            if (((((CPU->RegH << 8) | CPU->RegL) & 0xFFF) + (((CPU->RegH << 8) | CPU->RegL) & 0xFFF)) > 0xFFF) {
                CPU->RegF |= 0x20;
            }
            else {
                CPU->RegF &= ~(0x20);
            }

            if (result > 0xFFFF) {
                CPU->RegF |= 0x10;
            } 
            else {
                CPU->RegF &= ~(0x10);
            }

            CPU->RegH = (result >> 8) & 0xFF;
            CPU->RegL = result & 0xFF;

            CPU->RegF &= ~(0x40);

            return 8;
        }
        case (0x2A): { //LD A, [HL+]
            address = (CPU->RegH << 8 | CPU->RegL);
            CPU->RegA = MMURead(MMU, address);
            address = (address + 1);
            CPU->RegH = address >> 8 & 0xFF;
            CPU->RegL = address & 0xFF;
            return 8;
        }
        case (0x2B): { //DEC HL
            address = (CPU->RegH << 8 | CPU->RegL);
            address--;
            CPU->RegH = address >> 8 & 0xFF;
            CPU->RegL = address & 0xFF;
            return 8;
        }
        case (0x2C): { //INC L
            if ((CPU->RegL & 0xF) == 0xF) {
                CPU->RegF |= 0x20; 
            }
            else {
                CPU->RegF &= ~0x20;
            }
            CPU->RegL++;
            //Flags
            if (CPU->RegL == 0) {
                CPU->RegF |= 0x80; 
            }
            else {
                CPU->RegF &= ~0x80;
            }
            CPU->RegF &= ~0x40;           

            return 4;
        }
        case (0x2D): { //DEC L
            if ((CPU->RegL & 0xF) == 0x0) {
                CPU->RegF |= 0x20; 
            }
            else {
                CPU->RegF &= ~0x20;
            }
            CPU->RegL--;
            //Flags
            if (CPU->RegL == 0) {
                CPU->RegF |= 0x80; 
            }
            else {
                CPU->RegF &= ~0x80;
            }
            
            CPU->RegF |= 0x40;    

            return 4;
        }
        case (0x2E): { //LD L, n8
            CPU->RegL = MMURead(MMU, CPU->PC);
            CPU->PC++;
            return 8;
        }
        case (0x2F): { //CPL
            CPU->RegA = ~CPU->RegA;
            CPU->RegF |= 0x60; //Set Sub Flag and Half Carry Flag
            return 4;
        }
        case (0x30): { //JR NC, n8
            n8s = (int8_t)MMURead(MMU, CPU->PC);
            CPU->PC++;
            if (!(CPU->RegF & 0x10)) {
                CPU->PC = (CPU->PC + n8s) & 0xFFFF;
                return 12;
            }
            return 8;
        }
        case (0x31): { //LD SP, imm16
            CPU->SP = MMURead(MMU, CPU->PC + 1) << 8 | MMURead(MMU, CPU->PC);
            CPU->PC += 2;
            return 12;
        }
        case (0x32): { //LD [HL-], A
            address = (CPU->RegH << 8 | CPU->RegL);
            MMUWrite(MMU, address, CPU->RegA);
            address = (address - 1) & 0xFFFF;
            CPU->RegH = address >> 8 & 0xFF;
            CPU->RegL = address & 0xFF;
            return 8;
        }
        case (0x33): { //INC SP
            CPU->SP++;
            return 8;
        }
        case (0x34): { //INC (HL)
            address = (CPU->RegH << 8 | CPU->RegL);
            n8 = MMURead(MMU, address);
            if ((n8 & 0xF) == 0xF) {
                CPU->RegF |= 0x20; 
            }
            else {
                CPU->RegF &= ~0x20;
            }
            n8++;
            if (n8 == 0) {
                CPU->RegF |= 0x80; 
            }
            else {
                CPU->RegF &= ~0x80;
            }

            CPU->RegF &= ~0x40; 

            MMUWrite(MMU, address, n8);
            return 12;
        }
        case (0x35): { //DEC (HL)
            address = (CPU->RegH << 8 | CPU->RegL);
            n8 = MMURead(MMU, address);
            if ((n8 & 0xF) == 0x0) {
                CPU->RegF |= 0x20;
            }
            else {
                CPU->RegF &= ~0x20;
            }
            n8--;
            if (n8 == 0) {
                CPU->RegF |= 0x80;
            }
            else {
                CPU->RegF &= ~0x80;
            }
            CPU->RegF |= 0x40; 
            MMUWrite(MMU, address, n8);
            return 12;
        }
        case (0x36): { //LD (HL), n8
            MMUWrite(MMU, (CPU->RegH << 8 | CPU->RegL), MMURead(MMU, CPU->PC));
            CPU->PC++;
            return 12;
        }
        case (0x37): { //SCF
            //Clear and set flags
            CPU->RegF &= ~(0x40); 
            CPU->RegF &= ~(0x20); 
            CPU->RegF |= 0x10; 
            return 4;
        }
        case (0x38): { // JR C, n8
            n8s = (int8_t)MMURead(MMU, CPU->PC);
            CPU->PC++;
            if ((CPU->RegF & 0x10)) {
                CPU->PC = (CPU->PC + n8s) & 0xFFFF;
                return 12;
            }
            return 8;
        }
        case (0x39): { //ADD HL, SP
            uint16_t HL = (CPU->RegH << 8) | CPU->RegL;
            uint32_t result = HL + CPU->SP;
            if (((HL & 0x0FFF) + (CPU->SP & 0x0FFF)) > 0x0FFF) {
                CPU->RegF |= 0x20;
            } 
            else {
                CPU->RegF &= ~(0x20);
            }
            if (result > 0xFFFF) {
                CPU->RegF |= 0x10;
            }    
            else {
                CPU->RegF &= ~(0x10);
            }
            HL = result & 0xFFFF;
            CPU->RegH = (HL >> 8) & 0xFF;
            CPU->RegL = HL & 0xFF;
            CPU->RegF &= ~(0x40);

            return 8;
        }
        case (0x3A): { //LD A, [HL-]
            address = (CPU->RegH << 8 | CPU->RegL);
            CPU->RegA = MMURead(MMU, address);
            address = (address - 1) & 0xFFFF;
            CPU->RegH = address >> 8 & 0xFF;
            CPU->RegL = address & 0xFF;
            return 8;
        }
        case (0x3B): { //DEC SP
            CPU->SP--;
            return 8;
        }
        case (0x3C): { //INC A
            CPU->RegA++;
            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80; 
            }
            else {
                CPU->RegF &= ~0x80;
            }
            CPU->RegF &= ~0x40; 
            if ((CPU->RegA & 0x0F) == 0) {
                CPU->RegF |= 0x20; 
            }
            else {
                CPU->RegF &= ~0x20;
            }

            return 4;
        }
        case (0x3D): { //DEC A
            CPU->RegA--;
            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80; 
            }
            else {
                CPU->RegF &= ~0x80;
            }
            CPU->RegF |= 0x40; 
            if ((CPU->RegA & 0x0F) == 0x0F) {
                CPU->RegF |= 0x20; 
            }
            else {
                CPU->RegF &= ~0x20;
            }

            return 4;
        }
        case (0x3E): { //LD a, n8
            CPU->RegA = MMURead(MMU, CPU->PC);
            CPU->PC++;
            return 8;
        }
        case (0x3F): { //CCF
            //Clear and flip flags
            CPU->RegF &= ~(0x40); 
            CPU->RegF &= ~(0x20); 
            CPU->RegF ^= 0x10; 
            return 4;
        }
        case (0x40): { //LD B, B
            return 4;
        }
        case (0x41): { //LD B, C
            CPU->RegB = CPU->RegC;
            return 4;
        }
        case (0x42): { //LD B, D
            CPU->RegB = CPU->RegD;
            return 4;
        }
        case (0x43): { //LD B, E
            CPU->RegB = CPU->RegE;
            return 4;
        }
        case (0x44): { //LD B, H
            CPU->RegB = CPU->RegH;
            return 4;
        }
        case (0x45): { //LD B, L
            CPU->RegB = CPU->RegL;
            return 4;
        }
        case (0x46): { //LD B, (HL)
            CPU->RegB = MMURead(MMU, (CPU->RegH << 8 | CPU->RegL));
            return 8;
        }
        case (0x47): { //LD B, A
            CPU->RegB = CPU->RegA;
            return 4;
        }
        case (0x48): { //LD C, B
            CPU->RegC = CPU->RegB;
            return 4;
        }
        case (0x49): { //LD C, C
            return 4;
        }
        case (0x4A): { //LD C, D
            CPU->RegC = CPU->RegD;
            return 4;
        }
        case (0x4B): { //LD C, E
            CPU->RegC = CPU->RegE;
            return 4;
        }
        case (0x4C): { //LD C, H
            CPU->RegC = CPU->RegH;
            return 4;
        }
        case (0x4D): { //LD C, L
            CPU->RegC = CPU->RegL;
            return 4;
        }
        case (0x4E): { //LD C, (HL)
            CPU->RegC = MMURead(MMU, (CPU->RegH << 8 | CPU->RegL));
            return 8;
        }
        case (0x4F): { //LD C, A
            CPU->RegC = CPU->RegA;
            return 4;
        }
        case (0x50): { //LD D, B
            CPU->RegD = CPU->RegB;
            return 4;
        }
        case (0x51): { //LD D, C
            CPU->RegD = CPU->RegC;
            return 4;
        }
        case (0x52): { //LD D, D
            CPU->RegD = CPU->RegD;
            return 4;
        }
        case (0x53): { //LD D, E
            CPU->RegD = CPU->RegE;
            return 4;
        }
        case (0x54): { //LD D, H
            CPU->RegD = CPU->RegH;
            return 4;
        }
        case (0x55): { //LD D, L
            CPU->RegD = CPU->RegL;
            return 4;
        }
        case (0x56): { //LD D, (HL)
            CPU->RegD = MMURead(MMU, (CPU->RegH << 8 | CPU->RegL));
            return 8;
        }
        case (0x57): { //LD D, A
            CPU->RegD = CPU->RegA;
            return 4;
        }
        case (0x58): { //LD E, B
            CPU->RegE = CPU->RegB;
            return 4;
        }
        case (0x59): { //LD E, C
            CPU->RegE = CPU->RegC;
            return 4;
        }
        case (0x5A): { //LD E, D
            CPU->RegE = CPU->RegD;
            return 4;
        }
        case (0x5B): { //LD E, E
            CPU->RegE = CPU->RegE;
            return 4;
        }
        case (0x5C): { //LD E, H
            CPU->RegE = CPU->RegH;
            return 4;
        }
        case (0x5D): { //LD E, L
            CPU->RegE = CPU->RegL;
            return 4;
        }
        case (0x5E): { //LD E, (HL)
            CPU->RegE = MMURead(MMU, (CPU->RegH << 8 | CPU->RegL));
            return 8;
        }
        case (0x5F): { //LD E, A
            CPU->RegE = CPU->RegA;
            return 4;
        }
        case (0x60): { //LD H, B
            CPU->RegH = CPU->RegB;
            return 4;
        }
        case (0x61): { //LD H, C
            CPU->RegH = CPU->RegC;
            return 4;
        }
        case (0x62): { //LD H, D
            CPU->RegH = CPU->RegD;
            return 4;
        }
        case (0x63): { //LD H, E
            CPU->RegH = CPU->RegE;
            return 4;
        }
        case (0x64): { //LD H, H
            CPU->RegH = CPU->RegH;
            return 4;
        }
        case (0x65): { //LD H, L
            CPU->RegH = CPU->RegL;
            return 4;
        }
        case (0x66): { //LD H, (HL)
            CPU->RegH = MMURead(MMU, (CPU->RegH << 8 | CPU->RegL));
            return 8;
        }
        case (0x67): { //LD H, A
            CPU->RegH = CPU->RegA;
            return 4;
        }
        case (0x68): { //LD L, B
            CPU->RegL = CPU->RegB;
            return 4;
        }
        case (0x69): { //LD L, C
            CPU->RegL = CPU->RegC;
            return 4;
        }
        case (0x6A): { //LD L, D
            CPU->RegL = CPU->RegD;
            return 4;
        }
        case (0x6B): { //LD L, E
            CPU->RegL = CPU->RegE;
            return 4;
        }
        case (0x6C): { //LD L, H
            CPU->RegL = CPU->RegH;
            return 4;
        }
        case (0x6D): { //LD L, L
            CPU->RegL = CPU->RegL;
            return 4;
        }
        case (0x6E): { //LD L, (HL)
            CPU->RegL = MMURead(MMU, (CPU->RegH << 8 | CPU->RegL));
            return 8;
        }
        case (0x6F): { //LD L, A
            CPU->RegL = CPU->RegA;
            return 4;
        }
        case (0x70): { //LD (HL), B
            MMUWrite(MMU, (CPU->RegH << 8 | CPU->RegL), CPU->RegB);
            return 8;
        }
        case (0x71): { //LD (HL), C
            MMUWrite(MMU, (CPU->RegH << 8 | CPU->RegL), CPU->RegC);
            return 8;
        }
        case (0x72): { //LD (HL), D
            MMUWrite(MMU, (CPU->RegH << 8 | CPU->RegL), CPU->RegD);
            return 8;
        }
        case (0x73): { //LD (HL), E
            MMUWrite(MMU, (CPU->RegH << 8 | CPU->RegL), CPU->RegE);
            return 8;
        }
        case (0x74): { //LD (HL), H
            MMUWrite(MMU, (CPU->RegH << 8 | CPU->RegL), CPU->RegH);
            return 8;
        }
        case (0x75): { //LD (HL), L
            MMUWrite(MMU, (CPU->RegH << 8 | CPU->RegL), CPU->RegL);
            return 8;
        }
        case (0x76): { //HALT
            CPU->HALT = 1;
            return 4;
        }
        case (0x77): { //LD (HL), A
            MMUWrite(MMU, (CPU->RegH << 8 | CPU->RegL), CPU->RegA);
            return 8;
        }
        case (0x78): { //LD A, B
            CPU->RegA = CPU->RegB;
            return 4;
        }
        case (0x79): { //LD A, C
            CPU->RegA = CPU->RegC;
            return 4;
        }
        case (0x7A): { //LD A, D
            CPU->RegA = CPU->RegD;
            return 4;
        }
        case (0x7B): { //LD A, E
            CPU->RegA = CPU->RegE;
            return 4;
        }
        case (0x7C): { //LD A, H
            CPU->RegA = CPU->RegH;
            return 4;
        }
        case (0x7D): { //LD A, L
            CPU->RegA = CPU->RegL;
            return 4;
        }
        case (0x7E): { //LD A, (HL)
            CPU->RegA = MMURead(MMU, (CPU->RegH << 8 | CPU->RegL));
            return 8;
        }
        case (0x7F): { //LD A, A
            return 4;
        }
        case (0x80): { //ADD A, B
            uint16_t result = CPU->RegA + CPU->RegB;
            //Flags
            if ((CPU->RegA & 0xF) + (CPU->RegB & 0xF) > 0xF) {
                CPU->RegF |= 0x20; 
            }
            else {
                CPU->RegF &= ~0x20; 
            }
            if (result > 0xFF) {
                CPU->RegF |= 0x10; 
            }
            else {
                CPU->RegF &= ~0x10; 
            }
            CPU->RegA = result & 0xFF;
            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80; 
            }
            else {
                CPU->RegF &= ~0x80; 
            }
            CPU->RegF &= ~0x40; 

            return 4;
        }
        case (0x81): { //ADD A, C
            uint16_t result = CPU->RegA + CPU->RegC;
            //Flags
            if ((CPU->RegA & 0xF) + (CPU->RegC & 0xF) > 0xF) {
                CPU->RegF |= 0x20; 
            }
            else {
                CPU->RegF &= ~0x20; 
            }
            if (result > 0xFF) {
                CPU->RegF |= 0x10; 
            }
            else {
                CPU->RegF &= ~0x10; 
            }
            CPU->RegA = result & 0xFF;
            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80; 
            }
            else {
                CPU->RegF &= ~0x80; 
            }
            CPU->RegF &= ~0x40; 

            return 4;
        }
        case (0x82): { //ADD A, D
            uint16_t result = CPU->RegA + CPU->RegD;
            //Flags
            if ((CPU->RegA & 0xF) + (CPU->RegD & 0xF) > 0xF) {
                CPU->RegF |= 0x20; 
            }
            else {
                CPU->RegF &= ~0x20; 
            }
            if (result > 0xFF) {
                CPU->RegF |= 0x10; 
            }
            else {
                CPU->RegF &= ~0x10; 
            }
            CPU->RegA = result & 0xFF;
            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80; 
            }
            else {
                CPU->RegF &= ~0x80; 
            }
            CPU->RegF &= ~0x40; 

            return 4;
        }
        case (0x83): { //ADD A, E
            uint16_t result = CPU->RegA + CPU->RegE;
            //Flags
            if ((CPU->RegA & 0xF) + (CPU->RegE & 0xF) > 0xF) {
                CPU->RegF |= 0x20; 
            }
            else {
                CPU->RegF &= ~0x20; 
            }
            if (result > 0xFF) {
                CPU->RegF |= 0x10; 
            }
            else {
                CPU->RegF &= ~0x10; 
            }
            CPU->RegA = result & 0xFF;
            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80; 
            }
            else {
                CPU->RegF &= ~0x80;
            }
            CPU->RegF &= ~0x40; 

            return 4;
        }
        case (0x84): { //ADD A, H
            uint16_t result = CPU->RegA + CPU->RegH;
            //Flags
            if ((CPU->RegA & 0xF) + (CPU->RegH & 0xF) > 0xF) {
                CPU->RegF |= 0x20; 
            }
            else {
                CPU->RegF &= ~0x20; 
            }
            if (result > 0xFF) {
                CPU->RegF |= 0x10; 
            }
            else {
                CPU->RegF &= ~0x10; 
            }
            CPU->RegA = result & 0xFF;
            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80; 
            }
            else {
                CPU->RegF &= ~0x80; 
            }
            CPU->RegF &= ~0x40; 

            return 4;
        }
        case (0x85): { //ADD A, L
            uint16_t result = CPU->RegA + CPU->RegL;
            //Flags
            if ((CPU->RegA & 0xF) + (CPU->RegL & 0xF) > 0xF) {
                CPU->RegF |= 0x20; 
            }
            else {
                CPU->RegF &= ~0x20; 
            }
            if (result > 0xFF) {
                CPU->RegF |= 0x10; 
            }
            else {
                CPU->RegF &= ~0x10; 
            }
            CPU->RegA = result & 0xFF;
            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80; 
            }
            else {
                CPU->RegF &= ~0x80; 
            }
            CPU->RegF &= ~0x40; 

            return 4;
        }
        case (0x86): { //ADD A, (HL)
            uint16_t result = CPU->RegA + MMURead(MMU, (CPU->RegH << 8 | CPU->RegL));
            //Flags
            if ((CPU->RegA & 0xF) + (MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) & 0xF) > 0xF) {
                CPU->RegF |= 0x20;
            }
            else {
                CPU->RegF &= ~0x20;
            }
            if (result > 0xFF) {
                CPU->RegF |= 0x10; 
            }
            else {
                CPU->RegF &= ~0x10; 
            }
            CPU->RegA = result & 0xFF;
            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80; 
            }
            else {
                CPU->RegF &= ~0x80; 
            }
            CPU->RegF &= ~0x40; 

            return 8;
        }
        case (0x87): { //ADD A, A
            uint16_t result = CPU->RegA + CPU->RegA;

            if ((CPU->RegA & 0xF) + (CPU->RegA & 0xF) > 0xF) {
                CPU->RegF |= 0x20; //Set Half Carry Flag
            }
            else {
                CPU->RegF &= ~0x20; //Clear Half Carry Flag
            }

            if (result > 0xFF) {
                CPU->RegF |= 0x10; //Set Carry Flag
            }
            else {
                CPU->RegF &= ~0x10; //Clear Carry Flag
            }

            CPU->RegA = result & 0xFF;

            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            else {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }

            CPU->RegF &= ~0x40; //Clear Sub Flag

            return 4;
        }
        case (0x88): { //ADC A, B
            uint16_t result = CPU->RegA + CPU->RegB + ((CPU->RegF & 0x10) ? 1 : 0);

            //N Flag
            CPU->RegF &= ~0x40;

            //H Flag
            if (((CPU->RegA & 0xF) + (CPU->RegB & 0xF) + ((CPU->RegF & 0x10) ? 1 : 0)) >= 0x10) {
                CPU->RegF |= 0x20;
            }
            else {
                CPU->RegF &= ~0x20;
            }

            CPU->RegA = result & 0xFF;
            
            //C Flag
            if (result > 0xFF) {
                CPU->RegF |= 0x10;
            }
            else {
                CPU->RegF &= ~0x10;
            }

            //Z Flag
            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80;
            }
            else {
                CPU->RegF &= ~0x80;
            }

            return 8;
        }
        case (0x89): { //ADC A, C
            uint16_t result = CPU->RegA + CPU->RegC + ((CPU->RegF & 0x10) ? 1 : 0);

            //N Flag
            CPU->RegF &= ~0x40;

            //H Flag
            if (((CPU->RegA & 0xF) + (CPU->RegC & 0xF) + ((CPU->RegF & 0x10) ? 1 : 0)) >= 0x10) {
                CPU->RegF |= 0x20;
            }
            else {
                CPU->RegF &= ~0x20;
            }

            CPU->RegA = result & 0xFF;
            
            //C Flag
            if (result > 0xFF) {
                CPU->RegF |= 0x10;
            }
            else {
                CPU->RegF &= ~0x10;
            }

            //Z Flag
            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80;
            }
            else {
                CPU->RegF &= ~0x80;
            }

            return 8;
        }
        case (0x8A): { //ADC A, D
            uint16_t result = CPU->RegA + CPU->RegD + ((CPU->RegF & 0x10) ? 1 : 0);

            //N Flag
            CPU->RegF &= ~0x40;

            //H Flag
            if (((CPU->RegA & 0xF) + (CPU->RegD & 0xF) + ((CPU->RegF & 0x10) ? 1 : 0)) >= 0x10) {
                CPU->RegF |= 0x20;
            }
            else {
                CPU->RegF &= ~0x20;
            }

            CPU->RegA = result & 0xFF;
            
            //C Flag
            if (result > 0xFF) {
                CPU->RegF |= 0x10;
            }
            else {
                CPU->RegF &= ~0x10;
            }

            //Z Flag
            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80;
            }
            else {
                CPU->RegF &= ~0x80;
            }

            return 8;
        }
        case (0x8B): { //ADC A, E
            uint16_t result = CPU->RegA + CPU->RegE + ((CPU->RegF & 0x10) ? 1 : 0);

            //N Flag
            CPU->RegF &= ~0x40;

            //H Flag
            if (((CPU->RegA & 0xF) + (CPU->RegE & 0xF) + ((CPU->RegF & 0x10) ? 1 : 0)) >= 0x10) {
                CPU->RegF |= 0x20;
            }
            else {
                CPU->RegF &= ~0x20;
            }

            CPU->RegA = result & 0xFF;
            
            //C Flag
            if (result > 0xFF) {
                CPU->RegF |= 0x10;
            }
            else {
                CPU->RegF &= ~0x10;
            }

            //Z Flag
            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80;
            }
            else {
                CPU->RegF &= ~0x80;
            }

            return 8;
        }
        case (0x8C): { //ADC A, H
            uint16_t result = CPU->RegA + CPU->RegH + ((CPU->RegF & 0x10) ? 1 : 0);

            //N Flag
            CPU->RegF &= ~0x40;

            //H Flag
            if (((CPU->RegA & 0xF) + (CPU->RegH & 0xF) + ((CPU->RegF & 0x10) ? 1 : 0)) >= 0x10) {
                CPU->RegF |= 0x20;
            }
            else {
                CPU->RegF &= ~0x20;
            }

            CPU->RegA = result & 0xFF;
            
            //C Flag
            if (result > 0xFF) {
                CPU->RegF |= 0x10;
            }
            else {
                CPU->RegF &= ~0x10;
            }

            //Z Flag
            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80;
            }
            else {
                CPU->RegF &= ~0x80;
            }

            return 8;
        }
        case (0x8D): { //ADC A, L
            uint16_t result = CPU->RegA + CPU->RegL + ((CPU->RegF & 0x10) ? 1 : 0);

            //N Flag
            CPU->RegF &= ~0x40;

            //H Flag
            if (((CPU->RegA & 0xF) + (CPU->RegL & 0xF) + ((CPU->RegF & 0x10) ? 1 : 0)) >= 0x10) {
                CPU->RegF |= 0x20;
            }
            else {
                CPU->RegF &= ~0x20;
            }

            CPU->RegA = result & 0xFF;
            
            //C Flag
            if (result > 0xFF) {
                CPU->RegF |= 0x10;
            }
            else {
                CPU->RegF &= ~0x10;
            }

            //Z Flag
            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80;
            }
            else {
                CPU->RegF &= ~0x80;
            }

            return 8;
        }
        case (0x8E): { //ADC A, (HL)
            uint16_t result = CPU->RegA + MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) + ((CPU->RegF & 0x10) ? 1 : 0);

            //N Flag
            CPU->RegF &= ~0x40;

            //H Flag
            if (((CPU->RegA & 0xF) + (MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) & 0xF) + ((CPU->RegF & 0x10) ? 1 : 0)) >= 0x10) {
                CPU->RegF |= 0x20;
            }
            else {
                CPU->RegF &= ~0x20;
            }

            CPU->RegA = result & 0xFF;
            
            //C Flag
            if (result > 0xFF) {
                CPU->RegF |= 0x10;
            }
            else {
                CPU->RegF &= ~0x10;
            }

            //Z Flag
            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80;
            }
            else {
                CPU->RegF &= ~0x80;
            }

            return 8;
        }
        case (0x8F): { //ADC A, A
            uint16_t result = CPU->RegA + CPU->RegA + ((CPU->RegF & 0x10) ? 1 : 0);

            //N Flag
            CPU->RegF &= ~0x40;

            //H Flag
            if (((CPU->RegA & 0xF) + (CPU->RegA & 0xF) + ((CPU->RegF & 0x10) ? 1 : 0)) >= 0x10) {
                CPU->RegF |= 0x20;
            }
            else {
                CPU->RegF &= ~0x20;
            }

            CPU->RegA = result & 0xFF;
            
            //C Flag
            if (result > 0xFF) {
                CPU->RegF |= 0x10;
            }
            else {
                CPU->RegF &= ~0x10;
            }

            //Z Flag
            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80;
            }
            else {
                CPU->RegF &= ~0x80;
            }

            return 4;
        }
        case (0x90): { //SUB B
            uint16_t result = CPU->RegA - CPU->RegB;

            if ((CPU->RegA & 0xF) < (CPU->RegB & 0xF)) {
                CPU->RegF |= 0x20; //Set Half Carry Flag
            }
            else {
                CPU->RegF &= ~0x20; //Clear Half Carry Flag
            }

            if (result > 0xFF) {
                CPU->RegF |= 0x10; //Set Carry Flag
            }
            else {
                CPU->RegF &= ~0x10; //Clear Carry Flag
            }

            CPU->RegA = result & 0xFF;

            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            else {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }

            CPU->RegF |= 0x40; //Set Sub Flag

            return 4;
        }
        case (0x91): { //SUB C
            uint16_t result = CPU->RegA - CPU->RegC;

            if ((CPU->RegA & 0xF) < (CPU->RegC & 0xF)) {
                CPU->RegF |= 0x20; //Set Half Carry Flag
            }
            else {
                CPU->RegF &= ~0x20; //Clear Half Carry Flag
            }

            if (result > 0xFF) {
                CPU->RegF |= 0x10; //Set Carry Flag
            }
            else {
                CPU->RegF &= ~0x10; //Clear Carry Flag
            }

            CPU->RegA = result & 0xFF;

            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            else {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }

            CPU->RegF |= 0x40; //Set Sub Flag

            return 4;
        }
        case (0x92): { //SUB D
            uint16_t result = CPU->RegA - CPU->RegD;

            if ((CPU->RegA & 0xF) < (CPU->RegD & 0xF)) {
                CPU->RegF |= 0x20; //Set Half Carry Flag
            }
            else {
                CPU->RegF &= ~0x20; //Clear Half Carry Flag
            }

            if (result > 0xFF) {
                CPU->RegF |= 0x10; //Set Carry Flag
            }
            else {
                CPU->RegF &= ~0x10; //Clear Carry Flag
            }

            CPU->RegA = result & 0xFF;

            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            else {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }

            CPU->RegF |= 0x40; //Set Sub Flag

            return 4;
        }
        case (0x93): { //SUB E
            uint16_t result = CPU->RegA - CPU->RegE;

            if ((CPU->RegA & 0xF) < (CPU->RegE & 0xF)) {
                CPU->RegF |= 0x20; //Set Half Carry Flag
            }
            else {
                CPU->RegF &= ~0x20; //Clear Half Carry Flag
            }

            if (result > 0xFF) {
                CPU->RegF |= 0x10; //Set Carry Flag
            }
            else {
                CPU->RegF &= ~0x10; //Clear Carry Flag
            }

            CPU->RegA = result & 0xFF;

            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            else {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }

            CPU->RegF |= 0x40; //Set Sub Flag

            return 4;
        }
        case (0x94): { //SUB H
            uint16_t result = CPU->RegA - CPU->RegH;

            if ((CPU->RegA & 0xF) < (CPU->RegH & 0xF)) {
                CPU->RegF |= 0x20; //Set Half Carry Flag
            }
            else {
                CPU->RegF &= ~0x20; //Clear Half Carry Flag
            }

            if (result > 0xFF) {
                CPU->RegF |= 0x10; //Set Carry Flag
            }
            else {
                CPU->RegF &= ~0x10; //Clear Carry Flag
            }

            CPU->RegA = result & 0xFF;

            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            else {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }

            CPU->RegF |= 0x40; //Set Sub Flag

            return 4;
        }
        case (0x95): { //SUB L
            uint16_t result = CPU->RegA - CPU->RegL;

            if ((CPU->RegA & 0xF) < (CPU->RegL & 0xF)) {
                CPU->RegF |= 0x20; //Set Half Carry Flag
            }
            else {
                CPU->RegF &= ~0x20; //Clear Half Carry Flag
            }

            if (result > 0xFF) {
                CPU->RegF |= 0x10; //Set Carry Flag
            }
            else {
                CPU->RegF &= ~0x10; //Clear Carry Flag
            }

            CPU->RegA = result & 0xFF;

            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            else {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }

            CPU->RegF |= 0x40; //Set Sub Flag

            return 4;
        }
        case (0x96): { //SUB (HL)
            uint16_t result = CPU->RegA - MMURead(MMU, (CPU->RegH << 8 | CPU->RegL));

            if ((CPU->RegA & 0xF) < (MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) & 0xF)) {
                CPU->RegF |= 0x20; //Set Half Carry Flag
            }
            else {
                CPU->RegF &= ~0x20; //Clear Half Carry Flag
            }

            if (result > 0xFF) {
                CPU->RegF |= 0x10; //Set Carry Flag
            }
            else {
                CPU->RegF &= ~0x10; //Clear Carry Flag
            }

            CPU->RegA = result & 0xFF;

            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            else {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }

            CPU->RegF |= 0x40; //Set Sub Flag

            return 8;
        }
        case (0x97): { //SUB A
            uint16_t result = CPU->RegA - CPU->RegA;

            if ((CPU->RegA & 0xF) < (CPU->RegA & 0xF)) {
                CPU->RegF |= 0x20; //Set Half Carry Flag
            }
            else {
                CPU->RegF &= ~0x20; //Clear Half Carry Flag
            }

            if (result > 0xFF) {
                CPU->RegF |= 0x10; //Set Carry Flag
            }
            else {
                CPU->RegF &= ~0x10; //Clear Carry Flag
            }

            CPU->RegA = result & 0xFF;

            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            else {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }

            CPU->RegF |= 0x40; //Set Sub Flag

            return 4;
        }
        case (0x98): { //SBC A, B
            uint16_t result = CPU->RegA - CPU->RegB - ((CPU->RegF & 0x10) ? 1 : 0);

            //N Flag
            CPU->RegF |= 0x40;

            //H Flag
            if ((CPU->RegA & 0xF) < (CPU->RegB & 0xF) + ((CPU->RegF & 0x10) ? 1 : 0)) {
                CPU->RegF |= 0x20;
            }
            else {
                CPU->RegF &= ~0x20;
            }

            CPU->RegA = result & 0xFF;
            
            //C Flag
            if (result > 0xFF) {
                CPU->RegF |= 0x10;
            }
            else {
                CPU->RegF &= ~0x10;
            }

            //Z Flag
            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80;
            }
            else {
                CPU->RegF &= ~0x80;
            }

            return 8;
        }
        case (0x99): { //SBC A, C
            uint16_t result = CPU->RegA - CPU->RegC - ((CPU->RegF & 0x10) ? 1 : 0);

            //N Flag
            CPU->RegF |= 0x40;

            //H Flag
            if ((CPU->RegA & 0xF) < (CPU->RegC & 0xF) + ((CPU->RegF & 0x10) ? 1 : 0)) {
                CPU->RegF |= 0x20;
            }
            else {
                CPU->RegF &= ~0x20;
            }

            CPU->RegA = result & 0xFF;
            
            //C Flag
            if (result > 0xFF) {
                CPU->RegF |= 0x10;
            }
            else {
                CPU->RegF &= ~0x10;
            }

            //Z Flag
            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80;
            }
            else {
                CPU->RegF &= ~0x80;
            }

            return 8;
        }
        case (0x9A): { //SBC A, D
            uint16_t result = CPU->RegA - CPU->RegD - ((CPU->RegF & 0x10) ? 1 : 0);

            //N Flag
            CPU->RegF |= 0x40;

            //H Flag
            if ((CPU->RegA & 0xF) < (CPU->RegD & 0xF) + ((CPU->RegF & 0x10) ? 1 : 0)) {
                CPU->RegF |= 0x20;
            }
            else {
                CPU->RegF &= ~0x20;
            }

            CPU->RegA = result & 0xFF;
            
            //C Flag
            if (result > 0xFF) {
                CPU->RegF |= 0x10;
            }
            else {
                CPU->RegF &= ~0x10;
            }

            //Z Flag
            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80;
            }
            else {
                CPU->RegF &= ~0x80;
            }

            return 8;
        }
        case (0x9B): { //SBC A, E
            uint16_t result = CPU->RegA - CPU->RegE - ((CPU->RegF & 0x10) ? 1 : 0);

            //N Flag
            CPU->RegF |= 0x40;

            //H Flag
            if ((CPU->RegA & 0xF) < (CPU->RegE & 0xF) + ((CPU->RegF & 0x10) ? 1 : 0)) {
                CPU->RegF |= 0x20;
            }
            else {
                CPU->RegF &= ~0x20;
            }

            CPU->RegA = result & 0xFF;
            
            //C Flag
            if (result > 0xFF) {
                CPU->RegF |= 0x10;
            }
            else {
                CPU->RegF &= ~0x10;
            }

            //Z Flag
            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80;
            }
            else {
                CPU->RegF &= ~0x80;
            }

            return 8;
        }
        case (0x9C): { //SBC A, H
            uint16_t result = CPU->RegA - CPU->RegH - ((CPU->RegF & 0x10) ? 1 : 0);

            //N Flag
            CPU->RegF |= 0x40;

            //H Flag
            if ((CPU->RegA & 0xF) < (CPU->RegH & 0xF) + ((CPU->RegF & 0x10) ? 1 : 0)) {
                CPU->RegF |= 0x20;
            }
            else {
                CPU->RegF &= ~0x20;
            }

            CPU->RegA = result & 0xFF;
            
            //C Flag
            if (result > 0xFF) {
                CPU->RegF |= 0x10;
            }
            else {
                CPU->RegF &= ~0x10;
            }

            //Z Flag
            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80;
            }
            else {
                CPU->RegF &= ~0x80;
            }

            return 8;
        }
        case (0x9D): { //SBC A, L
            uint16_t result = CPU->RegA - CPU->RegL - ((CPU->RegF & 0x10) ? 1 : 0);

            //N Flag
            CPU->RegF |= 0x40;

            //H Flag
            if ((CPU->RegA & 0xF) < (CPU->RegL & 0xF) + ((CPU->RegF & 0x10) ? 1 : 0)) {
                CPU->RegF |= 0x20;
            }
            else {
                CPU->RegF &= ~0x20;
            }

            CPU->RegA = result & 0xFF;
            
            //C Flag
            if (result > 0xFF) {
                CPU->RegF |= 0x10;
            }
            else {
                CPU->RegF &= ~0x10;
            }

            //Z Flag
            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80;
            }
            else {
                CPU->RegF &= ~0x80;
            }

            return 8;
        }
        case (0x9E): { //SBC A, (HL)
            uint16_t result = CPU->RegA - MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) - ((CPU->RegF & 0x10) ? 1 : 0);

            //N Flag
            CPU->RegF |= 0x40;

            //H Flag
            if ((CPU->RegA & 0xF) < (MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) & 0xF) + ((CPU->RegF & 0x10) ? 1 : 0)) {
                CPU->RegF |= 0x20;
            }
            else {
                CPU->RegF &= ~0x20;
            }

            CPU->RegA = result & 0xFF;
            
            //C Flag
            if (result > 0xFF) {
                CPU->RegF |= 0x10;
            }
            else {
                CPU->RegF &= ~0x10;
            }

            //Z Flag
            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80;
            }
            else {
                CPU->RegF &= ~0x80;
            }

            return 8;
        }
        case (0x9F): { //SBC A, A
            uint16_t result = CPU->RegA - CPU->RegA - ((CPU->RegF & 0x10) ? 1 : 0);

            //N Flag
            CPU->RegF |= 0x40;

            //H Flag
            if ((CPU->RegA & 0xF) < (CPU->RegA & 0xF) + ((CPU->RegF & 0x10) ? 1 : 0)) {
                CPU->RegF |= 0x20;
            }
            else {
                CPU->RegF &= ~0x20;
            }

            CPU->RegA = result & 0xFF;
            
            //C Flag
            if (result > 0xFF) {
                CPU->RegF |= 0x10;
            }
            else {
                CPU->RegF &= ~0x10;
            }

            //Z Flag
            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80;
            }
            else {
                CPU->RegF &= ~0x80;
            }

            return 4;
        }
        case (0xA0): { //AND B
            CPU->RegA &= CPU->RegB;
            CPU->RegF = (!CPU->RegA) << 7; //Zero Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            CPU->RegF &= ~0x10; //Clear Carry Flag
            CPU->RegF &= ~0x40; //Clear Sub Flag
            return 4;
        }
        case (0xA1): { //AND C
            CPU->RegA &= CPU->RegC;
            CPU->RegF = (!CPU->RegA) << 7; //Zero Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            CPU->RegF &= ~0x10; //Clear Carry Flag
            CPU->RegF &= ~0x40; //Clear Sub Flag
            return 4;
        }
        case (0xA2): { //AND D
            CPU->RegA &= CPU->RegD;
            CPU->RegF = (!CPU->RegA) << 7; //Zero Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            CPU->RegF &= ~0x10; //Clear Carry Flag
            CPU->RegF &= ~0x40; //Clear Sub Flag
            return 4;
        }
        case (0xA3): { //AND E
            CPU->RegA &= CPU->RegE;
            CPU->RegF = (!CPU->RegA) << 7; //Zero Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            CPU->RegF &= ~0x10; //Clear Carry Flag
            CPU->RegF &= ~0x40; //Clear Sub Flag
            return 4;
        }
        case (0xA4): { //AND H
            CPU->RegA &= CPU->RegH;
            CPU->RegF = (!CPU->RegA) << 7; //Zero Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            CPU->RegF &= ~0x10; //Clear Carry Flag
            CPU->RegF &= ~0x40; //Clear Sub Flag
            return 4;
        }
        case (0xA5): { //AND L
            CPU->RegA &= CPU->RegL;
            CPU->RegF = (!CPU->RegA) << 7; //Zero Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            CPU->RegF &= ~0x10; //Clear Carry Flag
            CPU->RegF &= ~0x40; //Clear Sub Flag
            return 4;
        }
        case (0xA6): { //AND (HL)
            CPU->RegA &= MMURead(MMU, (CPU->RegH << 8 | CPU->RegL));
            CPU->RegF = (!CPU->RegA) << 7; //Zero Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            CPU->RegF &= ~0x10; //Clear Carry Flag
            CPU->RegF &= ~0x40; //Clear Sub Flag
            return 8;
        }
        case (0xA7): { //AND A
            CPU->RegA &= CPU->RegA;
            CPU->RegF = (!CPU->RegA) << 7; //Zero Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            CPU->RegF &= ~0x10; //Clear Carry Flag
            CPU->RegF &= ~0x40; //Clear Sub Flag
            return 4;
        }
        case (0xA8): { //XOR B
            CPU->RegA ^= CPU->RegB;

            CPU->RegF = (!CPU->RegA) << 7; //Set Zero Flag if A is 0

            return 4;
        }
        case (0xA9): { //XOR C
            CPU->RegA ^= CPU->RegC;

            CPU->RegF = (!CPU->RegA) << 7; //Set Zero Flag if A is 0

            return 4;
        }
        case (0xAA): { //XOR D
            CPU->RegA ^= CPU->RegD;

            CPU->RegF = (!CPU->RegA) << 7; //Set Zero Flag if A is 0

            return 4;
        }
        case (0xAB): { //XOR E
            CPU->RegA ^= CPU->RegE;

            CPU->RegF = (!CPU->RegA) << 7; //Set Zero Flag if A is 0

            return 4;
        }
        case (0xAC): { //XOR H
            CPU->RegA ^= CPU->RegH;

            CPU->RegF = (!CPU->RegA) << 7; //Set Zero Flag if A is 0

            return 4;
        }
        case (0xAD): { //XOR L
            CPU->RegA ^= CPU->RegL;

            CPU->RegF = (!CPU->RegA) << 7; //Set Zero Flag if A is 0

            return 4;
        }
        case (0xAE): { //XOR (HL)
            CPU->RegA ^= MMURead(MMU, (CPU->RegH << 8 | CPU->RegL));

              
            CPU->RegF = (!CPU->RegA) << 7; //Set Zero Flag if A is 0

            return 8;
        }
        case (0xAF): { //XOR A, A
            CPU->RegA ^= CPU->RegA;
            CPU->RegF = 0x80;
            return 4;
        }
        case (0xB0): { //OR B
            CPU->RegA |= CPU->RegB;
        
            CPU->RegF = (!CPU->RegA) << 7; //Zero Flag

            return 4;
        }
        case (0xB1): { //OR A, C
            CPU->RegA |= CPU->RegC;
        
            CPU->RegF = (!CPU->RegA) << 7; //Zero Flag

            return 4;
        }
        case (0xB2): { //OR D
            CPU->RegA |= CPU->RegD;
        
            CPU->RegF = (!CPU->RegA) << 7; //Zero Flag

            return 4;
        }
        case (0xB3): { //OR E
            CPU->RegA |= CPU->RegE;
        
            CPU->RegF = (!CPU->RegA) << 7; //Zero Flag

            return 4;
        }
        case (0xB4): { //OR H
            CPU->RegA |= CPU->RegH;
        
            CPU->RegF = (!CPU->RegA) << 7; //Zero Flag

            return 4;
        }
        case (0xB5): { //OR L
            CPU->RegA |= CPU->RegL;
        
            CPU->RegF = (!CPU->RegA) << 7; //Zero Flag

            return 4;
        }
        case (0xB6): { //OR (HL)
            CPU->RegA |= MMURead(MMU, (CPU->RegH << 8 | CPU->RegL));
            
            CPU->RegF = (!CPU->RegA) << 7; //Zero Flag

            return 8;
        }
        case (0xB7): { //OR A
            CPU->RegA |= CPU->RegA;
            
            CPU->RegF = (!CPU->RegA) << 7; //Zero Flag
            return 4;
        }
        case (0xB8): { //CP B
            if (CPU->RegA == CPU->RegB) {
                CPU->RegF |= 0x80; //Zero Flag
            }
            else {
                CPU->RegF &= ~0x80;
            }

            CPU->RegF |= 0x40; //Set Sub Flag

            if ((CPU->RegA & 0xF) < (CPU->RegB & 0xF)) {
                CPU->RegF |= 0x20; //Half Carry Flag
            }
            else {
                CPU->RegF &= ~0x20;
            }

            if (CPU->RegA < CPU->RegB) {
                CPU->RegF |= 0x10; //Carry Flag
            }
            else {
                CPU->RegF &= ~0x10;
            }

            return 4;

        }
        case (0xB9): { //CP C
            if (CPU->RegA == CPU->RegC) {
                CPU->RegF |= 0x80; //Zero Flag
            }
            else {
                CPU->RegF &= ~0x80;
            }

            CPU->RegF |= 0x40; //Set Sub Flag

            if ((CPU->RegA & 0xF) < (CPU->RegC & 0xF)) {
                CPU->RegF |= 0x20; //Half Carry Flag
            }
            else {
                CPU->RegF &= ~0x20;
            }

            if (CPU->RegA < CPU->RegC) {
                CPU->RegF |= 0x10; //Carry Flag
            }
            else {
                CPU->RegF &= ~0x10;
            }

            return 4;
        }
        case (0xBA): { //CP D
            if (CPU->RegA == CPU->RegD) {
                CPU->RegF |= 0x80; //Zero Flag
            }
            else {
                CPU->RegF &= ~0x80;
            }

            CPU->RegF |= 0x40; //Set Sub Flag

            if ((CPU->RegA & 0xF) < (CPU->RegD & 0xF)) {
                CPU->RegF |= 0x20; //Half Carry Flag
            }
            else {
                CPU->RegF &= ~0x20;
            }

            if (CPU->RegA < CPU->RegD) {
                CPU->RegF |= 0x10; //Carry Flag
            }
            else {
                CPU->RegF &= ~0x10;
            }

            return 4; 

        }
        case (0xBB): { //CP E
            if (CPU->RegA == CPU->RegE) {
                CPU->RegF |= 0x80; //Zero Flag
            }
            else {
                CPU->RegF &= ~0x80;
            }

            CPU->RegF |= 0x40; //Set Sub Flag

            if ((CPU->RegA & 0xF) < (CPU->RegE & 0xF)) {
                CPU->RegF |= 0x20; //Half Carry Flag
            }
            else {
                CPU->RegF &= ~0x20;
            }

            if (CPU->RegA < CPU->RegE) {
                CPU->RegF |= 0x10; //Carry Flag
            }
            else {
                CPU->RegF &= ~0x10;
            }

            return 4;
        }
        case (0xBC): { //CP H
            if (CPU->RegA == CPU->RegH) {
                CPU->RegF |= 0x80; //Zero Flag
            }
            else {
                CPU->RegF &= ~0x80;
            }

            CPU->RegF |= 0x40; //Set Sub Flag

            if ((CPU->RegA & 0xF) < (CPU->RegH & 0xF)) {
                CPU->RegF |= 0x20; //Half Carry Flag
            }
            else {
                CPU->RegF &= ~0x20;
            }

            if (CPU->RegA < CPU->RegH) {
                CPU->RegF |= 0x10; //Carry Flag
            }
            else {
                CPU->RegF &= ~0x10;
            }

            return 4;
        }
        case (0xBD): { //CP L
            if (CPU->RegA == CPU->RegL) {
                CPU->RegF |= 0x80; //Zero Flag
            }
            else {
                CPU->RegF &= ~0x80;
            }

            CPU->RegF |= 0x40; //Set Sub Flag

            if ((CPU->RegA & 0xF) < (CPU->RegL & 0xF)) {
                CPU->RegF |= 0x20; //Half Carry Flag
            }
            else {
                CPU->RegF &= ~0x20;
            }

            if (CPU->RegA < CPU->RegL) {
                CPU->RegF |= 0x10; //Carry Flag
            }
            else {
                CPU->RegF &= ~0x10;
            }

            return 4;
        }
        case (0xBE): { //CP (HL)
            if (CPU->RegA == MMURead(MMU, (CPU->RegH << 8 | CPU->RegL))) {
                CPU->RegF |= 0x80; //Zero Flag
            }
            else {
                CPU->RegF &= ~0x80;
            }

            CPU->RegF |= 0x40; //Set Sub Flag

            if ((CPU->RegA & 0xF) < (MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) & 0xF)) {
                CPU->RegF |= 0x20; //Half Carry Flag
            }
            else {
                CPU->RegF &= ~0x20;
            }

            if (CPU->RegA < MMURead(MMU, (CPU->RegH << 8 | CPU->RegL))) {
                CPU->RegF |= 0x10; //Carry Flag
            }
            else {
                CPU->RegF &= ~0x10;
            }

            return 8;
        }
        case (0xBF): { //CP A
            if (CPU->RegA == CPU->RegA) {
                CPU->RegF |= 0x80; //Zero Flag
            }
            else {
                CPU->RegF &= ~0x80;
            }

            CPU->RegF |= 0x40; //Set Sub Flag

            if ((CPU->RegA & 0xF) < (CPU->RegA & 0xF)) {
                CPU->RegF |= 0x20; //Half Carry Flag
            }
            else {
                CPU->RegF &= ~0x20;
            }

            if (CPU->RegA < CPU->RegA) {
                CPU->RegF |= 0x10; //Carry Flag
            }
            else {
                CPU->RegF &= ~0x10;
            }

            return 4;
        }
        case (0xC0): { //RET NZ
            if (!(CPU->RegF & 0x80)) {
                CPU->PC = MMURead(MMU, CPU->SP + 1) << 8 | MMURead(MMU, CPU->SP);
                CPU->SP = CPU->SP + 2 & 0xFFFF;
                return 20;
            }
            return 8;
        }
        case (0xC1): { //POP BC
            CPU->RegB = MMURead(MMU, CPU->SP + 1);
            CPU->RegC = MMURead(MMU, CPU->SP);
            CPU->SP += 2;
            return 12;
        }
        case (0xC2): { //JP NZ, imm16
            address = MMURead(MMU, CPU->PC + 1) << 8 | MMURead(MMU, CPU->PC);
            CPU->PC += 2;
            if (!(CPU->RegF & 0x80)) {
                CPU->PC = address;
                return 16;
            }
            return 12;
        }
        case (0xC3): { //JP imm16
            CPU->PC = MMURead(MMU, CPU->PC+1) << 8 | MMURead(MMU, CPU->PC);
            return 16;
        }
        case (0xC4): { //CALL NZ, imm16
            address = MMURead(MMU, CPU->PC + 1) << 8 | MMURead(MMU, CPU->PC);
            CPU->PC += 2;
            if (!(CPU->RegF & 0x80)) {
                CPU->SP -= 2;
                MMUWrite(MMU, CPU->SP, CPU->PC & 0x00FF);
                MMUWrite(MMU, CPU->SP + 1, (CPU->PC >> 8) & 0x00FF);
                CPU->PC = address;
                return 24;
            }
            return 12;
        }
        case (0xC5): { //PUSH BC
            CPU->SP -= 2;
            MMUWrite(MMU, CPU->SP + 1, CPU->RegB);
            MMUWrite(MMU, CPU->SP, CPU->RegC);
            return 16;
        }
        case (0xC6): { //ADD A, n8
            n8 = MMURead(MMU, CPU->PC);
            CPU->PC++;

            if ((CPU->RegA + n8) > 0xFF) {
                CPU->RegF |= 0x10; //Set Carry Flag
            }
            else {
                CPU->RegF &= ~0x10; //Clear Carry Flag
            }

            if (((CPU->RegA & 0xF) + (n8 & 0xF)) > 0xF) {
                CPU->RegF |= 0x20; //Set Half Carry Flag
            }
            else {
                CPU->RegF &= ~0x20; //Clear Half Carry Flag
            }

            CPU->RegA += n8;

            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            else {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }

            CPU->RegF &= ~0x40; //Clear Sub Flag

            return 8;
        }
        case (0xC7): { //RST 00H
            CPU->SP -= 2;
            MMUWrite(MMU, CPU->SP + 1, CPU->PC >> 8);
            MMUWrite(MMU, CPU->SP, CPU->PC & 0xFF);
            CPU->PC = 0x00;
            return 16;
        }
        case (0xC8): { //RET Z
            if (CPU->RegF & 0x80) {
                CPU->PC = MMURead(MMU, CPU->SP + 1) << 8 | MMURead(MMU, CPU->SP);
                CPU->SP = CPU->SP + 2 & 0xFFFF;
                return 20;
            }
            return 8;
        }
        case (0xC9): { //RET
            CPU->PC = ((MMURead(MMU, CPU->SP + 1) << 8) | MMURead(MMU, CPU->SP));
            CPU->SP = CPU->SP + 2 & 0xFFFF;
            return 16;

        }
        case (0xCA): { //JP Z, a16
            address = MMURead(MMU, CPU->PC + 1) << 8 | MMURead(MMU, CPU->PC);
            CPU->PC += 2;
            if ((CPU->RegF & 0x80)) {
                CPU->PC = address;
                return 16;
            }
            return 12;
        }
        case (0xCB): { //CB Prefix
            return CPUExecuteCB(CPU, MMU);
        }
        case (0xCC): { //CALL Z, imm16
            address = MMURead(MMU, CPU->PC + 1) << 8 | MMURead(MMU, CPU->PC);
            CPU->PC += 2;
            if (CPU->RegF & 0x80) {
                CPU->SP -= 2;
                MMUWrite(MMU, CPU->SP, CPU->PC & 0x00FF);
                MMUWrite(MMU, CPU->SP + 1, (CPU->PC >> 8) & 0x00FF);
                CPU->PC = address;
                return 24;
            }
            return 12;
        }
        case (0xCD): { //CALL imm16
            //Push PC to Stack
            CPU->SP -= 2;
            MMUWrite(MMU, CPU->SP, (CPU->PC + 2) & 0x00FF);
            MMUWrite(MMU, CPU->SP + 1, ((CPU->PC + 2) >> 8) & 0x00FF);
            //Jump to imm16
            CPU->PC = MMURead(MMU, CPU->PC+1) << 8 | MMURead(MMU, CPU->PC);
            return 24;

        }
        case (0xCE): { //ADC A, n8
            n8 = MMURead(MMU, CPU->PC);
            CPU->PC++;

            uint16_t result = CPU->RegA + n8 + ((CPU->RegF & 0x10) ? 1 : 0);

            //N Flag
            CPU->RegF &= ~0x40;

            //H Flag
            if (((CPU->RegA & 0xF) + (n8 & 0xF) + ((CPU->RegF & 0x10) ? 1 : 0)) >= 0x10) {
                CPU->RegF |= 0x20;
            }
            else {
                CPU->RegF &= ~0x20;
            }

            CPU->RegA = result & 0xFF;
            
            //C Flag
            if (result > 0xFF) {
                CPU->RegF |= 0x10;
            }
            else {
                CPU->RegF &= ~0x10;
            }

            //Z Flag
            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80;
            }
            else {
                CPU->RegF &= ~0x80;
            }

            return 8;
        }
        case (0xCF): { //RST 08H
            CPU->SP -= 2;
            MMUWrite(MMU, CPU->SP + 1, CPU->PC >> 8);
            MMUWrite(MMU, CPU->SP, CPU->PC & 0xFF);
            CPU->PC = 0x08;
            return 16;
        }
        case (0xD0): { //RET NC
            if (!(CPU->RegF & 0x10)) {
                CPU->PC = MMURead(MMU, CPU->SP + 1) << 8 | MMURead(MMU, CPU->SP);
                CPU->SP = CPU->SP + 2 & 0xFFFF;
                return 20;
            }
            return 8;
        }
        case (0xD1): { //POP DE
            CPU->RegD = MMURead(MMU, CPU->SP + 1);
            CPU->RegE = MMURead(MMU, CPU->SP);
            CPU->SP += 2;
            return 12;
        }
        case (0xD2): { //JP NC, imm16
            address = MMURead(MMU, CPU->PC + 1) << 8 | MMURead(MMU, CPU->PC);
            CPU->PC += 2;
            if (!(CPU->RegF & 0x10)) {
                CPU->PC = address;
                return 16;
            }
            return 12;
        }
        case (0xD4): { //CALL NC, imm16
            address = MMURead(MMU, CPU->PC + 1) << 8 | MMURead(MMU, CPU->PC);
            CPU->PC += 2;
            if (!(CPU->RegF & 0x10)) {
                CPU->SP -= 2;
                MMUWrite(MMU, CPU->SP, CPU->PC & 0x00FF);
                MMUWrite(MMU, CPU->SP + 1, (CPU->PC >> 8) & 0x00FF);
                CPU->PC = address;
                return 24;
            }
            return 12;
        }
        case (0xD5): { //PUSH DE
            CPU->SP -= 2;
            MMUWrite(MMU, CPU->SP + 1, CPU->RegD);
            MMUWrite(MMU, CPU->SP, CPU->RegE);
            return 16;
        }
        case (0xD6): { //SUB n8
            n8 = MMURead(MMU, CPU->PC);
            CPU->PC++;

            if (CPU->RegA < n8) {
                CPU->RegF |= 0x10; //Set Carry Flag
            }
            else {
                CPU->RegF &= ~0x10; //Clear Carry Flag
            }

            if ((CPU->RegA & 0xF) < (n8 & 0xF)) {
                CPU->RegF |= 0x20; //Set Half Carry Flag
            }
            else {
                CPU->RegF &= ~0x20; //Clear Half Carry Flag
            }

            CPU->RegA -= n8;

            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            else {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }

            CPU->RegF |= 0x40; //Set Sub Flag

            return 8;
        }
        case (0xD7): { //RST 10H
            CPU->SP -= 2;
            MMUWrite(MMU, CPU->SP + 1, CPU->PC >> 8);
            MMUWrite(MMU, CPU->SP, CPU->PC & 0xFF);
            CPU->PC = 0x10;
            return 16;
        }
        case (0xD8): { //RET C
            if (CPU->RegF & 0x10) {
                CPU->PC = MMURead(MMU, CPU->SP + 1) << 8 | MMURead(MMU, CPU->SP);
                CPU->SP = CPU->SP + 2 & 0xFFFF;
                return 20;
            }
            return 8;
        }
        case (0xD9): { //RET I
            CPU->PC = MMURead(MMU, CPU->SP + 1) << 8 | MMURead(MMU, CPU->SP);
            CPU->SP = CPU->SP + 2 & 0xFFFF;
            CPU->IME = 1;
            return 16;
        }
        case (0xDA): { //JP C, imm16
            int address = MMURead(MMU, CPU->PC + 1) << 8 | MMURead(MMU, CPU->PC);
            CPU->PC += 2;
            if (CPU->RegF & 0x10) {
                CPU->PC = address;
                return 16;
            }
            return 12;
        }
        case (0xDC): { //CALL C, imm16
            address = MMURead(MMU, CPU->PC + 1) << 8 | MMURead(MMU, CPU->PC);
            CPU->PC += 2;
            if (CPU->RegF & 0x10) {
                CPU->SP -= 2;
                MMUWrite(MMU, CPU->SP, CPU->PC & 0x00FF);
                MMUWrite(MMU, CPU->SP + 1, (CPU->PC >> 8) & 0x00FF);
                CPU->PC = address;
                return 24;
            }
            return 12;
        }
        case (0xDE): { //SBC A, n8
            uint8_t n8 = MMURead(MMU, CPU->PC);
            CPU->PC++;

            uint8_t flagC =  ((CPU->RegF & 0x10) ? 1 : 0);

            //Half Carry Flag
            if (((CPU->RegA & 0xF) - (n8 & 0xF) - flagC) < 0) {
                CPU->RegF |= 0x20;
            }
            else {
                CPU->RegF &= ~(0x20);
            }

            //Carry Flag
            if ((CPU->RegA - n8 - flagC) < 0) {
                CPU->RegF |= 0x10;
            }
            else {
                CPU->RegF &= ~(0x10);
            }

            CPU->RegA = CPU->RegA - n8 - flagC;

            //Zero Flag
            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80;
            }
            else {
                CPU->RegF &= ~(0x80);
            }

            CPU->RegF |= 0x40; //Set Sub Flag

            return 8;
        }
        case (0xDF): { //RST 18H
            CPU->SP -= 2;
            MMUWrite(MMU, CPU->SP + 1, CPU->PC >> 8);
            MMUWrite(MMU, CPU->SP, CPU->PC & 0xFF);
            CPU->PC = 0x18;
            return 16;
        }
        case (0xE0): { //LDH A, imm16
            MMUWrite(MMU, (0xFF00 | MMURead(MMU, CPU->PC)), CPU->RegA);
            CPU->PC++;
            return 12;
        }
        case (0xE1): { //POP HL
            CPU->RegH = MMURead(MMU, CPU->SP + 1);
            CPU->RegL = MMURead(MMU, CPU->SP);
            CPU->SP += 2;
            return 12;
        }
        case (0xE2): { //LD (C), A
            MMUWrite(MMU, 0xFF00 | CPU->RegC, CPU->RegA);
            return 8;
        }
        case (0xE5): { //PUSH HL
            CPU->SP -= 2;
            MMUWrite(MMU, CPU->SP + 1, CPU->RegH);
            MMUWrite(MMU, CPU->SP, CPU->RegL);
            return 16;
        }
        case (0xE6): { //AND n8
            n8 = MMURead(MMU, CPU->PC);
            CPU->PC++;

            CPU->RegA &= n8;

            CPU->RegF = (!CPU->RegA) << 7; //Zero Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            CPU->RegF &= ~0x10; //Clear Carry Flag
            CPU->RegF &= ~0x40; //Clear Sub Flag
            return 8;
        }
        case (0xE7): { //RST 20H
            CPU->SP -= 2;
            MMUWrite(MMU, CPU->SP + 1, CPU->PC >> 8);
            MMUWrite(MMU, CPU->SP, CPU->PC & 0xFF);
            CPU->PC = 0x20;
            return 16;
        }
        case (0xE8): { //ADD SP, r8
            int8_t r8 = (int8_t)MMURead(MMU, CPU->PC);
            CPU->PC++;

            uint16_t SP = CPU->SP;
            uint16_t result = SP + r8;

            CPU->RegF = 0; // Clear all flags

            // Half Carry flag
            if (((SP & 0x0F) + (r8 & 0x0F)) > 0x0F) {
                CPU->RegF |= 0x20;
            }

            // Carry flag
            if (((SP & 0xFF) + (r8 & 0xFF)) > 0xFF) {
                CPU->RegF |= 0x10;
            }       

            CPU->SP = result;

            return 16;
        }
        case (0xE9): { //JP HL
            CPU->PC = CPU->RegH << 8 | CPU->RegL;
            return 4;
        }
        case (0xEA): { //LD imm16, A
            MMUWrite(MMU, MMURead(MMU, CPU->PC+1) << 8 | MMURead(MMU, CPU->PC), CPU->RegA);
            CPU->PC += 2;
            return 16;
        }
        case (0xEE): { //XOR n8
            n8 = MMURead(MMU, CPU->PC);
            CPU->PC++;

            CPU->RegA ^= n8;

            CPU->RegF = 0; //Clear Flags    
            CPU->RegF = (!CPU->RegA) << 7; //Set Zero Flag if A is 0

            return 8;
        }
        case (0xEF): { //RST 28H
            CPU->SP -= 2;
            MMUWrite(MMU, CPU->SP + 1, CPU->PC >> 8);
            MMUWrite(MMU, CPU->SP, CPU->PC & 0xFF);
            CPU->PC = 0x28;
            return 16;
        }
        case (0xF0): { //LDH A, imm16
            CPU->RegA = MMURead(MMU, 0xFF00 | MMURead(MMU, CPU->PC));
            CPU->PC++;
            return 12;
        }
        case (0xF1): { //POP AF
            CPU->RegA = MMURead(MMU, CPU->SP + 1);
            CPU->RegF = MMURead(MMU, CPU->SP); 
            CPU->RegF = CPU->RegF & 0xF0; //The last 4 bits of F are hardwired to 0
            CPU->SP += 2;
            return 12;
        }
        case (0xF2): { //LD A, (C)
            CPU->RegA = MMURead(MMU, 0xFF00 | CPU->RegC);
            return 8;
        }
        case (0xF3): { //DI
            CPU->IME = 0;
            return 4;
        }
        case (0xF5): { //PUSH AF
            CPU->SP -= 2;
            MMUWrite(MMU, CPU->SP + 1, CPU->RegA);
            MMUWrite(MMU, CPU->SP, CPU->RegF);
            return 16;
        }
        case (0xF6): { //OR n8
            n8 = MMURead(MMU, CPU->PC);
            CPU->PC++;

            CPU->RegA |= n8;

            CPU->RegF = 0; //Clear Flags
            CPU->RegF = (!CPU->RegA) << 7; //Set Zero Flag if A is 0

            return 8;
        }
        case (0xF7): { //RST 30H
            CPU->SP -= 2;
            MMUWrite(MMU, CPU->SP + 1, CPU->PC >> 8);
            MMUWrite(MMU, CPU->SP, CPU->PC & 0xFF);
            CPU->PC = 0x30;
            return 16;
        }
        case (0xF8): { //LD HL, SP + e8
            int8_t r8 = (int8_t)MMURead(MMU, CPU->PC);
            CPU->PC++;

            uint16_t SP = CPU->SP;
            uint16_t result = SP + r8;

            CPU->RegF = 0; // Clear all flags

            // Half Carry flag
            if (((SP & 0x0F) + (r8 & 0x0F)) > 0x0F) {
                CPU->RegF |= 0x20;
            }

            // Carry flag
            if (((SP & 0xFF) + (r8 & 0xFF)) > 0xFF) {
                CPU->RegF |= 0x10;
            }

            CPU->RegH = (result >> 8) & 0xFF;
            CPU->RegL = result & 0xFF;

            return 12;
        }
        case (0xF9): { //LD SP, HL
            CPU->SP = CPU->RegH << 8 | CPU->RegL;
            return 8;
        }
        case (0xFA): { //LD A, imm16
            CPU->RegA = MMURead(MMU, MMURead(MMU, CPU->PC+1) << 8 | MMURead(MMU, CPU->PC));
            CPU->PC += 2;
            return 16;
        }
        case (0xFB): { //EI
            CPU->IME = 1;
            return 4;
        }
        case (0xFE): { //CP n8
            uint8_t n8 = MMURead(MMU, CPU->PC++);

            CPU->RegF &= ~(0x10); // Clear Carry Flag
            CPU->RegF |= 0x40;    // Set Subtract Flag

            if ((CPU->RegA - n8) == 0) {
                CPU->RegF |= 0x80; // Set Zero Flag
            } 
            else {
                CPU->RegF &= ~(0x80); // Clear Zero Flag
            }

            if ((CPU->RegA & 0x0F) < (n8 & 0x0F)) {
                CPU->RegF |= 0x20; // Set Half Carry Flag
            } 
            else {
                CPU->RegF &= ~(0x20); // Clear Half Carry Flag
            }

            if (CPU->RegA < n8) {
                CPU->RegF |= 0x10; // Set Carry Flag
            } 
            else {
                CPU->RegF &= ~(0x10); // Clear Carry Flag
            }
            return 8;
        }
        case (0xFF): { //RST 38
            CPU->SP -= 2;
            MMUWrite(MMU, CPU->SP, CPU->PC & 0x00FF);
            MMUWrite(MMU, CPU->SP + 1, (CPU->PC >> 8) & 0x00FF);
            CPU->PC = 0x0038;
            return 16;
        }
        default: {
            printf("Unimplemented Opcode: 0x%X\n", opcode);
            return 4;
        }
    }
}

uint8_t CPUExecuteCB(CPU *CPU, MMU *MMU) {
    uint8_t opcode = MMURead(MMU, CPU->PC);
    CPU->PC++;
    //Instruction vars
    uint8_t carry;

    switch (opcode) {
        case (0x00): { //RLC B
            carry = CPU->RegB & 0x80;
            CPU->RegB = (CPU->RegB << 1) | (CPU->RegB >> 7);
            CPU->RegF = (carry ? 0x10 : 0x00) | (CPU->RegB == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x01): { //RLC C
            carry = CPU->RegC & 0x80;
            CPU->RegC = (CPU->RegC << 1) | (CPU->RegC >> 7);
            CPU->RegF = (carry ? 0x10 : 0x00) | (CPU->RegC == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x02): { //RLC D
            carry = CPU->RegD & 0x80;
            CPU->RegD = (CPU->RegD << 1) | (CPU->RegD >> 7);
            CPU->RegF = (carry ? 0x10 : 0x00) | (CPU->RegD == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x03): { //RLC E
            carry = CPU->RegE & 0x80;
            CPU->RegE = (CPU->RegE << 1) | (CPU->RegE >> 7);
            CPU->RegF = (carry ? 0x10 : 0x00) | (CPU->RegE == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x04): { //RLC H
            carry = CPU->RegH & 0x80;
            CPU->RegH = (CPU->RegH << 1) | (CPU->RegH >> 7);
            CPU->RegF = (carry ? 0x10 : 0x00) | (CPU->RegH == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x05): { //RLC L
            carry = CPU->RegL & 0x80;
            CPU->RegL = (CPU->RegL << 1) | (CPU->RegL >> 7);
            CPU->RegF = (carry ? 0x10 : 0x00) | (CPU->RegL == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x06): { //RLC (HL)
            carry = MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) & 0x80;
            MMUWrite(MMU, (CPU->RegH << 8 | CPU->RegL), (MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) << 1) | (MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) >> 7));
            CPU->RegF = (carry ? 0x10 : 0x00) | (MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) == 0 ? 0x80 : 0x00);
            return 16;
        }
        case (0x07): { //RLC A
            carry = CPU->RegA & 0x80;
            CPU->RegA = (CPU->RegA << 1) | (CPU->RegA >> 7);
            CPU->RegF = (carry ? 0x10 : 0x00) | (CPU->RegA == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x08): { //RRC B
            uint8_t newCarryFlag = CPU->RegB & 0x01;
            CPU->RegB = (CPU->RegB >> 1) | (newCarryFlag << 7);
            CPU->RegF &= ~(0xF0);
            CPU->RegF |= (newCarryFlag << 4);
            if (CPU->RegB == 0) {
                CPU->RegF |= 0x80;
            }
            return 8;
        }
        case (0x09): { //RRC C
            uint8_t newCarryFlag = CPU->RegC & 0x01;
            CPU->RegC = (CPU->RegC >> 1) | (newCarryFlag << 7);
            CPU->RegF &= ~(0xF0);
            CPU->RegF |= (newCarryFlag << 4);
            if (CPU->RegC == 0) {
                CPU->RegF |= 0x80;
            }
            return 8;
        }
        case (0x0A): { //RRC D
            uint8_t newCarryFlag = CPU->RegD & 0x01;
            CPU->RegD = (CPU->RegD >> 1) | (newCarryFlag << 7);
            CPU->RegF &= ~(0xF0);
            CPU->RegF |= (newCarryFlag << 4);
            if (CPU->RegD == 0) {
                CPU->RegF |= 0x80;
            }
            return 8;
        }
        case (0x0B): { //RRC E
            uint8_t newCarryFlag = CPU->RegE & 0x01;
            CPU->RegE = (CPU->RegE >> 1) | (newCarryFlag << 7);
            CPU->RegF &= ~(0xF0);
            CPU->RegF |= (newCarryFlag << 4);
            if (CPU->RegE == 0) {
                CPU->RegF |= 0x80;
            }
            return 8;
        }
        case (0x0C): { //RRC H
            uint8_t newCarryFlag = CPU->RegH & 0x01;
            CPU->RegH = (CPU->RegH >> 1) | (newCarryFlag << 7);
            CPU->RegF &= ~(0xF0);
            CPU->RegF |= (newCarryFlag << 4);
            if (CPU->RegH == 0) {
                CPU->RegF |= 0x80;
            }
            return 8;
        }
        case (0x0D): { //RRC L
            uint8_t newCarryFlag = CPU->RegL & 0x01;
            CPU->RegL = (CPU->RegL >> 1) | (newCarryFlag << 7);
            CPU->RegF &= ~(0xF0);
            CPU->RegF |= (newCarryFlag << 4);
            if (CPU->RegL == 0) {
                CPU->RegF |= 0x80;
            }
            return 8;
        }
        case (0x0E): { //RRC (HL)
            uint8_t newCarryFlag = MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) & 0x01;
            MMUWrite(MMU, (CPU->RegH << 8 | CPU->RegL), (MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) >> 1) | (newCarryFlag << 7));
            CPU->RegF &= ~(0xF0);
            CPU->RegF |= (newCarryFlag << 4);
            if (MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) == 0) {
                CPU->RegF |= 0x80;
            }
            return 16;
        }
        case (0x0F): { //RRC A
            uint8_t newCarryFlag = CPU->RegA & 0x01;
            CPU->RegA = (CPU->RegA >> 1) | (newCarryFlag << 7);
            CPU->RegF &= ~(0xF0);
            CPU->RegF |= (newCarryFlag << 4);
            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80;
            }
            return 8;
        }
        case (0x10): { //RL B
            uint8_t carry = CPU->RegB >> 7;
            uint8_t val = CPU->RegB;

            val = (val << 1) | (CPU->RegF & 0x10 ? 1 : 0);
            CPU->RegF = (carry ? 0x10 : 0x00) | (val == 0 ? 0x80 : 0x00);

            CPU->RegB = val;
            return 8;
        }
        case (0x11): { //RL C
            uint8_t carry = CPU->RegC >> 7;
            uint8_t val = CPU->RegC;
            
            val = (val << 1) | (CPU->RegF & 0x10 ? 1 : 0);
            CPU->RegF = (carry ? 0x10 : 0x00) | (val == 0 ? 0x80 : 0x00);

            CPU->RegC = val;
            return 8;
        }
        case (0x12): { //RL D
            uint8_t carry = CPU->RegD >> 7;
            uint8_t val = CPU->RegD;
            
            val = (val << 1) | (CPU->RegF & 0x10 ? 1 : 0);
            CPU->RegF = (carry ? 0x10 : 0x00) | (val == 0 ? 0x80 : 0x00);

            CPU->RegD = val;
            return 8;
        }
        case (0x13): { //RL E
            uint8_t carry = CPU->RegE >> 7;
            uint8_t val = CPU->RegE;
            
            val = (val << 1) | (CPU->RegF & 0x10 ? 1 : 0);
            CPU->RegF = (carry ? 0x10 : 0x00) | (val == 0 ? 0x80 : 0x00);

            CPU->RegE = val;
            return 8;
        }
        case (0x14): { //RL H
            uint8_t carry = CPU->RegH >> 7;
            uint8_t val = CPU->RegH;
            
            val = (val << 1) | (CPU->RegF & 0x10 ? 1 : 0);
            CPU->RegF = (carry ? 0x10 : 0x00) | (val == 0 ? 0x80 : 0x00);

            CPU->RegH = val;
            return 8;
        }
        case (0x15): { //RL L
            uint8_t carry = CPU->RegL >> 7;
            uint8_t val = CPU->RegL;
            
            val = (val << 1) | (CPU->RegF & 0x10 ? 1 : 0);
            CPU->RegF = (carry ? 0x10 : 0x00) | (val == 0 ? 0x80 : 0x00);

            CPU->RegL = val;
            return 8;
        }
        case (0x16): { //RL (HL)
            uint8_t carry = MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) >> 7;
            uint8_t val = MMURead(MMU, (CPU->RegH << 8 | CPU->RegL));
            
            val = (val << 1) | (CPU->RegF & 0x10 ? 1 : 0);
            CPU->RegF = (carry ? 0x10 : 0x00) | (val == 0 ? 0x80 : 0x00);

            MMUWrite(MMU, (CPU->RegH << 8 | CPU->RegL), val);
            return 16;
        }
        case (0x17): { //RL A
            uint8_t carry = CPU->RegA >> 7;
            uint8_t val = CPU->RegA;
            
            val = (val << 1) | (CPU->RegF & 0x10 ? 1 : 0);
            CPU->RegF = (carry ? 0x10 : 0x00) | (val == 0 ? 0x80 : 0x00);

            CPU->RegA = val;
            return 8;
        }
        case (0x18): { //RR B
            carry = CPU->RegB & 0x01;
            CPU->RegB = (CPU->RegB >> 1) | ((CPU->RegF & 0x10) << 3);
            CPU->RegF = (carry ? 0x10 : 0x00) | (CPU->RegB == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x19): { //RR C
            carry = CPU->RegC & 0x01;
            CPU->RegC = (CPU->RegC >> 1) | ((CPU->RegF & 0x10) << 3);
            CPU->RegF = (carry ? 0x10 : 0x00) | (CPU->RegC == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x1A): { //RR D
            carry = CPU->RegD & 0x01;
            CPU->RegD = (CPU->RegD >> 1) | ((CPU->RegF & 0x10) << 3);
            CPU->RegF = (carry ? 0x10 : 0x00) | (CPU->RegD == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x1B): {  //RR E
            carry = CPU->RegE & 0x01;
            CPU->RegE = (CPU->RegE >> 1) | ((CPU->RegF & 0x10) << 3);
            CPU->RegF = (carry ? 0x10 : 0x00) | (CPU->RegE == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x1C): { //RR H
            carry = CPU->RegH & 0x01;
            CPU->RegH = (CPU->RegH >> 1) | ((CPU->RegF & 0x10) << 3);
            CPU->RegF = (carry ? 0x10 : 0x00) | (CPU->RegH == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x1D): { //RR L
            carry = CPU->RegL & 0x01;
            CPU->RegL = (CPU->RegL >> 1) | ((CPU->RegF & 0x10) << 3);
            CPU->RegF = (carry ? 0x10 : 0x00) | (CPU->RegL == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x1E): { //RR (HL)
            carry = MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) & 0x01;
            MMUWrite(MMU, (CPU->RegH << 8 | CPU->RegL), (MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) >> 1) | ((CPU->RegF & 0x10) << 3));
            CPU->RegF = (carry ? 0x10 : 0x00) | (MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) == 0 ? 0x80 : 0x00);
            return 16;
        }
        case (0x1F): { //RR A
            carry = CPU->RegA & 0x01;
            CPU->RegA = (CPU->RegA >> 1) | ((CPU->RegF & 0x10) << 3);
            CPU->RegF = (carry ? 0x10 : 0x00) | (CPU->RegA == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x20): { //SLA B
            carry = CPU->RegB & 0x80;
            CPU->RegB <<= 1;
            CPU->RegF = (carry ? 0x10 : 0x00) | (CPU->RegB == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x21): { //SLA C
            carry = CPU->RegC & 0x80;
            CPU->RegC <<= 1;
            CPU->RegF = (carry ? 0x10 : 0x00) | (CPU->RegC == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x22): { //SLA D
            carry = CPU->RegD & 0x80;
            CPU->RegD <<= 1;
            CPU->RegF = (carry ? 0x10 : 0x00) | (CPU->RegD == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x23): { //SLA E
            carry = CPU->RegE & 0x80;
            CPU->RegE <<= 1;
            CPU->RegF = (carry ? 0x10 : 0x00) | (CPU->RegE == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x24): { //SLA H
            carry = CPU->RegH & 0x80;
            CPU->RegH <<= 1;
            CPU->RegF = (carry ? 0x10 : 0x00) | (CPU->RegH == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x25): { //SLA L
            carry = CPU->RegL & 0x80;
            CPU->RegL <<= 1;
            CPU->RegF = (carry ? 0x10 : 0x00) | (CPU->RegL == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x26): { //SLA (HL)
            carry = MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) & 0x80;
            MMUWrite(MMU, (CPU->RegH << 8 | CPU->RegL), MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) << 1);
            CPU->RegF = (carry ? 0x10 : 0x00) | (MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) == 0 ? 0x80 : 0x00);
            return 16;
        }
        case (0x27): { //SLA A
            carry = CPU->RegA & 0x80;
            CPU->RegA <<= 1;
            CPU->RegF = (carry ? 0x10 : 0x00) | (CPU->RegA == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x28): { //SRA B
            carry = CPU->RegB & 0x01;
            CPU->RegB = (CPU->RegB & 0x80) | (CPU->RegB >> 1);
            CPU->RegF = (carry ? 0x10 : 0x00) | (CPU->RegB == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x29): { //SRA C
            carry = CPU->RegC & 0x01;
            CPU->RegC = (CPU->RegC & 0x80) | (CPU->RegC >> 1);
            CPU->RegF = (carry ? 0x10 : 0x00) | (CPU->RegC == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x2A): { //SRA D
            carry = CPU->RegD & 0x01;
            CPU->RegD = (CPU->RegD & 0x80) | (CPU->RegD >> 1);
            CPU->RegF = (carry ? 0x10 : 0x00) | (CPU->RegD == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x2B): { //SRA E
            carry = CPU->RegE & 0x01;
            CPU->RegE = (CPU->RegE & 0x80) | (CPU->RegE >> 1);
            CPU->RegF = (carry ? 0x10 : 0x00) | (CPU->RegE == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x2C): { //SRA H
            carry = CPU->RegH & 0x01;
            CPU->RegH = (CPU->RegH & 0x80) | (CPU->RegH >> 1);
            CPU->RegF = (carry ? 0x10 : 0x00) | (CPU->RegH == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x2D): { //SRA L
            carry = CPU->RegL & 0x01;
            CPU->RegL = (CPU->RegL & 0x80) | (CPU->RegL >> 1);
            CPU->RegF = (carry ? 0x10 : 0x00) | (CPU->RegL == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x2E): { //SRA (HL)
            carry = MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) & 0x01;
            MMUWrite(MMU, (CPU->RegH << 8 | CPU->RegL), (MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) & 0x80) | (MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) >> 1));
            CPU->RegF = (carry ? 0x10 : 0x00) | (MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) == 0 ? 0x80 : 0x00);
            return 16;
        }
        case (0x2F): { //SRA A
            carry = CPU->RegA & 0x01;
            CPU->RegA = (CPU->RegA & 0x80) | (CPU->RegA >> 1);
            CPU->RegF = (carry ? 0x10 : 0x00) | (CPU->RegA == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x30): { //SWAP B
            CPU->RegB = ((CPU->RegB & 0x0F) << 4) | ((CPU->RegB & 0xF0) >> 4);
            CPU->RegF = (CPU->RegB == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x31): { //SWAP C
            CPU->RegC = ((CPU->RegC & 0x0F) << 4) | ((CPU->RegC & 0xF0) >> 4);
            CPU->RegF = (CPU->RegC == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x32): { //SWAP D
            CPU->RegD = ((CPU->RegD & 0x0F) << 4) | ((CPU->RegD & 0xF0) >> 4);
            CPU->RegF = (CPU->RegD == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x33): { //SWAP E
            CPU->RegE = ((CPU->RegE & 0x0F) << 4) | ((CPU->RegE & 0xF0) >> 4);
            CPU->RegF = (CPU->RegE == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x34): { //SWAP H
            CPU->RegH = ((CPU->RegH & 0x0F) << 4) | ((CPU->RegH & 0xF0) >> 4);
            CPU->RegF = (CPU->RegH == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x35): { //SWAP L
            CPU->RegL = ((CPU->RegL & 0x0F) << 4) | ((CPU->RegL & 0xF0) >> 4);
            CPU->RegF = (CPU->RegL == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x36): { //SWAP (HL)
            uint8_t value = MMURead(MMU, (CPU->RegH << 8 | CPU->RegL));
            value = ((value & 0x0F) << 4) | ((value & 0xF0) >> 4);
            MMUWrite(MMU, (CPU->RegH << 8 | CPU->RegL), value);
            CPU->RegF = (value == 0 ? 0x80 : 0x00);
            return 16;
        }
        case (0x37): { //SWAP A
            CPU->RegA = ((CPU->RegA & 0x0F) << 4) | ((CPU->RegA & 0xF0) >> 4);
            CPU->RegF = (CPU->RegA == 0 ? 0x80 : 0x00);
            return 8;
        }
        case (0x38): { //SRL B
            if (CPU->RegB & 0x01) {
                CPU->RegF |= 0x10; //Set Carry Flag
            }
            else {
                CPU->RegF &= ~0x10; //Clear Carry Flag
            }

            CPU->RegB >>= 1;

            if (CPU->RegB == 0) {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            else {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }

            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF &= ~0x20; //Clear Half Carry Flag

            return 8;
        }
        case (0x39): { //SRL C
            if (CPU->RegC & 0x01) {
                CPU->RegF |= 0x10; //Set Carry Flag
            }
            else {
                CPU->RegF &= ~0x10; //Clear Carry Flag
            }

            CPU->RegC >>= 1;

            if (CPU->RegC == 0) {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            else {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }

            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF &= ~0x20; //Clear Half Carry Flag

            return 8;
        }
        case (0x3A): { //SRL D
            if (CPU->RegD & 0x01) {
                CPU->RegF |= 0x10; //Set Carry Flag
            }
            else {
                CPU->RegF &= ~0x10; //Clear Carry Flag
            }

            CPU->RegD >>= 1;

            if (CPU->RegD == 0) {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            else {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }

            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF &= ~0x20; //Clear Half Carry Flag

            return 8;
        }
        case (0x3B): { //SRL E
            if (CPU->RegE & 0x01) {
                CPU->RegF |= 0x10; //Set Carry Flag
            }
            else {
                CPU->RegF &= ~0x10; //Clear Carry Flag
            }

            CPU->RegE >>= 1;

            if (CPU->RegE == 0) {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            else {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }

            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF &= ~0x20; //Clear Half Carry Flag

            return 8;
        }
        case (0x3C): { //SRL H
            if (CPU->RegH & 0x01) {
                CPU->RegF |= 0x10; //Set Carry Flag
            }
            else {
                CPU->RegF &= ~0x10; //Clear Carry Flag
            }

            CPU->RegH >>= 1;

            if (CPU->RegH == 0) {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            else {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }

            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF &= ~0x20; //Clear Half Carry Flag

            return 8;
        }
        case (0x3D): { //SRL L
            if (CPU->RegL & 0x01) {
                CPU->RegF |= 0x10; //Set Carry Flag
            }
            else {
                CPU->RegF &= ~0x10; //Clear Carry Flag
            }

            CPU->RegL >>= 1;

            if (CPU->RegL == 0) {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            else {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }

            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF &= ~0x20; //Clear Half Carry Flag

            return 8;
        }
        case (0x3E): { //SRL (HL)
            if (MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) & 0x01) {
                CPU->RegF |= 0x10; //Set Carry Flag
            }
            else {
                CPU->RegF &= ~0x10; //Clear Carry Flag
            }

            MMUWrite(MMU, (CPU->RegH << 8 | CPU->RegL), MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) >> 1);

            if (MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) == 0) {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            else {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }

            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF &= ~0x20; //Clear Half Carry Flag

            return 16;
        }
        case (0x3F): { //SRL A
            if (CPU->RegA & 0x01) {
                CPU->RegF |= 0x10; //Set Carry Flag
            }
            else {
                CPU->RegF &= ~0x10; //Clear Carry Flag
            }

            CPU->RegA >>= 1;

            if (CPU->RegA == 0) {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            else {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }

            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF &= ~0x20; //Clear Half Carry Flag

            return 8;
        }
        case (0x40): { //BIT 0, B
            if (CPU->RegB & 0x01) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x41): { //BIT 0, C
            if (CPU->RegC & 0x01) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x42): { //BIT 0, D
            if (CPU->RegD & 0x01) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x43): { //BIT 0, E
            if (CPU->RegE & 0x01) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x44): { //BIT 0, H
            if (CPU->RegH & 0x01) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x45): { //BIT 0, L
            if (CPU->RegL & 0x01) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x46): { //BIT 0, (HL)
            if (MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) & 0x01) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 12;
        }
        case (0x47): { //BIT 0, A
            if (CPU->RegA & 0x01) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x48): { //BIT 1, B
            if (CPU->RegB & 0x02) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x49): { //BIT 1, C
            if (CPU->RegC & 0x02) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x4A): { //BIT 1, D
            if (CPU->RegD & 0x02) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x4B): { //BIT 1, E
            if (CPU->RegE & 0x02) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x4C): { //BIT 1, H
            if (CPU->RegH & 0x02) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x4D): { //BIT 1, L
            if (CPU->RegL & 0x02) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x4E): { //BIT 1, (HL)
            if (MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) & 0x02) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 12;
        }
        case (0x4F): { //BIT 1, A
            if (CPU->RegA & 0x02) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x50): { //BIT 2, B
            if (CPU->RegB & 0x04) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x51): { //BIT 2, C
            if (CPU->RegC & 0x04) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x52): { //BIT 2, D
            if (CPU->RegD & 0x04) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x53): { //BIT 2, E
            if (CPU->RegE & 0x04) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x54): { //BIT 2, H
            if (CPU->RegH & 0x04) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x55): { //BIT 2, L
            if (CPU->RegL & 0x04) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x56): { //BIT 2, (HL)
            if (MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) & 0x04) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 12;
        }
        case (0x57): { //BIT 2, A
            if (CPU->RegA & 0x04) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x58): { //BIT 3, B
            if (CPU->RegB & 0x08) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x59): { //BIT 3, C
            if (CPU->RegC & 0x08) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x5A): { //BIT 3, D
            if (CPU->RegD & 0x08) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x5B): { //BIT 3, E
            if (CPU->RegE & 0x08) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x5C): { //BIT 3, H
            if (CPU->RegH & 0x08) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x5D): { //BIT 3, L
            if (CPU->RegL & 0x08) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x5E): { //BIT 3, (HL)
            if (MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) & 0x08) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 12;
        }
        case (0x5F): { //BIT 3, A
            if (CPU->RegA & 0x08) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x60): { //BIT 4, B
            if (CPU->RegB & 0x10) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x61): { //BIT 4, C
            if (CPU->RegC & 0x10) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x62): { //BIT 4, D
            if (CPU->RegD & 0x10) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x63): { //BIT 4, E
            if (CPU->RegE & 0x10) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x64): { //BIT 4, H
            if (CPU->RegH & 0x10) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x65): { //BIT 4, L
            if (CPU->RegL & 0x10) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x66): { //BIT 4, (HL)
            if (MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) & 0x10) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 12;
        }
        case (0x67): { //BIT 4, A
            if (CPU->RegA & 0x10) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x68): { //BIT 5, B
            if (CPU->RegB & 0x20) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x69): { //BIT 5, C
            if (CPU->RegC & 0x20) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x6A): { //BIT 5, D
            if (CPU->RegD & 0x20) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x6B): { //BIT 5, E
            if (CPU->RegE & 0x20) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x6C): { //BIT 5, H
            if (CPU->RegH & 0x20) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x6D): { //BIT 5, L
            if (CPU->RegL & 0x20) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x6E): { //BIT 5, (HL)
            if (MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) & 0x20) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 12;
        }
        case (0x6F): { //BIT 5, A
            if (CPU->RegA & 0x20) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x70): { //BIT 6, B
            if (CPU->RegB & 0x40) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x71): { //BIT 6, C
            if (CPU->RegC & 0x40) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x72): { //BIT 6, D
            if (CPU->RegD & 0x40) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x73): { //BIT 6, E
            if (CPU->RegE & 0x40) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x74): { //BIT 6, H
            if (CPU->RegH & 0x40) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x75): { //BIT 6, L
            if (CPU->RegL & 0x40) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x76): { //BIT 6, (HL)
            if (MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) & 0x40) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 12;
        }
        case (0x77): { //BIT 6, A
            if (CPU->RegA & 0x40) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x78): { //BIT 7, B
            if (CPU->RegB & 0x80) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x79): { //BIT 7, C
            if (CPU->RegC & 0x80) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x7A): { //BIT 7, D
            if (CPU->RegD & 0x80) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x7B): { //BIT 7, E
            if (CPU->RegE & 0x80) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x7C): { //BIT 7, H
            if (CPU->RegH & 0x80) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x7D): { //BIT 7, L
            if (CPU->RegL & 0x80) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x7E): { //BIT 7, (HL)
            if (MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) & 0x80) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same  

            return 12;
        }
        case (0x7F): { //BIT 7, A
            if (CPU->RegA & 0x80) {
                CPU->RegF &= ~0x80; //Clear Zero Flag
            }
            else {
                CPU->RegF |= 0x80; //Set Zero Flag
            }
            CPU->RegF &= ~0x40; //Clear Sub Flag
            CPU->RegF |= 0x20; //Set Half Carry Flag
            //Keep Carry Flag the same

            return 8;
        }
        case (0x80): { //RES 0, B
            CPU->RegB &= 0xFE;
            return 8;
        }
        case (0x81): { //RES 0, C
            CPU->RegC &= 0xFE;
            return 8;
        }
        case (0x82): { //RES 0, D
            CPU->RegD &= 0xFE;
            return 8;
        }
        case (0x83): { //RES 0, E
            CPU->RegE &= 0xFE;
            return 8;
        }
        case (0x84): { //RES 0, H
            CPU->RegH &= 0xFE;
            return 8;
        }
        case (0x85): { //RES 0, L
            CPU->RegL &= 0xFE;
            return 8;
        }
        case (0x86): { //RES 0, (HL)
            MMUWrite(MMU, (CPU->RegH << 8 | CPU->RegL), (MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) & 0xFE));
            return 16;
        }
        case (0x87): { //RES 0, A
            CPU->RegA &= 0xFE;
            return 8;
        }
        case (0x88): { //RES 1, B
            CPU->RegB &= 0xFD;
            return 8;
        }
        case (0x89): { //RES 1, C
            CPU->RegC &= 0xFD;
            return 8;
        }
        case (0x8A): { //RES 1, D
            CPU->RegD &= 0xFD;
            return 8;
        }
        case (0x8B): { //RES 1, E
            CPU->RegE &= 0xFD;
            return 8;
        }
        case (0x8C): { //RES 1, H
            CPU->RegH &= 0xFD;
            return 8;
        }
        case (0x8D): { //RES 1, L
            CPU->RegL &= 0xFD;
            return 8;
        }
        case (0x8E): { //RES 1, (HL)
            MMUWrite(MMU, (CPU->RegH << 8 | CPU->RegL), MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) & 0xFD);
            return 16;
        }
        case (0x8F): { //RES 1, A
            CPU->RegA &= 0xFD;
            return 8;
        }
        case (0x90): { //RES 2, B
            CPU->RegB &= 0xFB;
            return 8;
        }
        case (0x91): { //RES 2, C
            CPU->RegC &= 0xFB;
            return 8;
        }
        case (0x92): { //RES 2, D
            CPU->RegD &= 0xFB;
            return 8;
        }
        case (0x93): { //RES 2, E
            CPU->RegE &= 0xFB;
            return 8;
        }
        case (0x94): { //RES 2, H
            CPU->RegH &= 0xFB;
            return 8;
        }
        case (0x95): { //RES 2, L
            CPU->RegL &= 0xFB;
            return 8;
        }
        case (0x96): { //RES 2, (HL)
            MMUWrite(MMU, (CPU->RegH << 8 | CPU->RegL), MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) & 0xFB);
            return 16;
        }
        case (0x97): { //RES 2, A
            CPU->RegA &= 0xFB;
            return 8;
        }
        case (0x98): { //RES 3, B
            CPU->RegB &= 0xF7;
            return 8;
        }
        case (0x99): { //RES 3, C
            CPU->RegC &= 0xF7;
            return 8;
        }
        case (0x9A): { //RES 3, D
            CPU->RegD &= 0xF7;
            return 8;
        }
        case (0x9B): { //RES 3, E
            CPU->RegE &= 0xF7;
            return 8;
        }
        case (0x9C): { //RES 3, H
            CPU->RegH &= 0xF7;
            return 8;
        }
        case (0x9D): { //RES 3, L
            CPU->RegL &= 0xF7;
            return 8;
        }
        case (0x9E): { //RES 3, (HL)
            MMUWrite(MMU, (CPU->RegH << 8 | CPU->RegL), MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) & 0xF7);
            return 16;
        }
        case (0x9F): { //RES 3, A
            CPU->RegA &= 0xF7;
            return 8;
        }
        case (0xA0): { //RES 4, B
            CPU->RegB &= 0xEF;
            return 8;
        }
        case (0xA1): { //RES 4, C
            CPU->RegC &= 0xEF;
            return 8;
        }
        case (0xA2): { //RES 4, D
            CPU->RegD &= 0xEF;
            return 8;
        }
        case (0xA3): { //RES 4, E
            CPU->RegE &= 0xEF;
            return 8;
        }
        case (0xA4): { //RES 4, H
            CPU->RegH &= 0xEF;
            return 8;
        }
        case (0xA5): { //RES 4, L
            CPU->RegL &= 0xEF;
            return 8;
        }
        case (0xA6): { //RES 4, (HL)
            MMUWrite(MMU, (CPU->RegH << 8 | CPU->RegL), MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) & 0xEF);
            return 16;
        }
        case (0xA7): { //RES 4, A
            CPU->RegA &= 0xEF;
            return 8;
        }
        case (0xA8): { //RES 5, B
            CPU->RegB &= 0xDF;
            return 8;
        }
        case (0xA9): { //RES 5, C
            CPU->RegC &= 0xDF;
            return 8;
        }
        case (0xAA): { //RES 5, D
            CPU->RegD &= 0xDF;
            return 8;
        }
        case (0xAB): { //RES 5, E
            CPU->RegE &= 0xDF;
            return 8;
        }
        case (0xAC): { //RES 5, H
            CPU->RegH &= 0xDF;
            return 8;
        }
        case (0xAD): { //RES 5, L
            CPU->RegL &= 0xDF;
            return 8;
        }
        case (0xAE): { //RES 5, (HL)
            MMUWrite(MMU, (CPU->RegH << 8 | CPU->RegL), MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) & 0xDF);
            return 16;
        }
        case (0xAF): { //RES 5, A
            CPU->RegA &= 0xDF;
            return 8;
        }
        case (0xB0): { //RES 6, B
            CPU->RegB &= 0xBF;
            return 8;
        }
        case (0xB1): { //RES 6, C
            CPU->RegC &= 0xBF;
            return 8;
        }
        case (0xB2): { //RES 6, D
            CPU->RegD &= 0xBF;
            return 8;
        }
        case (0xB3): { //RES 6, E
            CPU->RegE &= 0xBF;
            return 8;
        }
        case (0xB4): { //RES 6, H
            CPU->RegH &= 0xBF;
            return 8;
        }
        case (0xB5): { //RES 6, L
            CPU->RegL &= 0xBF;
            return 8;
        }
        case (0xB6): { //RES 6, (HL)
            MMUWrite(MMU, (CPU->RegH << 8 | CPU->RegL), MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) & 0xBF);
            return 16;
        }
        case (0xB7): { //RES 6, A
            CPU->RegA &= 0xBF;
            return 8;
        }
        case (0xB8): { //RES 7, B
            CPU->RegB &= 0x7F;
            return 8;
        }
        case (0xB9): { //RES 7, C
            CPU->RegC &= 0x7F;
            return 8;
        }
        case (0xBA): { //RES 7, D
            CPU->RegD &= 0x7F;
            return 8;
        }
        case (0xBB): { //RES 7, E
            CPU->RegE &= 0x7F;
            return 8;
        }
        case (0xBC): { //RES 7, H
            CPU->RegH &= 0x7F;
            return 8;
        }
        case (0xBD): { //RES 7, L
            CPU->RegL &= 0x7F;
            return 8;
        }
        case (0xBE): { //RES 7, (HL)
            MMUWrite(MMU, (CPU->RegH << 8 | CPU->RegL), MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) & 0x7F);
            return 16;
        }
        case (0xBF): { //RES 7, A
            CPU->RegA &= 0x7F;
            return 8;
        }
        case (0xC0): { //SET 0, B
            CPU->RegB |= 0x01;
            return 8;
        }
        case (0xC1): { //SET 0, C
            CPU->RegC |= 0x01;
            return 8;
        }
        case (0xC2): { //SET 0, D
            CPU->RegD |= 0x01;
            return 8;
        }
        case (0xC3): { //SET 0, E
            CPU->RegE |= 0x01;
            return 8;
        }
        case (0xC4): { //SET 0, H
            CPU->RegH |= 0x01;
            return 8;
        }
        case (0xC5): { //SET 0, L
            CPU->RegL |= 0x01;
            return 8;
        }
        case (0xC6): { //SET 0, (HL)
            MMUWrite(MMU, (CPU->RegH << 8 | CPU->RegL), MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) | 0x01);
            return 16;
        }
        case (0xC7): { //SET 0, A
            CPU->RegA |= 0x01;
            return 8;
        }
        case (0xC8): { //SET 1, B
            CPU->RegB |= 0x02;
            return 8;
        }
        case (0xC9): { //SET 1, C
            CPU->RegC |= 0x02;
            return 8;
        }
        case (0xCA): { //SET 1, D
            CPU->RegD |= 0x02;
            return 8;
        }
        case (0xCB): { //SET 1, E
            CPU->RegE |= 0x02;
            return 8;
        }
        case (0xCC): { //SET 1, H
            CPU->RegH |= 0x02;
            return 8;
        }
        case (0xCD): { //SET 1, L
            CPU->RegL |= 0x02;
            return 8;
        }
        case (0xCE): { //Set 1, (HL)
            MMUWrite(MMU, (CPU->RegH << 8 | CPU->RegL), MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) | 0x02);
            return 16;
        }
        case (0xCF): { //Set 1, A
            CPU->RegA |= 0x02;
            return 8;
        }
        case (0xD0): { //Set 2, B
            CPU->RegB |= 0x04;
            return 8;
        }
        case (0xD1): { //Set 2, C
            CPU->RegC |= 0x04;
            return 8;
        }
        case (0xD2): { //Set 2, D
            CPU->RegD |= 0x04;
            return 8;
        }
        case (0xD3): { //Set 2, E
            CPU->RegE |= 0x04;
            return 8;
        }
        case (0xD4): { //Set 2, H
            CPU->RegH |= 0x04;
            return 8;
        }
        case (0xD5): { //Set 2, L
            CPU->RegL |= 0x04;
            return 8;
        }
        case (0xD6): { //Set 2, (HL)
            MMUWrite(MMU, (CPU->RegH << 8 | CPU->RegL), MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) | 0x04);
            return 16;
        }
        case (0xD7): { //Set 2, A
            CPU->RegA |= 0x04;
            return 8;
        }
        case (0xD8): { //Set 3, B
            CPU->RegB |= 0x08;
            return 8;
        }
        case (0xD9): { //Set 3, C
            CPU->RegC |= 0x08;
            return 8;
        }
        case (0xDA): { //Set 3, D
            CPU->RegD |= 0x08;
            return 8;
        }
        case (0xDB): { //Set 3, E
            CPU->RegE |= 0x08;
            return 8;
        }
        case (0xDC): { //Set 3, H
            CPU->RegH |= 0x08;
            return 8;
        }
        case (0xDD): { //Set 3, L
            CPU->RegL |= 0x08;
            return 8;
        }
        case (0xDE): { //Set 3, (HL)
            MMUWrite(MMU, (CPU->RegH << 8 | CPU->RegL), MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) | 0x08);
            return 16;
        }
        case (0xDF): { //Set 3, A
            CPU->RegA |= 0x08;
            return 8;
        }
        case (0xE0): { //Set 4, B
            CPU->RegB |= 0x10;
            return 8;
        }
        case (0xE1): { //Set 4, C
            CPU->RegC |= 0x10;
            return 8;
        }
        case (0xE2): { //Set 4, D
            CPU->RegD |= 0x10;
            return 8;
        }
        case (0xE3): { //Set 4, E
            CPU->RegE |= 0x10;
            return 8;
        }
        case (0xE4): { //Set 4, H
            CPU->RegH |= 0x10;
            return 8;
        }
        case (0xE5): { //Set 4, L
            CPU->RegL |= 0x10;
            return 8;
        }
        case (0xE6): { //Set 4, (HL)
            MMUWrite(MMU, (CPU->RegH << 8 | CPU->RegL), MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) | 0x10);
            return 16;
        }
        case (0xE7): { //Set 4, A
            CPU->RegA |= 0x10;
            return 8;
        }
        case (0xE8): { //Set 5, B
            CPU->RegB |= 0x20;
            return 8;
        }
        case (0xE9): { //Set 5, C
            CPU->RegC |= 0x20;
            return 8;
        }
        case (0xEA): { //Set 5, D
            CPU->RegD |= 0x20;
            return 8;
        }
        case (0xEB): { //Set 5, E
            CPU->RegE |= 0x20;
            return 8;
        }
        case (0xEC): { //Set 5, H
            CPU->RegH |= 0x20;
            return 8;
        }
        case (0xED): { //Set 5, L
            CPU->RegL |= 0x20;
            return 8;
        }
        case (0xEE): { //Set 5, (HL)
            MMUWrite(MMU, (CPU->RegH << 8 | CPU->RegL), MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) | 0x20);
            return 16;
        }
        case (0xEF): { //Set 5, A
            CPU->RegA |= 0x20;
            return 8;
        }
        case (0xF0): { //Set 6, B
            CPU->RegB |= 0x40;
            return 8;
        }
        case (0xF1): { //Set 6, C
            CPU->RegC |= 0x40;
            return 8;
        }
        case (0xF2): { //Set 6, D
            CPU->RegD |= 0x40;
            return 8;
        }
        case (0xF3): { //Set 6, E
            CPU->RegE |= 0x40;
            return 8;
        }
        case (0xF4): { //Set 6, H
            CPU->RegH |= 0x40;
            return 8;
        }
        case (0xF5): { //Set 6, L
            CPU->RegL |= 0x40;
            return 8;
        }
        case (0xF6): { //Set 6, (HL)
            MMUWrite(MMU, (CPU->RegH << 8 | CPU->RegL), MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) | 0x40);
            return 16;
        }
        case (0xF7): { //Set 6, A
            CPU->RegA |= 0x40;
            return 8;
        }
        case (0xF8): { //Set 7, B
            CPU->RegB |= 0x80;
            return 8;
        }
        case (0xF9): { //Set 7, C
            CPU->RegC |= 0x80;
            return 8;
        }
        case (0xFA): { //Set 7, D
            CPU->RegD |= 0x80;
            return 8;
        }
        case (0xFB): { //Set 7, E
            CPU->RegE |= 0x80;
            return 8;
        }
        case (0xFC): { //Set 7, H
            CPU->RegH |= 0x80;
            return 8;
        }
        case (0xFD): { //Set 7, L
            CPU->RegL |= 0x80;
            return 8;
        }
        case (0xFE): { //Set 7, (HL)
            MMUWrite(MMU, (CPU->RegH << 8 | CPU->RegL), MMURead(MMU, (CPU->RegH << 8 | CPU->RegL)) | 0x80);
            return 16;
        }
        case (0xFF): { //Set 7, A
            CPU->RegA |= 0x80;
            return 8;
        }
        default: {
            printf("Unimplemented CB Opcode: 0xCB%X\n", opcode);
            return 8;
        }
    }
    return 8;
}


void CPUInit(CPU *CPU) {
        //Registers
    CPU->RegA = 0x01;
    CPU->RegF = 0xB0;
    
    CPU->RegB = 0x00;
    CPU->RegC = 0x13;
    
    CPU->RegD = 0x00;
    CPU->RegE  = 0xD8;

    CPU->RegH = 0x01;
    CPU->RegL = 0x4D;

    CPU->SP = 0xfffe;
    CPU->PC = 0x0100;

    // Flags
    CPU->HALT = 0;
    CPU->IME = 0; // Interrupt Master Enable Flag
	
	CPU->LOG = LOG;
}

//For Debugging
void CPULOG(CPU *CPU, MMU *MMU) {
    FILE *logfile = fopen("log.log", "a");
    // Get PC memory values
    uint8_t pcMem[4];
    for (int i = 0; i < 4; ++i) {
        pcMem[i] = MMURead(MMU, CPU->PC + i);
    }
    
    fprintf(logfile, "A:%02X F:%02X B:%02X C:%02X D:%02X E:%02X H:%02X L:%02X SP:%04X PC:%04X PCMEM:%02X,%02X,%02X,%02X\n",
            CPU->RegA, CPU->RegF, CPU->RegB, CPU->RegC, CPU->RegD, CPU->RegE, CPU->RegH, CPU->RegL,
            CPU->SP, (CPU->PC), pcMem[0], pcMem[1], pcMem[2], pcMem[3]);
    
    fclose(logfile);
}