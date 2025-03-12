#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "APU.h"    
#include "MMU.h"
#include <math.h>

extern SDL_AudioSpec audio;
extern SDL_AudioSpec audio2;
extern SDL_AudioDeviceID audioDevice;

//Many thanks to the following resources for helping me understand the Gameboy's APU:
//https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware
//Pan Docs: https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware
//GhostBoy: https://github.com/GhostSonic21/GhostBoy/blob/68a6b5ccddf3b6b3216681c47b615e66e0c7a968/GhostBoy/APU.cpp
//emuboy: https://github.com/jacoblister/emuboy/blob/master/src/apu.ts#L42

//1 CPU Cycle = 4 APU Tick

void APUTick(APU *APU, MMU *MMU) {
    //Update Internal Variables
    APUUpdate(APU, MMU);

    //Check if APU is turned on or off
    if ((APU->NR52 & 0x80) == 0) {
        //APU is off, and we should clear all of the registers and channels.
        APUInit(APU, MMU);
        //Keep APU off
        APU->NR52 = APU->NR52 & ~0x80; //Set bit 7 to 0, as the APU is still off.
        return;
    }
    //Otherwise, continue with the APU logic.

    //Frame Sequencer (Every 8192 CPU Cycles or every 32,768 APU "Ticks") 
    if (APU->FrameSequencerCounter == 0) {
        APU->FrameSequencerCounter = 8192; // Reset Frame Sequencer Counter

        switch (APU->FrameSequencerStep) {
            case 0:
            case 4:
                // Length Counter Clock for Channels
                APULengthCLK(APU, MMU);
                break;
            case 2:
            case 6:
                // Length Counter and Sweep Clock
                APUSweepCLK(APU, MMU);
                APULengthCLK(APU, MMU);
                break;
            case 7:
                // Clock the Envelope for all channels 
                APUEnvelopeCLK(APU, MMU);
                break;
        }
        APU->FrameSequencerStep++;
        if (APU->FrameSequencerStep == 8) {
            APU->FrameSequencerStep = 0;
        }
    } 
    else {
        APU->FrameSequencerCounter--;
    }
    //Tick channels
    APUPulseWithSweepTick(APU, MMU);
    APUPulseTick(APU, MMU);
    APUWaveTick(APU, MMU);
    APUNoiseTick(APU, MMU);

    //Update NR52 based on what channels are on or off
    APU->NR52 = APU->NR52 | 0x80; //Set bit 7 to 1, as the APU is on.
    APU->NR52 &= ~0x0F; //Clear bits 0-3
    //Update based on which channels are on or off.
    if (APU->PulseWithSweep.ChannelOn == 1) {
        APU->NR52 = APU->NR52 | 0x01; //Set bit 0 to 1, as the Pulse with Sweep channel is on.
    }
    if (APU->Pulse.ChannelOn == 1) {
        APU->NR52 = APU->NR52 | 0x02; //Set bit 1 to 1, as the Pulse channel is on.
    }
    if (APU->Wave.ChannelOn == 1) {
        APU->NR52 = APU->NR52 | 0x04; //Set bit 2 to 1, as the Wave channel is on.
    }
    if (APU->Noise.ChannelOn == 1) {
        APU->NR52 = APU->NR52 | 0x08; //Set bit 3 to 1, as the Noise channel is on.
    }

    //Update NR52's data in System Memory
    MMU->SystemMemory[0xFF26] = APU->NR52;

    //Make Master Samples
    APU->MasterSampleLeft = 0;
    APU->MasterSampleRight = 0;

    //Mix Channels

    if ((APU->NR51 & 0x01) == 0x01) {
        APU->MasterSampleRight = APU->MasterSampleRight + APU->PulseWithSweep.Sample;
    }
    if ((APU->NR51 & 0x10) == 0x10) {
        APU->MasterSampleLeft = APU->MasterSampleLeft + APU->PulseWithSweep.Sample;
    }
    if ((APU->NR51 & 0x02) == 0x02) {
        APU->MasterSampleRight = APU->MasterSampleRight + APU->Pulse.Sample;
    }
    if ((APU->NR51 & 0x20) == 0x20) {
        APU->MasterSampleLeft = APU->MasterSampleLeft + APU->Pulse.Sample;
    }
    if ((APU->NR51 & 0x04) == 0x04) { 
        APU->MasterSampleRight = APU->MasterSampleRight + APU->Wave.Sample;
    }
    if ((APU->NR51 & 0x40) == 0x40) {
        APU->MasterSampleLeft = APU->MasterSampleLeft + APU->Wave.Sample;
    }
    
    if ((APU->NR51 & 0x08) == 0x08) { //If the channel is enabled
        APU->MasterSampleRight = APU->MasterSampleRight + APU->Noise.Sample;
    }
    if ((APU->NR51 & 0x80) == 0x80) { 
        APU->MasterSampleLeft = APU->MasterSampleLeft + APU->Noise.Sample;
    }

    //Send to SDL Audio Buffer
    SDLPlayAudio(APU, MMU);
}

