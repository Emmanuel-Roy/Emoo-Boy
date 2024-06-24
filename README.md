# Emoo-Boy: A Gameboy Emulator written in C

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

#### Background Renderer (1-3), Background Scrolling (4-5), Window Renderer (6), Sprite Renderer (7)
<img src= "https://github.com/Emmanuel-Roy/Emoo-Boy/assets/54725843/b88b7e96-f4ab-4963-be9f-74e88de82ec8" width="200">
<img src= "https://github.com/Emmanuel-Roy/Emoo-Boy/assets/54725843/21f3ff0b-8fd9-4ebc-9d3f-259d480ec9bd" width="200">
<img src= "https://github.com/Emmanuel-Roy/Emoo-Boy/assets/54725843/7d745293-6f5f-4236-adc6-aa18fda82707" width="200">
<img src= "https://github.com/Emmanuel-Roy/Emoo-Boy/assets/54725843/fd14776d-09b6-4c5f-9d07-fb41d3166ddd" width="200">
<img src= "https://github.com/Emmanuel-Roy/Emoo-Boy/assets/54725843/55fd1cc6-2888-4f58-9a4d-d46c7210a715" width="200">
<img src= "https://github.com/Emmanuel-Roy/Emoo-Boy/assets/54725843/f9ed6ac8-85ef-4395-bc62-1c14d558e0c8" width="200">

### Designing the Software Structure.

### Implementing the ROM and RAM

### Implementing the CPU

### Implementing Timer, Interrupt, and Gamepad Interactions

### Implementing the Interrupt Service Routine in the CPU.

### Completing the PPU and DMA

### Adding Bank Switching Support

### Audio Support (To Do)

## Reference:
   * https://github.com/jacoblister/emuboy <- I found his journey to create a Gameboy emulator incredibly useful in determining which order I tackled the technical problems in this project.
   * https://github.com/gameplaycolor/gameplaycolor <- Inspiration for this project, I have fond memories of using this emulator to play a dump of my brother's Pokemon Blue Cartridge on my middle school iPad.
   * [Pandocs](https://gbdev.io/pandocs/)
   * [Ultimate Gameboy Talk](https://www.youtube.com/watch?v=HyzD8pNlpwI)
   * r/EmuDev and the Emulator Development Discord Server.
