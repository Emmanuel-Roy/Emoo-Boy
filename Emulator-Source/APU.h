#ifndef APU_H
#define APU_H

#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "MMU.h"

typedef struct {
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
    int Timer;
    int DutyStep;
    int SweepTimer;
    int SweepPeriod;
    int SweepShift;
    int SweepDirection;
    int ShadowFrequency;
    int SweepEnabled;
    //Channel on or off
    int ChannelOn;
    //Audio Sample
    int Sample;
} PulseWithSweepChannel;

typedef struct {
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
    int Timer;
    int DutyStep;
    //Channel on or off
    int ChannelOn;
    //Audio Sample
    int Sample;
} PulseChannel;

typedef struct {
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
    int Timer;
    int Position;
    //Channel on or off
    int ChannelOn;
    //Audio Sample
    int Sample;
} WaveChannel;

typedef struct {
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
    int16_t MasterSampleRight;
    int16_t MasterSampleLeft;
    int FrameSequencerStep;
    int FrameSequencerCounter;
    int CurrentSample;
    int SampleTimer;
    int16_t AudioBuffer[2048]; // Stereo PCM buffer
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