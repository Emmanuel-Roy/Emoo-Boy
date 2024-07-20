## Build:

### Note
I would reccomend using Linux for this emulator. While perfectly playable on windows, it seems that the nature of the scanline renderer is messing with the frame rate, giving readings of 4000+ fps. It seems that windows caps it to around this number, which is problematic because there are 144 scanlines, meaning the actual framerate is capped around 28-30 fps instead of the 57.86 the gameboy normally runs at. Linux seems to not have this issue, and runs flawlessly.

### Windows
* Download the repo and run the "make Windows" command.
![image](https://github.com/user-attachments/assets/981d1bb5-4c8e-4b62-a4fe-180463a0defd)

### Linux
* Make sure you have SDL2 installed through sudo, and then run the "make Linux" command.
