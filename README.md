# Emoo-Boy: A Gameboy Emulator written in C

## Contributors:
  * Emmanuel Roy

## Introduction:
The Nintendo Gameboy system is one of the most beloved devices of all time, selling a combined 118.69 million units worldwide, one of which ended up in the hands of my family. 

I have a lot of nostalgia for the system, and I figured creating an accurate simulation of the hardware would be an incredibly challenging yet fulfilling technical achievement.

My Ultimate Goal with this project is for the emulator to run the following games from my childhood.
* Pokemon Gold and Silver ✅
* Link's Awakening ✅ 
* Kirby's Dream Land 2 ✅

## Goals:
  * Boot commercial games like Tetris and Pokemon Gold. ✅
  * Custom Palette Support. ✅
  * Save Support. ✅
  * Pass the dmg-acid2 display test. ✅
  * Implement all instructions and pass the Blargg cpu_instrs test. ✅

## Planned Editions:
  * Real Time Clock Support. ✅
  * Save States.
  * Implement Sound.
  * Implement Gameboy Color Compatibility.
  * Make a physical device, hopefully around the size of a credit card, using a Raspberry Pi Zero with a custom PCB and Casing.

# My Process and Challenges.

## Why I chose C and not a more modern language?
 * In short, I wanted to reduce the amount of dependencies used and maximize compatibility on a wide range of systems.
 * As mentioned in my planned editions, I want to eventually be able to run this emulator on its own embedded system, and I feel that using C will open doors for this later on, even if the lack of classes and other object-oriented fundamentals lead to somewhat messier code.
 * In addition, my current university courses focus on languages like C++ and Java, and I feel as if I have become somewhat complacent in maintaining my C programming skills.

## Initial Renderer Attempts.
 * I started this project by working on a basic renderer, and I did so because I thought this would be incredibly useful to help debug other components early on. To do this, I used mGBA to dump Memory Data while games were playing and created a renderer based on the information in the Gameboy Pandocs to reconstruct this memory data into actual frames.
 * This was useful in teaching me some basic system details, and it was awesome to see visual confirmation of my progress in real time. Using memory dumps was also insightful in figuring out what potential save state functionality could look like in the future. 
 * I started working on a renderer for the background, as seen by the first 5 images, and once I verified that worked I tested background scrolling.
 * After implementing the background, I sought to implement the window functionality.
 * Finally, to complete the basic renderer I implemented OAM scanning and sprite functionality.
 * At this point, I tried to test the DMG-Acid2 ROM, however, the results and errors found from testing were most likely not problems with implementation, but rather the inherent problems from rendering out of a memory dump. Getting this test to render properly as a goal was moved to be done during PPU testing and evaluation.

### Background Renderer (1-4), Background Scrolling (5-6), Window Renderer (7), Sprite Renderer (8), Palette Support (9-11), DMG-Acid2 Test (12)
<img src= "https://github.com/Emmanuel-Roy/Emoo-Boy/assets/54725843/b88b7e96-f4ab-4963-be9f-74e88de82ec8" width="200">
<img src= "https://github.com/Emmanuel-Roy/Emoo-Boy/assets/54725843/21f3ff0b-8fd9-4ebc-9d3f-259d480ec9bd" width="200">
<img src= "https://github.com/Emmanuel-Roy/Emoo-Boy/assets/54725843/7d745293-6f5f-4236-adc6-aa18fda82707" width="200">
<img src= "https://github.com/Emmanuel-Roy/Emoo-Boy/assets/54725843/91c77edd-2fde-484e-bc15-29d6b7a5dfcd" width="200">
<img src= "https://github.com/Emmanuel-Roy/Emoo-Boy/assets/54725843/fd14776d-09b6-4c5f-9d07-fb41d3166ddd" width="200">
<img src= "https://github.com/Emmanuel-Roy/Emoo-Boy/assets/54725843/55fd1cc6-2888-4f58-9a4d-d46c7210a715" width="200">
<img src= "https://github.com/Emmanuel-Roy/Emoo-Boy/assets/54725843/f9ed6ac8-85ef-4395-bc62-1c14d558e0c8" width="200">
<img src= "https://github.com/Emmanuel-Roy/Emoo-Boy/assets/54725843/cfa24ee7-8c58-4fb8-ab51-3542c5ab141e" width="200">
<img src= "https://github.com/Emmanuel-Roy/Emoo-Boy/assets/54725843/f4523e7f-4cda-44f3-adee-4d7f02a424fe" width="200">
<img src= "https://github.com/Emmanuel-Roy/Emoo-Boy/assets/54725843/667a08bb-2f81-42a0-ae47-6d1ab4028812" width="200">
<img src= "https://github.com/Emmanuel-Roy/Emoo-Boy/assets/54725843/786f0790-cb0b-4328-9dda-c0ebaf986f2f" width="200">
<img src= "https://github.com/Emmanuel-Roy/Emoo-Boy/assets/54725843/bfd2302e-87d3-4021-9ca6-f88d95e233c7" width="200">