//Set the MMU Values to the Default Values
void APUInit(APU *APU, MMU *MMU) {
    //Adjust MMU values
    //APU Registers
    MMU->SystemMemory[0xFF24] = 0x77; //NR50
    MMU->SystemMemory[0xFF25] = 0xF3; //NR51
    MMU->SystemMemory[0xFF26] = 0xF1; //NR52

    //Pulse with Sweep Registers
    MMU->SystemMemory[0xFF10] = 0x00; //NR10
    MMU->SystemMemory[0xFF11] = 0x00; //NR11
    MMU->SystemMemory[0xFF12] = 0x00; //NR12
    MMU->SystemMemory[0xFF13] = 0x00; //NR13
    MMU->SystemMemory[0xFF14] = 0x00; //NR14

    //Pulse Registers
    MMU->SystemMemory[0xFF16] = 0x00; //NR21
    MMU->SystemMemory[0xFF17] = 0x00; //NR22
    MMU->SystemMemory[0xFF18] = 0x00; //NR23
    MMU->SystemMemory[0xFF19] = 0x00; //NR24

    //Wave Registers
    MMU->SystemMemory[0xFF1A] = 0x00; //NR30
    MMU->SystemMemory[0xFF1B] = 0x00; //NR31
    MMU->SystemMemory[0xFF1C] = 0x00; //NR32
    MMU->SystemMemory[0xFF1D] = 0x00; //NR33
    MMU->SystemMemory[0xFF1E] = 0x00; //NR34

    for (int i = 0xFF30; i <= 0xFF3F; i++) {
        MMU->SystemMemory[i] = 0x00; //Wave Pattern RAM
    }

    //Noise Registers
    MMU->SystemMemory[0xFF20] = 0x00; //NR41
    MMU->SystemMemory[0xFF21] = 0x00; //NR42
    MMU->SystemMemory[0xFF22] = 0x00; //NR43
    MMU->SystemMemory[0xFF23] = 0x00; //NR44

    //Adjust Internal Samples
    APU->PulseWithSweep.Sample = 0;
    APU->Pulse.Sample = 0;
    APU->Wave.Sample = 0;
    APU->Noise.Sample = 0;

    APU->MasterSampleLeft = 0;
    APU->MasterSampleRight = 0;
    APU->FrameSequencerStep = 0;
    APU->FrameSequencerCounter = 8192;
    APU->CurrentSample = 0;
    APU->Ticks = 0;
}

//Update APU Internal Variables with MMU Values, check for changes in registers, and trigger channels if needed.
void APUUpdate(APU *APU, MMU *MMU) {
    //Update APU Internal Variables with MMU Values
    //A bit of a waste of performance, but increases readability by a ton.
    //In Addtion, this allows us to cover APU Reading and Writting changes.
    //If some MMU values are written, we can have it do things like increase the volume and trigger channels.

    //APU Registers
    APU->NR50 = MMU->SystemMemory[0xFF24]; //Master volume & VIN panning
    APU->NR51 = MMU->SystemMemory[0xFF25]; //Sound Panning
    APU->NR52 = MMU->SystemMemory[0xFF26];  //Audio Master Control

    //Pulse with Sweep Registers
    APU->PulseWithSweep.NR10 = MMU->SystemMemory[0xFF10];
    APU->PulseWithSweep.NR11 = MMU->SystemMemory[0xFF11];
    if (APU->PulseWithSweep.NR12 != MMU->SystemMemory[0xFF12]) {
        //Change Volume of channel if different (Means value was written to.)
        if ((MMU->SystemMemory[0xFF12] & 0xF0) == 0) {
            //Channel is disabled, so we should clear the sample.
            APU->PulseWithSweep.Sample = 0;
            APU->PulseWithSweep.ChannelOn = 0; //Turn off the channel.
        }
    }
    APU->PulseWithSweep.NR12 = MMU->SystemMemory[0xFF12]; //Other wise, its the same, and we don't need to do anything.
    APU->PulseWithSweep.NR13 = MMU->SystemMemory[0xFF13];
    if (APU->PulseWithSweep.NR14 != MMU->SystemMemory[0xFF14]) {
        if ((MMU->SystemMemory[0xFF14] & 0x80) == 0x80) {
            //Trigger Channel if different and with bit 7 set (Means value was written to.)
            APUPulseWithSweepTrigger(APU, MMU);
        }
    }
    APU->PulseWithSweep.NR14 = MMU->SystemMemory[0xFF14]; //Other wise, its the same, and we don't need to do anything.

    //Pulse Registers
    APU->Pulse.NR21 = MMU->SystemMemory[0xFF16];
    if (APU->Pulse.NR22 != MMU->SystemMemory[0xFF17]) {
        //Change Volume of channel if different (Means value was written to.)
        if ((MMU->SystemMemory[0xFF17] & 0xF0) == 0) {
            //Channel is disabled, so we should clear the sample.
            APU->Pulse.Sample = 0;
            APU->Pulse.ChannelOn = 0; //Turn off the channel.
        }
    }
    APU->Pulse.NR22 = MMU->SystemMemory[0xFF17]; 
    APU->Pulse.NR23 = MMU->SystemMemory[0xFF18];
    if (APU->Pulse.NR24 != MMU->SystemMemory[0xFF19]) {
        if ((MMU->SystemMemory[0xFF19] & 0x80) == 0x80) {
            //Trigger Channel if different and with bit 7 set (Means value was written to.)
            APUPulseTrigger(APU, MMU);
        }
    }
    APU->Pulse.NR24 = MMU->SystemMemory[0xFF19];

    //Wave Registers
    APU->Wave.NR30 = MMU->SystemMemory[0xFF1A];
    APU->Wave.NR31 = MMU->SystemMemory[0xFF1B];
    APU->Wave.NR32 = MMU->SystemMemory[0xFF1C]; 
    APU->Wave.NR33 = MMU->SystemMemory[0xFF1D];
    if (APU->Wave.NR34 != MMU->SystemMemory[0xFF1E]) {
        if ((MMU->SystemMemory[0xFF1E] & 0x80) == 0x80) {
            //Trigger Channel if different and with bit 7 set (Means value was written to.)
            APUWaveTrigger(APU, MMU);
        }
    }
    APU->Wave.NR34 = MMU->SystemMemory[0xFF1E];
    for (int i = 0xFF30; i <= 0xFF3F; i++) {
        APU->Wave.WavePatternRAM[i - 0xFF30] = MMU->SystemMemory[i];
    }

    //Noise Registers
    APU->Noise.NR41 = MMU->SystemMemory[0xFF20];
    if (APU->Noise.NR42 != MMU->SystemMemory[0xFF21]) {
        //Change Volume of channel if different (Means value was written to.)
        if ((MMU->SystemMemory[0xFF21] & 0xF0) == 0) {
            //Channel is disabled, so we should clear the sample.
            APU->Noise.Sample = 0;
            APU->Noise.ChannelOn = 0; //Turn off the channel.
        }
    }
    APU->Noise.NR42 = MMU->SystemMemory[0xFF21];
    APU->Noise.NR43 = MMU->SystemMemory[0xFF22];
    if (APU->Noise.NR44 != MMU->SystemMemory[0xFF23]) {
        if ((MMU->SystemMemory[0xFF23] & 0x80) == 0x80) {
            //Trigger Channel if different and with bit 7 set (Means value was written to.)
            APUNoiseTrigger(APU, MMU);
        }
    }
    APU->Noise.NR44 = MMU->SystemMemory[0xFF23];
}

