-# Emoo-Boy: A Gameboy Emulator written in C

## Contributors:
  * Emmanuel Roy
    

## Introduction:
The Nintendo Gameboy system is one of the most beloved devices of all time, selling a combined 118.69 million units worldwide, one of which ended up in the hands of my family. 

I have a lot of nostalgia for the system, and I figured creating an accurate simulation of the hardware would be an incredibly challenging yet fulfilling technical achievement.

My Ultimate Goal with this project is for the emulator to run the following games from my childhood with near-perfect accuracy.
* Pokemon Gold and Silver
* The Legend of Zelda Links Awakening
* Kirby's Dream Land 2

## Goals:
  * Boot commercial games like Tetris and Pokemon Gold.
  * Custom Palette Support.
  * Save Support
  * Implement rendering based on the Pixel Fifo and pass the dmg-acid2 display test.
  * Implement all instructions and pass the Blargg cpu_instrs test.

## Planned Editions:
  * Real Time Clock Support
  * Implement Sound.
  * Implement Gameboy Color Compatibility.
  * Make a physical device, hopefully around the size of a credit card, using a Raspberry Pi Zero with a custom PCB and Casing.
  * Swap from Scanline-Based Rendering to Pixel Fifo, to get better accuracy in edge case games.

## My Process and Challenges.

### Why I chose C and not a more modern language.
 * In short, I wanted to reduce the amount of dependencies used and maximize compatibility on a wide range of systems.
 * As mentioned in my planned editions, I want to eventually be able to run this emulator on its own embedded system, and I feel that using C will open doors for this later on, even if the lack of classes and other object-oriented fundamentals lead to somewhat messier code.
 * In addition, my current university courses focus on languages like C++ and Java, and I feel as if I have become somewhat complacent in maintaining my C programming skills.

### Initial Renderer Attempts.
 * I started this project by working on a basic renderer, and I did so because I thought this would be incredibly useful to help debug other components early on. To do this, I used mGBA to dump Memory Data while games were playing and created a renderer based on the information in the Gameboy Pandocs to reconstruct this memory data into actual frames.
 * This was useful in teaching me some basic system details, and it was awesome to see visual confirmation of my progress in real-time. Using memory dumps was also insightful to figure out what potential save state functionality could look like in the future. 
 * I started working on a renderer for the background, as seen by the first 5 images, and once I verified that worked I tested background scrolling.
 * After implementing the background, I sought to implement the window functionality.
 * Finally, to complete the basic renderer I implemented OAM scanning and sprite functionality.
 * At this point, I tried to test the DMG-Acid2 ROM, however, the results and errors found from testing were most likely not problems with implementation, but rather the inherent problems from rendering out of a memory dump. Getting this test to render properly as a goal was moved to be done during PPU testing and evaluation.

#### Background Renderer (1-4), Background Scrolling (5-6), Window Renderer (7), Sprite Renderer (8), Palette Support (9-11), DMG-Acid2 Test (12)
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





### Designing the Software Structure.
  * At this point in the project, I spent a lot of time researching the source code of more mature emulators to figure out how they were implemented. I'm not a fan of using many files, so I wanted to base everything on a few basic components. 
  * These components would be held in individual.c and .h files, containing a struct definition for each element and the functions that allowed the components to work.
  * One major difference between my emulator and a lot of others was the focus on using one large uint8_t SystemMemory[0x10000] array. The primary reason for this was to allow for easier implementation of save-state support later on, and also because it felt more true to the actual system as far as I read documentation-wise in terms of the way addressing modes and the Gameboy memory map worked. While this was perhaps not the optimal choice for performance, I thought it was cool.
  * Ultimately, after reading the Pan Docs a few times and cross-referencing other mature emulators, I decided on this software structure.
  
### Implementing Main
  * Getting basic console functionality was imperative for this project. I wanted users to be able to choose ROM files, Save files, Palettes, Control help, and read cartridge information before actually launching the games.
  * Getting this done was rather simple, and only took a couple of hours. It also helped me understand how I would load and process files given by users, which was very useful for MMU implementation.

### Implementing the MMU 
  * The first major component I worked on in the system was the MMU. This was because I figured having the ability to load roms would be imperative for developing more difficult components later on.
  * The MMU ended up being one of the largest and most complicated components to implement by far, and I really enjoyed think about the challenges.
  * One of my favorite challenges was setting up Memory Banking support, and I chose to implement this by loading the ROM bank data from another array to the SystemMemory array when requested.
  * Aside from this, I wrote basic DMA functions and basic MMURead and MMUWrite functions for the CPU to use. This was interesting because I had most of the other components interface with system memory directly, but I figured that there were many cases (such as CPU reading/writing from VRAM in PPU mode 3) for me to justify writing dedicated functions.

### Implementing the PPU 
  * Originally, I thought the PPU would be the component that would take the most time. I think was right, however, my previous implementation of the VRAM Reader made this component significantly easier to implement.
  * In essence, I converted the functions in the VRAM reader to operate on a pixel-by-pixel basis, and I really only needed to implement correct timing for the modes alongside raising interrupt flags when necessary.
  * I had some previous experience using SDL for the CHIP-8 emulator and VRAM Reader, so it was a breeze setting up the rendering and GUI this time. I found that rendering an image pixel by pixel was a bit too slow, so even though it could cause accuracy issues in edge-case games, I chose to only render on a scanline-by-scanline basis.
  * The PPU ended up also not using the MMURead and MMUWrite functions and instead interfaced with system memory directly. I did this because it was a little easier to code, and I figured all the extra function calls wouldn't be an issue down the line.
  * One of the major things that the PPU got me to think of was the idea of Machine Cycles vs Ticks. It turns out that a lot of documentation on the Gameboy focuses on machine cycles, (4 system ticks), even though a lot of components rely on system ticks to operate. For example, a given opcode may take 1 machine cycle, but that would also activate 4 ticks on the ppu, drawing 4 pixels. Another opcode could take 2 machine cycles, and would as such activate 8 ticks on the PPU, drawing 8 pixels.
  * Thinking about this also helped me think about how I would fix timing issues if they occurred, and I ultimately just chose to loop the other system components for however many ticks the CPU took to operate a given opcode.

### Implementing the Gamepad and the timer
  * The gamepad was a breeze, I just had SDL check the keyboard before each SystemTick and updated the appropriate register in system memory accordingly.
  * I ended up adding it to MMU.c, as I figured opcodes like "stop" would rely on checking for an update to the gamepad.
  * The timer wasn't as easy, but it only took an hour or two to understand and set up. The previous knowledge I acquired about ticks vs machine cycles was also relevant here, and I thought it was an interesting technical challenge to convert the two.

### Implementing the CPU and Interrupt Handling

### Testing

### Audio Support (To Do)

### MBC Features (To Do)

### Pixel Fifo (To Do)

## Reference:
   * https://github.com/jacoblister/emuboy <- I found his journey to create a Gameboy emulator incredibly useful in determining which order I tackled the technical problems in this project.
   * https://github.com/gameplaycolor/gameplaycolor <- Inspiration for this project, I have fond memories of using this emulator to play a dump of my brother's Pokemon Blue Cartridge on my middle school iPad.
   * [Pandocs](https://gbdev.io/pandocs/)
   * [Ultimate Gameboy Talk](https://www.youtube.com/watch?v=HyzD8pNlpwI)
   * r/EmuDev and the Emulator Development Discord Server.