## Designing the Software Structure.
  * At this point in the project, I spent a lot of time researching the source code of more mature emulators to figure out how they were implemented. I'm not a fan of using many files, so I wanted to base everything on a few basic components. 
  * These components would be held in individual.c and .h files, containing a struct definition for each element and the functions that allowed the components to work.
  * One major difference between my emulator and a lot of others was the focus on using one large uint8_t SystemMemory[0x10000] array. The primary reason for this was to allow for easier implementation of save-state support later on, and also because it felt more true to the actual system as far as I read documentation-wise in terms of the way addressing modes and the Gameboy memory map worked. While this was perhaps not the optimal choice for performance, I thought it was cool.
  * Ultimately, after reading the Pan Docs a few times and cross-referencing other mature emulators, I decided on my current software structure.
  * Essentially, the main would get information which was then passed into an overarching struct called GameBoy. This struct initializes all the other components (PPU, Timers, CPU, MMU, APU), loads in ROM and RAM data from the given save files, and then ticks itself and each component in an infinite loop until the user chooses to exit the program.
  * While designing each underlying component, I needed to see what would change during each "tick" (Clock Cycle). Aside from this, I also needed to research on the pandocs and other emulators what registers and other variables were used by each component, and why.

  
## Implementing Main
  * Getting basic console functionality was imperative for this project. I wanted users to be able to choose ROM files, Save files, Palettes, Control help, and read cartridge information before actually launching the games.
  * Getting this done was rather simple, and only took a couple of hours. It also helped me understand how I would load and process files given by users, which was very useful for MMU implementation.

