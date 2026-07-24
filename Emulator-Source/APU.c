#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "APU.h"    
#include "MMU.h"
#include <math.h>

extern SDL_AudioSpec audio;
extern SDL_AudioSpec audio2;
extern SDL_AudioDeviceID audioDevice;

static const uint8_t DutyCycles[4][8] = {
    {0, 0, 0, 0, 0, 0, 0, 1}, // 12.5%
    {1, 0, 0, 0, 0, 0, 0, 1}, // 25%
    {1, 0, 0, 0, 0, 1, 1, 1}, // 50%
    {0, 1, 1, 1, 1, 1, 1, 0}  // 75%
};

static const int NoiseDivisors[8] = {8, 16, 32, 48, 64, 80, 96, 112};

void APUInit(APU *APU, MMU *MMU) {
    MMU->SystemMemory[0xFF24] = 0x77; // NR50
    MMU->SystemMemory[0xFF25] = 0xF3; // NR51
    MMU->SystemMemory[0xFF26] = 0xF1; // NR52

    MMU->SystemMemory[0xFF10] = 0x80;
    MMU->SystemMemory[0xFF11] = 0xBF;
    MMU->SystemMemory[0xFF12] = 0xF3;
    MMU->SystemMemory[0xFF14] = 0xBF;

    MMU->SystemMemory[0xFF16] = 0x3F;
    MMU->SystemMemory[0xFF17] = 0x00;
    MMU->SystemMemory[0xFF19] = 0xBF;

    MMU->SystemMemory[0xFF1A] = 0x7F;
    MMU->SystemMemory[0xFF1B] = 0xFF;
    MMU->SystemMemory[0xFF1C] = 0x9F;
    MMU->SystemMemory[0xFF1E] = 0xBF;

    MMU->SystemMemory[0xFF20] = 0xFF;
    MMU->SystemMemory[0xFF21] = 0x00;
    MMU->SystemMemory[0xFF22] = 0x00;
    MMU->SystemMemory[0xFF23] = 0xBF;

    for (int i = 0xFF30; i <= 0xFF3F; i++) {
        MMU->SystemMemory[i] = 0x00;
    }

    APU->PulseWithSweep.Sample = 0;
    APU->PulseWithSweep.ChannelOn = 0;
    APU->PulseWithSweep.Timer = 0;
    APU->PulseWithSweep.DutyStep = 0;

    APU->Pulse.Sample = 0;
    APU->Pulse.ChannelOn = 0;
    APU->Pulse.Timer = 0;
    APU->Pulse.DutyStep = 0;

    APU->Wave.Sample = 0;
    APU->Wave.ChannelOn = 0;
    APU->Wave.Timer = 0;
    APU->Wave.Position = 0;

    APU->Noise.Sample = 0;
    APU->Noise.ChannelOn = 0;
    APU->Noise.Timer = 0;
    APU->Noise.LSFR = 0x7FFF;

    APU->MasterSampleLeft = 0;
    APU->MasterSampleRight = 0;
    APU->FrameSequencerStep = 0;
    APU->FrameSequencerCounter = 8192;
    APU->CurrentSample = 0;
    APU->SampleTimer = 0;
    APU->Ticks = 0;
}