//Length CLK, Sweep CLK, Envelope CLK.
void APULengthCLK(APU *APU, MMU *MMU) {
    //Pulse with Sweep
    
    //Pulse
    
    //Wave

    //Noise

    return;
}
void APUSweepCLK(APU *APU, MMU *MMU) {
    //Pulse with Sweep
    
    return;
}
void APUEnvelopeCLK(APU *APU, MMU *MMU) {
    //Pulse with Sweep

    //Pulse

    //Noise
    
    return;
}

//Tick Channels
void APUPulseWithSweepTick(APU *APU, MMU *MMU) {
    return;
}
void APUPulseTick(APU *APU, MMU *MMU) {
    return;
}
void APUWaveTick(APU *APU, MMU *MMU) {
    return;
}

void APUNoiseTick(APU *APU, MMU *MMU) {
    return;
}

//Trigger Channels
void APUPulseWithSweepTrigger(APU *APU, MMU *MMU) {
    return;
}
void APUPulseTrigger(APU *APU, MMU *MMU) {
    return;
}
void APUWaveTrigger(APU *APU, MMU *MMU) {
    return;
}
void APUNoiseTrigger(APU *APU, MMU *MMU) {
    return;
}

//SDL Audio Callback
void SDLPlayAudio(APU *APU, MMU *MMU) {
    // Increase Volume (delete this line if it breaks things)
    APU->MasterSampleLeft *= (APU->NR50 & 0x07);          // Volume for left channel
    APU->MasterSampleRight *= ((APU->NR50 & 0x70) >> 4);  // Volume for right channel


    // Clamp the samples to the maximum and minimum values
    if (APU->MasterSampleLeft > 32767) {
        APU->MasterSampleLeft = 32767;
    }
    if (APU->MasterSampleLeft < -32768) {
        APU->MasterSampleLeft = -32768;
    }
    if (APU->MasterSampleRight > 32767) {
        APU->MasterSampleRight = 32767;
    }
    if (APU->MasterSampleRight < -32768) {
        APU->MasterSampleRight = -32768;
    }

    if (APU->MasterSampleLeft == 0) {
        return;
    }
    if (APU->MasterSampleRight == 0) {
        return;
    }

    // Write samples to buffer
    APU->AudioBuffer[APU->CurrentSample * 2] = APU->MasterSampleLeft;
    APU->AudioBuffer[APU->CurrentSample * 2 + 1] = APU->MasterSampleRight;    

    if (APU->CurrentSample >= 94) {
        SDL_QueueAudio(audioDevice, APU->AudioBuffer, sizeof(APU->AudioBuffer)); 
        //Seems to have some issues actually playing stereo audio, but to be honest, I'm not sure why.
        APU->CurrentSample = 0;
    }
    else {
        APU->CurrentSample++;
        APU->CurrentSample++;
    }
}