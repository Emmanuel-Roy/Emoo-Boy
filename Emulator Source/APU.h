#ifndef APU_H
#define APU_H

#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "MMU.h"

typedef struct {
    /* Audio Registers in MMU
    0xFF10 - NR10 - Channel 1 Sweep Register (R/W)
    0xFF11 - NR11 - Channel 1 Sound length/Duty Cycle (R/W)
    0xFF12 - NR12 - Channel 1 Volume & Envelope (R/W)
    0xFF13 - NR13 - Channel 1 Frequency lo (Write Only)
    0xFF14 - NR14 - Channel 1 Frequency hi & control (R/W)
    */
    uint8_t NR10;
    uint8_t NR11;
    uint8_t NR12;
    uint8_t NR13;
    uint8_t NR14;
    //Volume, Envelope, and Length
    int Volume;
    int EnvelopePeriod;
    int EnvelopeDirection;
    int EnvelopeTimer;
    int LengthCounter;
    //Channel on or off
    int ChannelOn;
    //Audio Sample
    int Sample;
} PulseWithSweepChannel;

typedef struct {
    /* Audio Registers in MMU (Same as Channel 1 without Sweep.)
    0xFF16 - NR21 - Channel 2 Sound Length/Wave Pattern Duty (R/W)
    0xFF17 - NR22 - Channel 2 Volume Envelope (R/W)
    0xFF18 - NR23 - Channel 2 Frequency lo data (Write Only)
    0xFF19 - NR24 - Channel 2 Frequency hi data & control (R/W)
    */
    uint8_t NR21;
    uint8_t NR22;
    uint8_t NR23;
    uint8_t NR24;
    //Volume, Envelope, and Length
    int Volume;
    int EnvelopePeriod;
    int EnvelopeDirection;
    int EnvelopeTimer;
    int LengthCounter;
    //Channel on or off
    int ChannelOn;
    //Audio Sample
    int Sample;
} PulseChannel;

typedef struct {
    /* Audio Registers in MMU
    0xFF1A - NR30 - Channel 3 Sound on/off (R/W)
    0xFF1B - NR31 - Channel 3 Sound Length (Write Only)
    0xFF1C - NR32 - Channel 3 Output Level (R/W)
    0xFF1D - NR33 - Channel 3 Peroid Low (W)
    0xFF1E - NR34 - Channel 3 Peroid High & Control (R/W) 
    0xFF30 - 0xFF3F - Wave Pattern RAM
    */
    uint8_t NR30;
    uint8_t NR31;
    uint8_t NR32;
    uint8_t NR33;
    uint8_t NR34;
    uint8_t WavePatternRAM[16];
    //Volume, Envelope, and Length
    int Volume;
    int EnvelopePeriod;
    int EnvelopeDirection;
    int EnvelopeTimer;
    int LengthCounter;
    //Channel on or off
    int ChannelOn;
    //Audio Sample
    int Sample;
} WaveChannel;

typedef struct {
    /* Audio Registers in MMU
    0xFF20 - NR41 - Channel 4 Sound Length (Write Only)
    0xFF21 - NR42 - Channel 4 Volume & Envelope (R/W)
    0xFF22 - NR43 - Channel 4 Frequency and Randomness (R/W)
    0xFF23 - NR44 - Channel 4 Control (R/W)
    */
    uint8_t NR41;
    uint8_t NR42;
    uint8_t NR43;
    uint8_t NR44;
    //LSFR 
    uint16_t LSFR;
    //Volume, Envelope, and Length
    int Volume;
    int EnvelopePeriod;
    int EnvelopeDirection;
    int EnvelopeTimer;
    int LengthCounter;
    int ClockShift;
    int Timer;
    //Channel on or off
    int ChannelOn;
    //Audio Sample
    int Sample;
} NoiseChannel;

typedef struct {
    //APU Channels
    PulseWithSweepChannel PulseWithSweep;
    PulseChannel Pulse;
    WaveChannel Wave;
    NoiseChannel Noise;
    /* Audio Registers in MMU
    0xFF24 - NR50 - Master volume & VIN panning
    0xFF25 - NR51 - Sound Panning
    0xFF26 - NR52 - Audio Master Control 
    */
    uint8_t NR50;
    uint8_t NR51;
    uint8_t NR52;
    //Master Audio Sample
    uint16_t MasterSampleRight;
    uint16_t MasterSampleLeft;
    int FrameSequencerStep;
    int FrameSequencerCounter;
    int CurrentSample;
    uint16_t AudioBuffer[200]; //One Frames Worth, including Stereo Sound.
    int Ticks;
} APU;

//Main Functions
void APUInit(APU *APU, MMU *MMU);
void APUTick(APU *APU, MMU *MMU);
void APUUpdate(APU *APU, MMU *MMU);

//Tick Channel Functions
void APUPulseWithSweepTick(APU *APU, MMU *MMU);
void APUPulseTick(APU *APU, MMU *MMU);
void APUWaveTick(APU *APU, MMU *MMU);
void APUNoiseTick(APU *APU, MMU *MMU);

//Length CLK, Sweep CLK, and Envelope CLK
void APULengthCLK(APU *APU, MMU *MMU);
void APUSweepCLK(APU *APU, MMU *MMU);
void APUEnvelopeCLK(APU *APU, MMU *MMU);

//Trigger Functions
void APUPulseWithSweepTrigger(APU *APU, MMU *MMU);
void APUPulseTrigger(APU *APU, MMU *MMU);
void APUWaveTrigger(APU *APU, MMU *MMU);
void APUNoiseTrigger(APU *APU, MMU *MMU);

//SDL Audio Functions
void SDLPlayAudio(APU *APU, MMU *MMU);


#endif 