void APUUpdate(APU *APU, MMU *MMU) {
    APU->NR50 = MMU->SystemMemory[0xFF24];
    APU->NR51 = MMU->SystemMemory[0xFF25];
    APU->NR52 = MMU->SystemMemory[0xFF26];

    // Channel 1
    APU->PulseWithSweep.NR10 = MMU->SystemMemory[0xFF10];
    APU->PulseWithSweep.NR11 = MMU->SystemMemory[0xFF11];
    APU->PulseWithSweep.NR12 = MMU->SystemMemory[0xFF12];
    APU->PulseWithSweep.NR13 = MMU->SystemMemory[0xFF13];
    if (APU->PulseWithSweep.NR14 != MMU->SystemMemory[0xFF14]) {
        if ((MMU->SystemMemory[0xFF14] & 0x80) == 0x80) {
            APUPulseWithSweepTrigger(APU, MMU);
        }
    }
    APU->PulseWithSweep.NR14 = MMU->SystemMemory[0xFF14];

    // Channel 2
    APU->Pulse.NR21 = MMU->SystemMemory[0xFF16];
    APU->Pulse.NR22 = MMU->SystemMemory[0xFF17];
    APU->Pulse.NR23 = MMU->SystemMemory[0xFF18];
    if (APU->Pulse.NR24 != MMU->SystemMemory[0xFF19]) {
        if ((MMU->SystemMemory[0xFF19] & 0x80) == 0x80) {
            APUPulseTrigger(APU, MMU);
        }
    }
    APU->Pulse.NR24 = MMU->SystemMemory[0xFF19];

    // Channel 3
    APU->Wave.NR30 = MMU->SystemMemory[0xFF1A];
    APU->Wave.NR31 = MMU->SystemMemory[0xFF1B];
    APU->Wave.NR32 = MMU->SystemMemory[0xFF1C];
    APU->Wave.NR33 = MMU->SystemMemory[0xFF1D];
    if (APU->Wave.NR34 != MMU->SystemMemory[0xFF1E]) {
        if ((MMU->SystemMemory[0xFF1E] & 0x80) == 0x80) {
            APUWaveTrigger(APU, MMU);
        }
    }
    APU->Wave.NR34 = MMU->SystemMemory[0xFF1E];
    for (int i = 0xFF30; i <= 0xFF3F; i++) {
        APU->Wave.WavePatternRAM[i - 0xFF30] = MMU->SystemMemory[i];
    }

    // Channel 4
    APU->Noise.NR41 = MMU->SystemMemory[0xFF20];
    APU->Noise.NR42 = MMU->SystemMemory[0xFF21];
    APU->Noise.NR43 = MMU->SystemMemory[0xFF22];
    if (APU->Noise.NR44 != MMU->SystemMemory[0xFF23]) {
        if ((MMU->SystemMemory[0xFF23] & 0x80) == 0x80) {
            APUNoiseTrigger(APU, MMU);
        }
    }
    APU->Noise.NR44 = MMU->SystemMemory[0xFF23];
}

void APULengthCLK(APU *APU, MMU *MMU) {
    if ((APU->PulseWithSweep.NR14 & 0x40) && APU->PulseWithSweep.LengthCounter > 0) {
        APU->PulseWithSweep.LengthCounter--;
        if (APU->PulseWithSweep.LengthCounter == 0) APU->PulseWithSweep.ChannelOn = 0;
    }
    if ((APU->Pulse.NR24 & 0x40) && APU->Pulse.LengthCounter > 0) {
        APU->Pulse.LengthCounter--;
        if (APU->Pulse.LengthCounter == 0) APU->Pulse.ChannelOn = 0;
    }
    if ((APU->Wave.NR34 & 0x40) && APU->Wave.LengthCounter > 0) {
        APU->Wave.LengthCounter--;
        if (APU->Wave.LengthCounter == 0) APU->Wave.ChannelOn = 0;
    }
    if ((APU->Noise.NR44 & 0x40) && APU->Noise.LengthCounter > 0) {
        APU->Noise.LengthCounter--;
        if (APU->Noise.LengthCounter == 0) APU->Noise.ChannelOn = 0;
    }
}

void APUSweepCLK(APU *APU, MMU *MMU) {
    if (APU->PulseWithSweep.SweepEnabled && APU->PulseWithSweep.SweepPeriod > 0) {
        APU->PulseWithSweep.SweepTimer--;
        if (APU->PulseWithSweep.SweepTimer <= 0) {
            APU->PulseWithSweep.SweepTimer = APU->PulseWithSweep.SweepPeriod ? APU->PulseWithSweep.SweepPeriod : 8;
            if (APU->PulseWithSweep.SweepEnabled && APU->PulseWithSweep.SweepPeriod > 0) {
                int newFreq = APU->PulseWithSweep.ShadowFrequency >> APU->PulseWithSweep.SweepShift;
                if (APU->PulseWithSweep.SweepDirection) newFreq = APU->PulseWithSweep.ShadowFrequency - newFreq;
                else newFreq = APU->PulseWithSweep.ShadowFrequency + newFreq;

                if (newFreq <= 2047 && APU->PulseWithSweep.SweepShift > 0) {
                    APU->PulseWithSweep.ShadowFrequency = newFreq;
                    APU->PulseWithSweep.NR13 = newFreq & 0xFF;
                    APU->PulseWithSweep.NR14 = (APU->PulseWithSweep.NR14 & 0xF8) | ((newFreq >> 8) & 0x07);
                } else if (newFreq > 2047) {
                    APU->PulseWithSweep.ChannelOn = 0;
                }
            }
        }
    }
}

