# Quick Reference

## Extract Frames

```bash
# Test extraction (fast, for testing)
python3 extract_frames.py --input movie.mp4 --output ./test --increment 100

# Full extraction (classic VSMP speed)
python3 extract_frames.py --input movie.mp4 --output ./frames --increment 4

# High contrast (better for colour films)
python3 extract_frames.py --input movie.mp4 --output ./frames --increment 4 --contrast 1.8
```

## Configure Playback Speed

Edit `esp32_cyd_slow_movie.ino` line 32:

```cpp
#define FRAME_DELAY_MS  30000   // 30 seconds
#define FRAME_DELAY_MS  60000   // 1 minute  
#define FRAME_DELAY_MS  120000  // 2 minutes (classic)
#define FRAME_DELAY_MS  300000  // 5 minutes
```

## Skip Blank Frames

Edit line 38:

```cpp
#define START_FRAME  0    // Start from beginning
#define START_FRAME  3    // Skip first 3 frames
#define START_FRAME  5    // Skip first 5 frames
```

## How Long Will My Movie Take?

**Formula:** `(total_frames × delay_ms) / 1000 / 60 = minutes`

**90-minute film examples:**

| Increment | Frames | 30sec delay | 2min delay | 5min delay |
|-----------|--------|-------------|------------|------------|
| 4         | ~1350  | 11.25 hours | 45 hours   | 112.5 hours|
| 10        | ~540   | 4.5 hours   | 18 hours   | 45 hours   |
| 25        | ~216   | 1.8 hours   | 7.2 hours  | 18 hours   |

## Troubleshooting Quick Fixes

**Blank screen:**
- Check SD card is inserted
- Try another SD card  
- Increase `BACKLIGHT_BRIGHTNESS` to 255

**"SD Card Error":**
- Reformat SD as FAT32
- Check files copied correctly
- Ensure `manifest.txt` is in root

**Images inverted (negative):**
- Fixed in provided sketch (line 189)
- Check: `tft.color565(255-r, 255-g, 255-b)`

**Upload fails:**
- Hold BOOT button during upload
- Change Upload Speed to 115200
- Try different USB cable

## Download Public Domain Films

```bash
# Install yt-dlp
brew install yt-dlp  # macOS
pip3 install yt-dlp  # cross-platform

# Download film
yt-dlp --merge-output-format mp4 "VIDEO_URL"
```

**Sources:**
- Internet Archive: https://archive.org/details/movies
- Search: "public domain film noir"
- Search: "public domain silent film"
