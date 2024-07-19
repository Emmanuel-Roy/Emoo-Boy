#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "DMG.h"


//System Globals.
char ROMFilePath[512]; 
char RAMFilePath[512]; 
int DMGPalette[4] = {0xFFFFFF, 0xb6b6b6, 0x676767, 0x000000}; //Default DMG Colors
int ROMSize;
int RAMSize;
int MBCType;
int LoadSaveFile = 0;
int Exit = 0;
int RenderingMode = 1;
int LOG = 0;
//SDL Globals
SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture* texture;
int SCALE = 5;

/*
  I tend to avoid using global variables in my code, after all, it does lead to a variety of performance problems and can hurt maintainability.
  
  However, in this case, I'm using them to store the ROM and Palette information that the user inputs and I'm not going to be changing them at all after the user inputs them.
  
  In addition, I plan to use functions like ROMSize and RAMSize in the creation of arrays, primarily in the MMU Struct, and I feel that it would be easier to use them as global variables in comparison to a lengthy pointer implementation.
  I may revisit this code and implement those structures using pointers, but for now I have decided to use global variables.

  While I could pass them as arguments to the functions that need them, I feel that it would be a bit overkill for this program.
  I also doubt that I will be creating similar variables in each struct or function that I create, so I feel that this is an acceptable use of global variables.
*/


//Used Function for Readability Purposes.
void GetROMInfo();


int main(int argc, char *argv[]) 
{
	int MenuChoice = 0;
	int flag = 0;

	printf("Welcome to Emoo-Boy!\n \n");
	
    while (flag != 1) {
		printf("Please select an option:\n");
		printf("1. Load ROM File\n");
		printf("2. Update Palette\n");
		printf("3. Update Scale Factor\n");
		printf("4. Update Rendering Method\n");
		printf("5. Help/Controls \n");
		printf("6. Toggle CPU Logging \n \n");
		
		scanf("%d", &MenuChoice);
		
		switch(MenuChoice) {
			case (1): {
				GetROMInfo();
				flag = 1;
				break;
			}
			case (2): {
				printf("\nPlease enter all values in hexadecimal, for example use 0xFFFFFF to represent white and 0x000000 to represent black. \n \n");
				printf("Please enter the value you wish to use for Palette Color 0, currently 0x%x. \n", DMGPalette[0]);
				scanf("%x", &DMGPalette[0]);
				printf("Please enter the value you wish to use for Palette Color 1, currently 0x%x. \n", DMGPalette[1]);
				scanf("%x", &DMGPalette[1]);
				printf("Please enter the value you wish to use for Palette Color 2, currently 0x%x. \n", DMGPalette[2]);
				scanf("%x", &DMGPalette[2]);
				printf("Please enter the value you wish to use for Palette Color 3, currently 0x%x. \n", DMGPalette[3]);
				scanf("%x", &DMGPalette[3]);
				printf("Palette Updated! \n \n");
				break;
			}
			case (3): {
				int CurrentHeight = 160 * SCALE;
				int CurrentWidth = 144 * SCALE;
				
				printf("\nThe current scale factor is %d. \n", SCALE);
				printf("The current window size is %d x %d. \n", CurrentWidth, CurrentHeight);

				printf("Please enter the new scale factor value. \n \n");
				scanf("%d", &SCALE);
				
				CurrentHeight = 160 * SCALE;
				CurrentWidth = 144 * SCALE;
				
				printf("\nScale Updated! \n");
				printf("The new window size is %d x %d. \n \n", CurrentWidth, CurrentHeight);

				break;
			}
			case (4): {
				printf("\nEnter 0 for Per Pixel Rendering (VERY SLOW): \n");
				printf("\nEnter 1 for Scanline Rendering (HBLANK) (SLOW): \n");
				printf("\nEnter 2 for Frame Rendering (VBLANK) (FAST): \n \n");

				scanf("%d", &RenderingMode);

				printf("\n");
				break;

			}
			case (5): {
				printf("\nThe controls for the emulator are as follows: \n");
				printf("Arrow Keys: D-Pad\n");
				printf("Z: A Button\n");
				printf("X: B Button\n");
				printf("C: Start Button\n");
				printf("D: Select Button\n \n");

				printf("For any inquires, please contact the developer: royemmanuel39@gmail.com \n");

				break;
			}
			case (6): {
				if (LOG == 0) {
					printf("CPU Logging Enabled \n");
					LOG = 1;
				}
				else if (LOG == 1) {
					printf("CPU Logging Disabled \n");
					LOG = 0;
				}
				break;
			}
			default: {
				printf("\nInput Invalid, please try again. \n");
				break;
			}
		}
	}

	//Create Gameboy Struct;
	DMG Gameboy;
	//Run Gameboy Init
	DMGInit(&Gameboy);

	//Main Loop
	while (!Exit) { //SDL Scancode quit
		DMGTick(&Gameboy);
	}
	
	// On Program Exit
	if (LoadSaveFile == 1) {
		MMUSaveFile(&Gameboy.DMG_MMU);
	}
	
	// Close SDL
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	// Free MMU Memory
	MMUFree(&Gameboy.DMG_MMU);


    return EXIT_SUCCESS;
};