void APUEnvelopeCLK(APU *APU, MMU *MMU) {
    if (APU->PulseWithSweep.EnvelopePeriod > 0) {
        APU->PulseWithSweep.EnvelopeTimer--;
        if (APU->PulseWithSweep.EnvelopeTimer <= 0) {
            APU->PulseWithSweep.EnvelopeTimer = APU->PulseWithSweep.EnvelopePeriod;
            if (APU->PulseWithSweep.EnvelopeDirection && APU->PulseWithSweep.Volume < 15) APU->PulseWithSweep.Volume++;
            else if (!APU->PulseWithSweep.EnvelopeDirection && APU->PulseWithSweep.Volume > 0) APU->PulseWithSweep.Volume--;
        }
    }
    if (APU->Pulse.EnvelopePeriod > 0) {
        APU->Pulse.EnvelopeTimer--;
        if (APU->Pulse.EnvelopeTimer <= 0) {
            APU->Pulse.EnvelopeTimer = APU->Pulse.EnvelopePeriod;
            if (APU->Pulse.EnvelopeDirection && APU->Pulse.Volume < 15) APU->Pulse.Volume++;
            else if (!APU->Pulse.EnvelopeDirection && APU->Pulse.Volume > 0) APU->Pulse.Volume--;
        }
    }
    if (APU->Noise.EnvelopePeriod > 0) {
        APU->Noise.EnvelopeTimer--;
        if (APU->Noise.EnvelopeTimer <= 0) {
            APU->Noise.EnvelopeTimer = APU->Noise.EnvelopePeriod;
            if (APU->Noise.EnvelopeDirection && APU->Noise.Volume < 15) APU->Noise.Volume++;
            else if (!APU->Noise.EnvelopeDirection && APU->Noise.Volume > 0) APU->Noise.Volume--;
        }
    }
}

void APUPulseWithSweepTick(APU *APU, MMU *MMU) {
    if (!APU->PulseWithSweep.ChannelOn) {
        APU->PulseWithSweep.Sample = 0;
        return;
    }
    APU->PulseWithSweep.Timer--;
    if (APU->PulseWithSweep.Timer <= 0) {
        uint16_t freq = APU->PulseWithSweep.NR13 | ((APU->PulseWithSweep.NR14 & 0x07) << 8);
        APU->PulseWithSweep.Timer = (2048 - freq) * 4;
        APU->PulseWithSweep.DutyStep = (APU->PulseWithSweep.DutyStep + 1) & 7;
    }
    uint8_t duty = (APU->PulseWithSweep.NR11 >> 6) & 0x03;
    if (DutyCycles[duty][APU->PulseWithSweep.DutyStep]) {
        APU->PulseWithSweep.Sample = APU->PulseWithSweep.Volume;
    } else {
        APU->PulseWithSweep.Sample = 0;
    }
}

void APUPulseTick(APU *APU, MMU *MMU) {
    if (!APU->Pulse.ChannelOn) {
        APU->Pulse.Sample = 0;
        return;
    }
    APU->Pulse.Timer--;
    if (APU->Pulse.Timer <= 0) {
        uint16_t freq = APU->Pulse.NR23 | ((APU->Pulse.NR24 & 0x07) << 8);
        APU->Pulse.Timer = (2048 - freq) * 4;
        APU->Pulse.DutyStep = (APU->Pulse.DutyStep + 1) & 7;
    }
    uint8_t duty = (APU->Pulse.NR21 >> 6) & 0x03;
    if (DutyCycles[duty][APU->Pulse.DutyStep]) {
        APU->Pulse.Sample = APU->Pulse.Volume;
    } else {
        APU->Pulse.Sample = 0;
    }
}