![image](https://github.com/Emmanuel-Roy/Emoo-Boy/assets/54725843/a18875b6-0121-4220-bd59-7b8e54b965f1)


## Implementing the MMU 
  * The first major component I worked on in the system was the MMU. This was because I figured having the ability to load roms would be imperative for developing more difficult components later on.
  * The MMU ended up being one of the largest and most complicated components to implement by far, and I enjoyed thinking about the challenges.
  * One of my favorite challenges was setting up Memory Banking support, and I chose to implement this by loading the ROM bank data from another array to the SystemMemory array when requested.
  * Aside from this, I wrote basic DMA functions and basic MMURead and MMUWrite functions for the CPU to use. This was interesting because I had most of the other components interface with system memory directly, but I figured that there were many cases (such as CPU reading/writing from VRAM in PPU mode 3) for me to justify writing dedicated functions.

![image](https://github.com/Emmanuel-Roy/Emoo-Boy/assets/54725843/07adc605-a890-41aa-8c2b-78ae3cbd0bb1)

## Implementing the PPU 
  * Originally, I thought the PPU would be the component that would take the most time. I think was right, however, my previous implementation of the VRAM Reader made this component significantly easier to implement.
  * In essence, I converted the functions in the VRAM reader to operate on a pixel-by-pixel basis, and I only needed to implement correct timing for the modes alongside raising interrupt flags when necessary.
  * I had some previous experience using SDL for the CHIP-8 emulator and VRAM Reader, so it was a breeze setting up the rendering and GUI this time. I found that rendering an image pixel by pixel was a bit too slow, so even though it could cause accuracy issues in edge-case games, I chose to only render on a scanline-by-scanline basis.
  * The PPU ended up also not using the MMURead and MMUWrite functions and instead interfaced with system memory directly. I did this because it was a little easier to code, and I figured all the extra function calls wouldn't be an issue down the line.
  * One of the major things that the PPU got me to think of was the idea of Machine Cycles vs Ticks. It turns out that a lot of documentation on the Gameboy focuses on machine cycles, (4 system ticks), even though a lot of components rely on system ticks to operate. For example, a given opcode may take 1 machine cycle, but that would also activate 4 ticks on the PPU, drawing 4 pixels. Another opcode could take 2 machine cycles, and would as such activate 8 ticks on the PPU, drawing 8 pixels.
  * Thinking about this also helped me think about how I would fix timing issues if they occurred, and I ultimately just chose to loop the other system components for however many ticks the CPU took to operate a given opcode.

## Implementing the Gamepad and the Timer
  * The gamepad was a breeze, I just had SDL check the keyboard after each CPU cycle and updated the appropriate register in system memory accordingly.
  * I ended up adding it to MMU.c, as I figured opcodes like "stop" would rely on checking for an update to the gamepad.
  * The timer wasn't as easy, but it only took an hour or two to understand and set up. The previous knowledge I acquired about ticks vs machine cycles was also relevant here, and I thought it was an interesting technical challenge to convert the two.

## Implementing the CPU (PT 1) and Interrupt Handling
  * MANY Lessons learned here. Interrupt handling was surprisingly simple and took only an hour or so to set up.
  * It turns out there's a good reason why every emulator guide recommends completely the CPU first, it's tough to debug other components if you don't have good accuracy when implementing instructions.
  * I implemented the first couple of instructions required for the "hello-world.gb" rom, which took a couple of attempts to get right. Thankfully there wasn't anything wrong with my PPU for this test.
  * Next I tried to implement the instructions for "DMG-Acid2.gb", and it took a while to implement and ensure that the instructions WEREN'T the reason for test failures. Ultimately, I was searching the .asm file trying to see if fixing any CPU bugs would fix my poor PPU implementation.
  * Ultimately, I think having a rock-solid CPU implementation would have saved a ton of heartache, and I would recommend anyone starting an emulator to finish that first.

<img src= "https://github.com/user-attachments/assets/25fed018-e827-4fc6-bc56-f727ce2be3fb" width="200">

## PPU Testing
  * I failed DMG-Acid2.gb a TON of times, and most of it had to do with my initial PPU implementation.
  * I changed gears, using the "The Gameboy Emulator Development Guide" for calculating offsets, adding tons of variables like the Window Line Counter.
  * I went back to my VRAM testing application and used it to test new implementations. If it broke the VRAM reader, chances were that it wouldn't work in the emulator as well.
  * With the help of the emu dev discord, I ended up realizing one of the biggest mistakes I made was in calculating the LYC=LY interrupt, which seemed to be causing issues every vblank.
  * This was one of the hardest and longest parts of development, but I'm glad I got to make it in the end.
  * If you are reading this hoping to develop your own emulator, DON'T GIVE UP!!!

### July 9th 2024:

<img src= "https://github.com/user-attachments/assets/2f7e9594-d587-4faf-974a-17cb6f3413ee" width="200">
<img src= "https://github.com/user-attachments/assets/03f71db7-b6fc-439c-955e-a84d0323012b" width="200">
<img src= "https://github.com/user-attachments/assets/4c40c1ff-cc00-43c0-848a-3ae9ea73389d" width="200">
<img src= "https://github.com/user-attachments/assets/477e40ee-9a63-41e2-a758-b26b5cf57bb9" width="200">

### July 13th, 2024:

<img src= "https://github.com/user-attachments/assets/a1fdda7f-56e1-45b2-b61e-999be34590ed" width="200">
<img src= "https://github.com/user-attachments/assets/d513e35a-df24-4451-b2a8-2f2f9d5f84d5" width="200">
<img src= "https://github.com/user-attachments/assets/959470c9-6edc-40cb-9722-9a5f4c23c871" width="200">
<img src= "https://github.com/user-attachments/assets/5b32ef92-185f-4ee3-9dce-cae74921fcd8" width="200">

## Implementing the CPU (PT 2)
  * After gaining back my confidence, I decided to implement and debug Tetris.
  * This ended up going quite horribly, so I decided to walk back a bit and pass some standardized CPU tests first.
  * There are some great test roms, namely the Blargg's suite, and a useful tool called Gameboy Doctor debugs instructions tick by tick.
  * GameBoy Doctor seemed to have some accuracy issues with some tests, namely test 2, so I also cross-referenced with some of the more accurate logs in the EmuDev Discord.
  * I decided to fork my code to a separate branch dedicated to Gameboy Doctor, with the intention to simply copy-paste the switch statement that passed all the instruction tests.
  * Here's what the terminal looked like for CPU testing.
![image](https://github.com/user-attachments/assets/86779d25-0d0e-46fe-8269-7461e04858c8)
  * After a while, I managed to pass the tests and implement every instruction correctly.
<img src= "https://github.com/user-attachments/assets/cef5e427-0113-4e9b-97b6-b1b100f754f4" width="200">

## Testing Games
 * I had a couple of issues getting games to run, as it seemed like there were a ton of bugs with my code. While most of the games I tried ended up crashing at some point, I managed to get my first game playable!
 * I'm thrilled that this was the first game that worked, as Kirby's Dreamland 2 is one of my favorite childhood games!
<img src= "https://github.com/user-attachments/assets/e4b71f77-9db0-40ba-a59a-86afe5dfc255" width="200">
<img src= "https://github.com/user-attachments/assets/509be3f3-4c2a-471e-bca6-4bc66c7cb312" width="200">
<img src= "https://github.com/user-attachments/assets/3ed8c715-af4a-4aba-b5e7-310bccf1b8b1" width="200">
<img src= "https://github.com/user-attachments/assets/e9b111dc-6b36-4724-a690-f49c1022dab0" width="200">

 * Tetris and some other simple games worked well.
<img src= "https://github.com/user-attachments/assets/420814a4-f11f-4d4a-bedd-1e14c9589d61" width="200">
<img src= "https://github.com/user-attachments/assets/068c0ca8-6392-4796-9326-c04d15225d37" width="200">
<img src= "https://github.com/user-attachments/assets/4e87b52f-f818-474c-9377-a2751fcd3ec1" width="200">

 * Metroid 2 had some major graphical glitches.
<img src= "https://github.com/user-attachments/assets/cae6fbe2-59b5-4779-8236-5982ccc90c89" width="200">

* Links Awakening is unplayable, most likely due to MBC issues I will need to fix.
<img src= "https://github.com/user-attachments/assets/6f630d01-20a3-48ec-98b9-4452fda89fd5" width="200">

* Pokemon Red has some bugs, namely with the window layer causing text to be unreadable, but it seems to be playable. Also it seems that my save function works!
<img src= "https://github.com/user-attachments/assets/ae84807a-b8a5-4b1a-95e9-ff1391464e3a" width="200">

* However, other than a broken intro sequence, a lack of RTC implementation, and issues preventing save files from working, it seems that Pokemon Gold works perfectly!
<img src= "https://github.com/user-attachments/assets/88b200d0-0d41-492b-a355-008cf4dfa77f" width="200">

## Fixes 

### Pokemon Red and Blue
* It seems that a problem with my window layer (replacing color 0 for transparency instead of white) caused the bugs in Pokemon Red, and now it works perfectly!
<img src= "https://github.com/user-attachments/assets/d12bd933-9cef-415b-b3e8-56fbeaf14803" width="200">
<img src= "https://github.com/user-attachments/assets/1f650c2e-a74f-45a9-87a2-7dbc86dd28e3" width="200">

### Link's Awakening
* Link's Awakening had a pretty interesting bug. It seemed to be consistently getting stuck on RST 38, an instruction that should not have been triggering. Upon further inspection, the routine at RST 00 was causing these. After inspecting the ROM file, there was a mismatch (0xFF instead of 0xC3) on the first byte of ROM data, which shouldn't have been happening. I checked the memory data and made sure the rom data was loaded properly, which it seemed to have been. This meant something was modifying this byte when it shouldn't have been, and I remembered that some games send RAM-enable requests this way. To fix this bug, I needed to add a case to MMUWrite where writes below 0x2000 don't do anything.
* It seemed that the Window was still bugged, and after consulting the Emudev discord, it seemed like the WY and WX viewports needed to be able to handle negative numbers. This was an easy fix, and I just changed uint8_t to int8_t for ViewportX and ViewportY.
* Well, the fix ended up being harder than anticipated, and I had to adjust the code a lot, mainly because the behavior for the int8_t for the viewport was right for the window, but uint8_t was right for the background. Ultimately, I decided to just delete viewportX and viewportY, and just use different variables for the window and the background based on their respective int8_t and uint8_t types.
<img src= "https://github.com/user-attachments/assets/9ddb07b9-33f7-446e-a389-e287d69fa6fd" width="200">

### Metroid 2
* Metroid 2 was a fairly easy fix, I just had to adjust the background/window pixel value by the first two bits of the actual palette stored in SystemMemory[FF47].
<img src= "https://github.com/user-attachments/assets/322e767b-7f8b-43d7-b177-f687cb42ac9a" width="200">

### Pokemon Gold and Silver
* I spent a long time trying to fix the "no window available for popping" error.
* If anyone came here attempting to figure it out, the long and short of it is that it's most likely a problem with your SRAM implementation, namely the method of bank switching you are using. Otherwise, it may be an issue with your RTC implementation.
* Here was the process in which how I tried to fix this.
* I implemented a basic RTC, it's not very accurate and doesn't use latching, just works by reading from the system clock whenever an RTC value is needed. However, it works fine enough for this game.
* I still had the saving bug and spent a ton of time debugging and going through CPU logs tirelessly.
* It got to the point where I have a Google Doc 17 pages long trying to reverse engineer the cause of the issue, even going through the Pokemon Gold decompilation.
* Well, it turns out the fix was easy. The extra fail-safes I had implemented in MMUSwapRAMBank() were the cause of the issue, namely the "bank = bank & (MMU->NumRAMBanks-1)" and the "if (bank == 0) {bank = 1}" pieces of code respectively.
* I believe I made this error because I falsely assumed RAM Bank switching was practically the same as ROM bank switching, but it turns out that it is not.
* Pokemon Gold and Silver now work perfectly!

### General Fixes

#### CPU Logging
* I realized that having a completely separate branch for CPU logging was inefficient, so I decided to just include it in the program as a setting.

#### Multithreading
* I discovered that performance was quite terrible, and it seems to be because SDL caps the framerate at 4000 or so FPS on Windows. This sounds great on paper until you realize that SDL renders a new frame for each scanline, meaning the actual framerate was around 29.7 fps.
* To mitigate this, I implemented some basic multithreading using SDL. I made a separate thread that would consistently render the screen at 60 fps. While this is a little off the Gameboy's internal 59.7 fps, I felt as if this was close enough, and I prefer the faster speed.
* This led to the internal game logic running a lot faster, so I had to include some form of an internal logic cap. To do this, I set up a system where SDL would delay for a millisecond after a user-specified amount of scanlines were rendered. On my desktop PC, I found the sweet spot to be around 1 millisecond for every 13 scanlines.
* In the future, I will be creating a speed-up function that will work by adjusting this internal variable.
* Adding multithreading also fixed the transitions between screens, as it was seemingly broken in games like Pokemon and Link's Awakening.

#### Allowing for Custom OBJ Palette 0, and OBJ Palette 1 support.
* While fixing Metroid 2, I realized that some games allow for these two palettes to be changed. This is seemingly because of the Super Gameboy and the Gameboy Color.
* To add functionality, I figured it would be cool to allow users to adjust these as well to their liking.

<img src= "https://github.com/user-attachments/assets/9958e738-e396-485d-9f4e-616a4b1845c0" width="200">
<img src= "https://github.com/user-attachments/assets/2878f90a-3d2d-42af-98e3-227cb788837a" width="200">

#### Custom Framerate Support.
* As stated in the multithreading portion, I created a separate thread that would constantly render the display at a rate of 60 frames per second. I figured it would be fun to add, so I have allowed users to set custom frame rates for how the display refreshes. Since this doesn't actually affect the internal game logic, I figure this method of increasing framerates is somewhat similar to interpolation.

## Audio Support (To Do)

* Audio on the GameBoy isn't too complex from a hardware perspective, but from an emulation perspective, it's an enormous task that often gets skipped.
* I've decided to implement audio because the system wouldn't be complete without it.
* The pandocs explains the APU like this, which will be what my implementation is based upon.
<img src= "https://github.com/user-attachments/assets/4cc47f24-f259-4565-a2d3-fca2d353693f" width="400">

* To prevent bugs, audio will most likely need to be handled on a separate thread.
* This is because the display already has a separate thread keeping it running at 60 fps, and the internal game logic has a delay that could cause issues with audio playback.

## Hardware (To Do)

* I plan to use a Raspberry Pi Zero 2 W and a custom PCB to create a handheld capable of running the "EMOO-Boy" software.
* For now though, I've tested running the program on my Steam deck through Proton, and it runs quite well.

![image](https://github.com/user-attachments/assets/b275ef92-4bd6-4ac2-841f-f4a8a1eccfba)

* I had to move the 1 ms delay to happen every 20 scanlines instead of every 13 like on my desktop, but it runs practically perfectly.
* I assume running the program on a much weaker Raspberry Pi may cause the built-in delay to be unnecessary or perhaps once every 40 or so scanlines. 


## Takeaways.
 * The project would have taken significantly longer without the vast amount of resources available on the internet.
 * Roughly ~7000 lines of code were written (not including apu).
 * Implementing the CPU is probably the best decision for future emulator development, it makes it significantly easier to isolate issues with each component during testing.
 * Considering how many opcodes were practically the same, it would have been more useful to have planned out dedicated functions rather than copying and pasting code between opcodes. Ultimately, I think I definitely rushed the CPU implementation.
 * Generative AI definitely won't be able to create a proper implementation for years to come, but it definitely can help reduce some of the tedium when developing components like the CPU.
 * PPU testing/implementation should have probably waited until I got the CPU flawless. However, implementing the VRAM reader first was a fantastic idea in keeping up motivation for the project.
 * Proper planning is imperative to saving time and fixing bugs in the long run.
 * Tests aren't entirely comprehensive nor clear about errors, so asking others for assistance can help reduce a lot of the frustration from debugging.

## Reference and Inspiration:
   * [Pandocs](https://gbdev.io/pandocs/) <- The best comprehensive reference in how the original Gameboy system operates.
   * [Ultimate Gameboy Talk](https://www.youtube.com/watch?v=HyzD8pNlpwI) <- A great video summary explaining the components of the Gameboy at a more technical level.
   * [GBDEV OPCODES](https://gbdev.io/gb-opcodes/optables/) <- OP Code Table, very useful for writing the CPU.
   * [Gameboy Emulator Development Guide](https://hacktix.github.io/GBEDG/ppu/) <- THE BEST REFERENCE for fixing PPU issues I could find.
   * [MGBA](https://github.com/mgba-emu/mgba) <- Perhaps the most mature Gameboy emulator out there, and it was very useful in getting data dumps to test functions like my VRAM Reader and save file loading/saving.
   * [Gameboy 2bpp](https://www.huderlem.com/demos/gameboy2bpp.html) <- Great reference in understanding how the tiles in the GameBoy are converted from bytes to actual pixels.
   * [Game Play Color](https://github.com/gameplaycolor/gameplaycolor) <- Inspiration for this project, I have fond memories of using this emulator to play a dump of my brother's Pokemon Blue Cartridge on my middle school iPad.
   * [emuboy Emulator](https://github.com/jacoblister/emuboy) <- I found his journey to create a Gameboy emulator incredibly useful in determining which order I tackled the technical problems in this project. I also found his implementation of the DMA transfer process to be quite insightful, and I tried to implement his technique for it in my emulator.
   * [PokeGB](https://binji.github.io/posts/pokegb/) <- Another source of inspiration, and a great write-up on how certain opcodes were implemented. This helped a lot when I was confused on how certain opcodes were implemented.
   * [Espeon ESP32 Gameboy Emulator](https://github.com/Ryuzaki-MrL/Espeon/tree/master) <- Very interesting emulator, some inspiration for the hardware side of this project.
   * [Low-Level Devel - Gameboy Emulator Development Youtube Series](https://www.youtube.com/playlist?list=PLVxiWMqQvhg_yk4qy2cSC3457wZJga_e5) <- Very useful to see the thought process behind emulator development.
   * [r/EmuDev](https://www.reddit.com/r/EmuDev/) and the Emulator Development Discord Server.
