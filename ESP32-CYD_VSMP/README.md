# ESP32-CYD Very Slow Movie Player

A digital picture frame that displays movie frames in extreme slow motion on the ESP32 Cheap Yellow Display (CYD). Inspired by Tom Whitwell's [Very Slow Movie Player](https://github.com/TomWhitwell/SlowMovie) for Raspberry Pi and e-ink displays.

![ESP32-CYD displaying a dithered frame from Un Chien Andalou](docs/example.jpg)

## What is this?

The Very Slow Movie Player concept, originally created by [Bryan Boyer](https://medium.com/s/story/very-slow-movie-player-499f76c48b62), plays films at 24 frames per hour instead of 24 frames per second. A feature-length movie takes days or weeks to complete, transforming cinema into something closer to a slowly changing artwork on your wall.

This adaptation brings that concept to the affordable ESP32-CYD board with its built-in TFT colour display and SD card slot, while preserving the aesthetic appeal of the original through Floyd-Steinberg dithering.

## Hardware Required

- **ESP32-2432S028R** (aka "Cheap Yellow Display" / CYD) - [Purchase link](https://makeradvisor.com/tools/cyd-cheap-yellow-display-esp32-2432s028r/)
- **MicroSD card** (FAT16 or FAT32 formatted, 1GB+ recommended)
- **USB cable** for programming and power
- **Computer** for preprocessing movie frames (Mac, Windows, or Linux)

## Features

- **Classic B&W Floyd-Steinberg dithering** for that authentic VSMP aesthetic
- **Adjustable playback speed** - from seconds to hours between frames
- **Auto-looping** - seamlessly restarts when the movie ends
- **Power-efficient** - can run 24/7 from USB power
- **No WiFi required** - fully standalone once programmed

## Quick Start

### 1. Install Prerequisites

**On your computer:**

- **Python 3** with pip
- **FFmpeg** for video processing
- **Arduino IDE 2.x** for programming the ESP32

```bash
# Install Python libraries
pip3 install ffmpeg-python pillow

# Install FFmpeg
# macOS:
brew install ffmpeg

# Windows (with Chocolatey):
choco install ffmpeg

# Linux (Ubuntu/Debian):
sudo apt install ffmpeg
```

**In Arduino IDE:**

1. Install ESP32 board support:
   - Open **File > Preferences**
   - Add to "Additional boards manager URLs": 
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - Go to **Tools > Board > Boards Manager**
   - Search for "esp32" and install **esp32 by Espressif Systems**

2. Install TFT_eSPI library:
   - Go to **Sketch > Include Library > Manage Libraries**
   - Search for "TFT_eSPI" and install **TFT_eSPI by Bodmer**

3. Configure TFT_eSPI for the CYD:
   - Download the correct `User_Setup.h` from [Random Nerd Tutorials](https://randomnerdtutorials.com/cheap-yellow-display-esp32-2432s028r/)
   - Replace the existing file in `Arduino/libraries/TFT_eSPI/User_Setup.h`
   - **Important:** Use the exact file provided - other configs will not work

### 2. Prepare Your Movie

Extract and process frames from your movie file:

```bash
# Quick test (every 100th frame for fast results)
python3 extract_frames.py --input my_movie.mp4 --output ./frames_test --increment 100

# Full VSMP experience (every 4th frame, SlowMovie default)
python3 extract_frames.py --input my_movie.mp4 --output ./frames --increment 4

# Adjust contrast for better dithering (recommended for colour films)
python3 extract_frames.py --input my_movie.mp4 --output ./frames --increment 4 --contrast 1.8
```

**Options:**
- `--input` - Path to your video file (mp4, mkv, avi, mov, etc.)
- `--output` - Directory to save frames (will be created if needed)
- `--increment` - Extract every Nth frame (4 = ~6 frames/minute of film, 10 = ~2.4 frames/minute)
- `--contrast` - Adjust contrast (1.0 = no change, 1.5-2.0 recommended for dithering)
- `--colour` - Use colour dithering instead of B&W (experimental)

**Processing time:** Depends on movie length and your computer speed. A 2-hour movie with `--increment 4` might take 30 mins to 2 hours to process. You can let it run overnight.

The script creates:
- `frames/` - Folder containing numbered BMP files (00000.bmp, 00001.bmp, etc.)
- `manifest.txt` - File telling the ESP32 how many frames exist

### 3. Prepare the SD Card

1. **Format** the SD card as **FAT32** (or FAT16 if < 2GB)
   - On Mac: Use Disk Utility, format as "MS-DOS (FAT)"
   - On Windows: Right-click drive, Format, select FAT32
   
2. **Copy files** to the SD card root:
   ```
   SD Card Root/
   ├── frames/
   │   ├── 00000.bmp
   │   ├── 00001.bmp
   │   └── ...
   └── manifest.txt
   ```

3. **Eject safely** before removing the card

### 4. Program the ESP32-CYD

1. Open `esp32_cyd_slow_movie/esp32_cyd_slow_movie.ino` in Arduino IDE

2. Adjust settings at the top of the sketch:
   ```cpp
   #define FRAME_DELAY_MS       30000    // 30 seconds (adjust to taste)
   #define BACKLIGHT_BRIGHTNESS    180    // 0-255
   ```

3. Configure Arduino IDE (**Tools** menu):
   - **Board:** ESP32 Dev Module
   - **Port:** Select your CYD's USB port
   - **Upload Speed:** 115200

4. **Insert SD card** into the CYD

5. **Upload** the sketch (click the → arrow)
   - If upload fails, hold the **BOOT** button while clicking upload
   - Release when you see "Connecting..." change to "Writing..."

6. **Open Serial Monitor** (Tools > Serial Monitor, 115200 baud) to watch progress

### 5. Enjoy!

The CYD should display:
- "Loading..." briefly
- Then start showing frames from your movie
- Each frame displays for the configured delay
- Automatically loops back to the start when finished

## Advanced: Sourcing Movies with yt-dlp

**Legal Notice:** Only download content you have the right to access. Respect copyright laws in your jurisdiction.

[yt-dlp](https://github.com/yt-dlp/yt-dlp) is a powerful tool for downloading videos from YouTube and many other sites, particularly useful for finding public domain films.

**Install yt-dlp:**
```bash
# macOS (Homebrew):
brew install yt-dlp

# Windows (Chocolatey):
choco install yt-dlp

# Linux / pip:
pip3 install yt-dlp
```

**Example: Download a public domain film**
```bash
# Download in best quality
yt-dlp "https://www.youtube.com/watch?v=EXAMPLE"

# Download specific format
yt-dlp -f "bestvideo[ext=mp4]+bestaudio[ext=m4a]/best[ext=mp4]" URL

# Download as MP4 (most compatible)
yt-dlp --merge-output-format mp4 URL
```

**Great sources for public domain films:**
- [Internet Archive](https://archive.org/details/movies) - Massive collection of public domain films
- [Project Gutenberg](https://www.gutenberg.org/) - Some films available
- YouTube channels dedicated to public domain content

**Recommendations for VSMP:**
- **Silent films** work beautifully (already B&W, high contrast)
- **Film noir** - strong shadows and lighting
- **Animated films** - bold shapes and clear forms
- **Experimental/art films** - visually striking compositions

## Troubleshooting

### "SD Card Error" on display
- Check SD card is properly inserted
- Verify SD card is formatted as FAT32 (or FAT16)
- Try a different SD card
- Check files copied correctly

### "No frames found!"
- Verify `frames/` folder exists on SD card root
- Check files are named correctly (00000.bmp, 00001.bmp, etc.)
- Ensure `manifest.txt` exists in SD card root

### Screen is blank but Serial Monitor shows activity
- Check TFT_eSPI `User_Setup.h` is correct for CYD
- Try adjusting `BACKLIGHT_BRIGHTNESS` (increase to 255)
- Verify the sketch uses `digitalWrite` for backlight, not PWM

### Images appear as negatives (inverted black/white)
- This is fixed in the provided sketch (line: `tft.color565(255-r, 255-g, 255-b)`)
- If still inverted, check you're using the latest version

### Upload fails / won't connect
- Hold **BOOT** button while clicking upload
- Change **Upload Speed** to 115200
- Try a different USB cable (data cable, not charge-only)

### Frame extraction is slow
- This is normal - can take 30min to 2+ hours for feature films
- Let it run in background or overnight
- Test with `--increment 100` first for quick results

## Configuration Options

### Playback Speed

Edit `FRAME_DELAY_MS` in the Arduino sketch:
```cpp
#define FRAME_DELAY_MS  10000   // 10 seconds (testing)
#define FRAME_DELAY_MS  30000   // 30 seconds
#define FRAME_DELAY_MS  60000   // 1 minute
#define FRAME_DELAY_MS  120000  // 2 minutes (classic VSMP)
#define FRAME_DELAY_MS  300000  // 5 minutes (very slow)
```

**How long will my movie take?**

For a 90-minute film:
- `--increment 4` extracts ~1,350 frames
- At 30 seconds/frame = **11.25 hours** total
- At 2 minutes/frame = **45 hours** (classic VSMP speed)
- At 5 minutes/frame = **112.5 hours** (~4.5 days)

### Skip Blank Frames at Start

If your extraction includes blank frames, edit this line:
```cpp
int currentFrame = 0;    // Change to 3, 5, etc. to skip ahead
```

### Backlight Brightness

Adjust brightness (0-255):
```cpp
#define BACKLIGHT_BRIGHTNESS 180  // Lower = dimmer, saves power
```

### Dithering Style

The Python script defaults to **B&W Floyd-Steinberg** dithering (faithful to original VSMP). To experiment:

```bash
# Colour dithering (retro/pixel art aesthetic)
python3 extract_frames.py --input movie.mp4 --output ./frames --colour

# Adjust contrast for better B&W results
python3 extract_frames.py --input movie.mp4 --output ./frames --contrast 2.0
```

## Credits & Inspiration

This project is inspired by and adapts the concept from:

- **Bryan Boyer** - Original [Very Slow Movie Player](https://medium.com/s/story/very-slow-movie-player-499f76c48b62) concept (2018)
- **Tom Whitwell** - [SlowMovie GitHub repository](https://github.com/TomWhitwell/SlowMovie) for Raspberry Pi + e-ink (2020)

Hardware and software guidance:
- **Random Nerd Tutorials** - [ESP32-CYD setup guide](https://randomnerdtutorials.com/cheap-yellow-display-esp32-2432s028r/)
- **Bodmer** - [TFT_eSPI library](https://github.com/Bodmer/TFT_eSPI)

## License

MIT License - feel free to modify and share

## Contributing

Found a bug? Want to improve the documentation? Pull requests welcome!

## Gallery

Share your VSMP setups! Open an issue with a photo of your build and what movie you're playing.

---

**Enjoy your Very Slow Movie Player!** There's something meditative about watching cinema unfold at this glacial pace.