void APUWaveTick(APU *APU, MMU *MMU) {
    if (!APU->Wave.ChannelOn || !(APU->Wave.NR30 & 0x80)) {
        APU->Wave.Sample = 0;
        return;
    }
    APU->Wave.Timer--;
    if (APU->Wave.Timer <= 0) {
        uint16_t freq = APU->Wave.NR33 | ((APU->Wave.NR34 & 0x07) << 8);
        APU->Wave.Timer = (2048 - freq) * 2;
        APU->Wave.Position = (APU->Wave.Position + 1) & 31;
    }
    uint8_t byteVal = APU->Wave.WavePatternRAM[APU->Wave.Position / 2];
    uint8_t sampleVal = (APU->Wave.Position & 1) ? (byteVal & 0x0F) : (byteVal >> 4);
    uint8_t volumeCode = (APU->Wave.NR32 >> 5) & 0x03;
    switch (volumeCode) {
        case 0: APU->Wave.Sample = 0; break;
        case 1: APU->Wave.Sample = sampleVal; break;
        case 2: APU->Wave.Sample = sampleVal >> 1; break;
        case 3: APU->Wave.Sample = sampleVal >> 2; break;
    }
}

void APUNoiseTick(APU *APU, MMU *MMU) {
    if (!APU->Noise.ChannelOn) {
        APU->Noise.Sample = 0;
        return;
    }
    APU->Noise.Timer--;
    if (APU->Noise.Timer <= 0) {
        uint8_t shift = (APU->Noise.NR43 >> 4) & 0x0F;
        uint8_t divisor = APU->Noise.NR43 & 0x07;
        APU->Noise.Timer = (NoiseDivisors[divisor] << shift);

        uint16_t lfsr = APU->Noise.LSFR;
        uint8_t result = (lfsr & 1) ^ ((lfsr >> 1) & 1);
        lfsr >>= 1;
        lfsr |= (result << 14);
        if (APU->Noise.NR43 & 0x08) {
            lfsr &= ~(1 << 6);
            lfsr |= (result << 6);
        }
        APU->Noise.LSFR = lfsr;
    }
    if ((APU->Noise.LSFR & 1) == 0) {
        APU->Noise.Sample = APU->Noise.Volume;
    } else {
        APU->Noise.Sample = 0;
    }
}

void APUPulseWithSweepTrigger(APU *APU, MMU *MMU) {
    APU->PulseWithSweep.ChannelOn = 1;
    if (APU->PulseWithSweep.LengthCounter == 0) APU->PulseWithSweep.LengthCounter = 64;
    uint16_t freq = APU->PulseWithSweep.NR13 | ((APU->PulseWithSweep.NR14 & 0x07) << 8);
    APU->PulseWithSweep.Timer = (2048 - freq) * 4;
    APU->PulseWithSweep.Volume = (APU->PulseWithSweep.NR12 >> 4) & 0x0F;
    APU->PulseWithSweep.EnvelopeDirection = (APU->PulseWithSweep.NR12 & 0x08) ? 1 : 0;
    APU->PulseWithSweep.EnvelopePeriod = APU->PulseWithSweep.NR12 & 0x07;
    APU->PulseWithSweep.EnvelopeTimer = APU->PulseWithSweep.EnvelopePeriod;

    APU->PulseWithSweep.ShadowFrequency = freq;
    APU->PulseWithSweep.SweepPeriod = (APU->PulseWithSweep.NR10 >> 4) & 0x07;
    APU->PulseWithSweep.SweepDirection = (APU->PulseWithSweep.NR10 & 0x08) ? 1 : 0;
    APU->PulseWithSweep.SweepShift = APU->PulseWithSweep.NR10 & 0x07;
    APU->PulseWithSweep.SweepTimer = APU->PulseWithSweep.SweepPeriod ? APU->PulseWithSweep.SweepPeriod : 8;
    APU->PulseWithSweep.SweepEnabled = (APU->PulseWithSweep.SweepPeriod > 0 || APU->PulseWithSweep.SweepShift > 0);
}

void APUPulseTrigger(APU *APU, MMU *MMU) {
    APU->Pulse.ChannelOn = 1;
    if (APU->Pulse.LengthCounter == 0) APU->Pulse.LengthCounter = 64;
    uint16_t freq = APU->Pulse.NR23 | ((APU->Pulse.NR24 & 0x07) << 8);
    APU->Pulse.Timer = (2048 - freq) * 4;
    APU->Pulse.Volume = (APU->Pulse.NR22 >> 4) & 0x0F;
    APU->Pulse.EnvelopeDirection = (APU->Pulse.NR22 & 0x08) ? 1 : 0;
    APU->Pulse.EnvelopePeriod = APU->Pulse.NR22 & 0x07;
    APU->Pulse.EnvelopeTimer = APU->Pulse.EnvelopePeriod;
}