void GetROMInfo() {
	char ROMName[256]; //I doubt a ROM name will be larger than 256 chars
	char RAMName[256]; 
	int FileSize;
	unsigned char ramSizeByte;
	unsigned char romSizeByte;
	unsigned char MBCByte;
	int RAMChoice = 0;

    printf("\nPlease enter the ROM file name: \n");
    printf("Note that all ROM files MUST be located in the ROM folder of this emulator. \n");
	printf("PLEASE REMEMBER TO INCLUDE THE FILE EXTENSION!!! (the .gb) \n \n");
    // Flush the input buffer
    int FlushInput;
    while ((FlushInput = getchar()) != '\n' && FlushInput != EOF);

    fgets(ROMName, sizeof(ROMName), stdin);
	
    // Remove the newline character
    ROMName[strcspn(ROMName, "\n")] = 0;
        
    snprintf(ROMFilePath, sizeof(ROMFilePath), "ROM/%s", ROMName);

    FILE *romFile = fopen(ROMFilePath, "rb");
    
	if (romFile == NULL) {
        printf("Error: Could not open ROM file %s\n", ROMFilePath);
        return;
    }



    // find the length of the file. (ROM Size)
    fseek(romFile, 0, SEEK_END);
    FileSize = ftell(romFile);

	//find the rom size
	fseek(romFile, 0x148, SEEK_SET);
	fread(&romSizeByte, sizeof(unsigned char), 1, romFile);   
	switch (romSizeByte) {
	case (0x00): //32KB (2 Banks)
		ROMSize = 32768;
		break;
	case (0x01): //64KB (4 Banks)
		ROMSize = 65536;
		break;	
	case (0x02): //128KB (8 Banks)
		ROMSize = 131072;
		break;
	case (0x03): //256KB (16 Banks)
		ROMSize = 262144;
		break;
	case (0x04): //512KB (32 Banks)
		ROMSize = 524288;
		break;
	case (0x05): //1MB (64 Banks)
		ROMSize = 1048576;
		break;
	case (0x06): //2MB (128 Banks)
		ROMSize = 2097152;
		break;
	case (0x07): //4MB (256 Banks)
		ROMSize = 4194304;
		break;
	case (0x08):
		ROMSize = 8388608; //This is 8MB, the largest commericially avaiable ROM size for a Gameboy ROM. 
		//In all honesty, running a ROM like this on my emulator would be a performance nightmare.
		break;
	case (0x52): //72 banks (I'm going to assume this is 1MB + 128KB)
		ROMSize = 1048576 + 131072;
		break;
	case (0x53): //80 banks (I'm going to assume this is 1MB + 256KB)
		ROMSize = 1048576 + 262144;
		break;
	case (0x54): //96 banks (I'm going to assume this is 1MB + 512KB)
		ROMSize = 1048576 + 524288;
		break;
	default:
		ROMSize = 32768;
		break;
	} 

	//find the ram size
	fseek(romFile, 0x149, SEEK_SET);
	fread(&ramSizeByte, sizeof(unsigned char), 1, romFile);
	
	switch (ramSizeByte) {
		case (0x00):
			RAMSize = 0;
			break;
		case (0x01):
			RAMSize = 0;
			break;
		case (0x02):
			RAMSize = 8192;
			break;
		case (0x03):
			RAMSize = 32768;
			break;
		case (0x04):
			RAMSize = 131072;
			break;
		case (0x05):
			RAMSize = 65536;
			break;
		default:
			RAMSize = 0;
			break;
	}

	//find the MBC type
	fseek(romFile, 0x147, SEEK_SET);
	fread(&MBCByte, sizeof(unsigned char), 1, romFile);
	MBCType = MBCByte;
	fclose(romFile);

	if (RAMSize > 0) {
		printf("\nThis ROM has a Save file associated with it. \n");
		printf("Would you like to load in a Save file? \n");
		printf("If you do not, your game will NOT be saved.\n");
		printf("1. Yes \n");
		printf("2. No \n \n");

		scanf("%d", &LoadSaveFile);

		if (LoadSaveFile == 1) {
			printf("\nPlease enter the Save file name: \n");
			printf("Note that all Save files MUST be located in the Battery folder of this emulator. \n");
			printf("PLEASE REMEMBER TO INCLUDE THE FILE EXTENSION!!! (the .sav) \n \n");
			// Flush the input buffer
			int FlushInput;
			while ((FlushInput = getchar()) != '\n' && FlushInput != EOF);

			fgets(RAMName, sizeof(RAMName), stdin);
			
			// Remove the newline character
			RAMName[strcspn(RAMName, "\n")] = 0;

			snprintf(RAMFilePath, sizeof(RAMFilePath), "Battery/%s", RAMName);

			printf("\nSave File Loaded! \n");
		}
		else {
			printf("\nNo Save file loaded, proceeding with program. \n");
		}
	}

    printf("\nROM Info for %s \n \n", ROMName);

    printf("File Size: %d bytes\n", FileSize);
	printf("ROM Size: %d bytes\n", ROMSize);
	printf("RAM Size: %d bytes\n", RAMSize);
	printf("MBC Type: 0x%x \n \n", MBCType);

	printf("Note: The MBCType Value is the hexademical value of the MBC Type, please reference the PanDocs to confirm accuracy. \n");
	printf("Note: Not all MBC Features are implemented yet, please remember to confirm that the File Size and ROM Size are the same. \n");

	if (RAMChoice == 1) {
		printf("Note: The Save File has been loaded, and will be updated when you choose to exit the emulator. \n");
	}

	/*Files Located at
	printf("%s\n\n", ROMFilePath);
	printf("%s\n\n", RAMFilePath); 
	*/
    // Pause Program
	printf("\nPress Enter to Start Game...\n");

    getchar();
}