void APUWaveTrigger(APU *APU, MMU *MMU) {
    APU->Wave.ChannelOn = 1;
    if (APU->Wave.LengthCounter == 0) APU->Wave.LengthCounter = 256;
    uint16_t freq = APU->Wave.NR33 | ((APU->Wave.NR34 & 0x07) << 8);
    APU->Wave.Timer = (2048 - freq) * 2;
    APU->Wave.Position = 0;
}

void APUNoiseTrigger(APU *APU, MMU *MMU) {
    APU->Noise.ChannelOn = 1;
    if (APU->Noise.LengthCounter == 0) APU->Noise.LengthCounter = 64;
    uint8_t shift = (APU->Noise.NR43 >> 4) & 0x0F;
    uint8_t divisor = APU->Noise.NR43 & 0x07;
    APU->Noise.Timer = (NoiseDivisors[divisor] << shift);
    APU->Noise.Volume = (APU->Noise.NR42 >> 4) & 0x0F;
    APU->Noise.EnvelopeDirection = (APU->Noise.NR42 & 0x08) ? 1 : 0;
    APU->Noise.EnvelopePeriod = APU->Noise.NR42 & 0x07;
    APU->Noise.EnvelopeTimer = APU->Noise.EnvelopePeriod;
    APU->Noise.LSFR = 0x7FFF;
}

void APUTick(APU *APU, MMU *MMU) {
    APUUpdate(APU, MMU);

    if ((APU->NR52 & 0x80) == 0) {
        APUInit(APU, MMU);
        return;
    }

    if (APU->FrameSequencerCounter <= 0) {
        APU->FrameSequencerCounter = 8192;
        switch (APU->FrameSequencerStep) {
            case 0: case 4: APULengthCLK(APU, MMU); break;
            case 2: case 6: APUSweepCLK(APU, MMU); APULengthCLK(APU, MMU); break;
            case 7: APUEnvelopeCLK(APU, MMU); break;
        }
        APU->FrameSequencerStep = (APU->FrameSequencerStep + 1) & 7;
    } else {
        APU->FrameSequencerCounter--;
    }

    APUPulseWithSweepTick(APU, MMU);
    APUPulseTick(APU, MMU);
    APUWaveTick(APU, MMU);
    APUNoiseTick(APU, MMU);

    // Mixer
    APU->MasterSampleLeft = 0;
    APU->MasterSampleRight = 0;

    if (APU->NR51 & 0x10) APU->MasterSampleLeft += APU->PulseWithSweep.Sample;
    if (APU->NR51 & 0x01) APU->MasterSampleRight += APU->PulseWithSweep.Sample;

    if (APU->NR51 & 0x20) APU->MasterSampleLeft += APU->Pulse.Sample;
    if (APU->NR51 & 0x02) APU->MasterSampleRight += APU->Pulse.Sample;

    if (APU->NR51 & 0x40) APU->MasterSampleLeft += APU->Wave.Sample;
    if (APU->NR51 & 0x04) APU->MasterSampleRight += APU->Wave.Sample;

    if (APU->NR51 & 0x80) APU->MasterSampleLeft += APU->Noise.Sample;
    if (APU->NR51 & 0x08) APU->MasterSampleRight += APU->Noise.Sample;

    // Downsampling to 44.1 kHz (~95 T-cycles per sample)
    APU->SampleTimer++;
    if (APU->SampleTimer >= 95) {
        APU->SampleTimer = 0;
        SDLPlayAudio(APU, MMU);
    }
}

void SDLPlayAudio(APU *APU, MMU *MMU) {
    int leftVol = (APU->NR50 & 0x07);
    int rightVol = (APU->NR50 & 0x70) >> 4;

    int16_t leftPcm = (int16_t)(APU->MasterSampleLeft * leftVol * 120);
    int16_t rightPcm = (int16_t)(APU->MasterSampleRight * rightVol * 120);

    APU->AudioBuffer[APU->CurrentSample * 2] = leftPcm;
    APU->AudioBuffer[APU->CurrentSample * 2 + 1] = rightPcm;
    APU->CurrentSample++;

    if (APU->CurrentSample >= 512) {
        if (SDL_GetQueuedAudioSize(audioDevice) < 8192) {
            SDL_QueueAudio(audioDevice, APU->AudioBuffer, sizeof(int16_t) * 1024);
        }
        APU->CurrentSample = 0;
    }
}