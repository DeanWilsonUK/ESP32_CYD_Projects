/*
 * ESP32-CYD Very Slow Movie Player
 * =================================
 * Displays pre-processed, dithered movie frames from SD card on the
 * ESP32 Cheap Yellow Display (ESP32-2432S028R).
 *
 * Inspired by Tom Whitwell's SlowMovie for Raspberry Pi + e-ink
 * https://github.com/TomWhitwell/SlowMovie
 *
 * Hardware:
 *   - ESP32-2432S028R (Cheap Yellow Display / CYD)
 *   - MicroSD card with extracted frames
 *
 * Libraries required (install via Arduino Library Manager):
 *   - TFT_eSPI by Bodmer (MUST configure User_Setup.h for CYD)
 *   - SD (built into ESP32 board support)
 *
 * Setup instructions:
 *   https://github.com/yourusername/ESP32-CYD_VSMP
 */

#include <SPI.h>
#include <SD.h>
#include <TFT_eSPI.h>

// ============================================================
// Configuration - adjust these to your preference
// ============================================================

// Delay between frames in milliseconds
// 10000  = 10 seconds (good for testing)
// 30000  = 30 seconds
// 60000  = 1 minute
// 120000 = 2 minutes (classic VSMP speed)
// 300000 = 5 minutes (very slow)
#define FRAME_DELAY_MS       30000

// Backlight brightness 0-255
#define BACKLIGHT_BRIGHTNESS    180

// Backlight dimming during frame transitions
// Set to false to disable the subtle dim/brighten effect
#define DIM_ON_TRANSITION      true
#define DIM_BRIGHTNESS           20
#define DIM_DURATION_MS         200

// Start from this frame number (useful to skip blank frames at beginning)
// Change to 3, 5, etc. if your extraction has blank frames at the start
#define START_FRAME               0

// ============================================================
// Pin definitions for ESP32-CYD
// ============================================================
#define SD_CS_PIN             5
#define SD_SCK_PIN           18
#define SD_MISO_PIN          19
#define SD_MOSI_PIN          23
#define TFT_BACKLIGHT_PIN    21

// ============================================================
// Globals
// ============================================================
TFT_eSPI tft = TFT_eSPI();
SPIClass sdSPI(HSPI);

int totalFrames  = 0;
int currentFrame = START_FRAME;

// ============================================================
// Backlight control
// ============================================================
void setBacklight(int brightness) {
    ledcWrite(TFT_BACKLIGHT_PIN, brightness);
}

// ============================================================
// SD card helpers
// ============================================================
bool initSDCard() {
    sdSPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    if (!SD.begin(SD_CS_PIN, sdSPI)) {
        Serial.println("SD card init failed!");
        return false;
    }
    Serial.println("SD card initialised.");
    return true;
}

int readManifest() {
    if (!SD.exists("/manifest.txt")) {
        Serial.println("No manifest.txt found, counting frames...");
        return countFrames();
    }
    File f = SD.open("/manifest.txt");
    if (!f) return 0;
    int frames = 0;
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.startsWith("frames=")) {
            frames = line.substring(7).toInt();
            Serial.printf("Manifest: %d frames\n", frames);
        }
    }
    f.close();
    return frames;
}

int countFrames() {
    File dir = SD.open("/frames");
    if (!dir) { 
        Serial.println("No /frames directory!"); 
        return 0; 
    }
    int count = 0;
    while (true) {
        File entry = dir.openNextFile();
        if (!entry) break;
        String name = String(entry.name());
        name.toLowerCase();
        if (name.endsWith(".bmp")) count++;
        entry.close();
    }
    dir.close();
    Serial.printf("Counted %d frames\n", count);
    return count;
}

// ============================================================
// Safe multi-byte reads (avoids alignment crashes on ESP32)
// ============================================================
uint16_t read16(File &f) {
    uint8_t lo = f.read();
    uint8_t hi = f.read();
    return (uint16_t)lo | ((uint16_t)hi << 8);
}

uint32_t read32(File &f) {
    uint8_t b0 = f.read();
    uint8_t b1 = f.read();
    uint8_t b2 = f.read();
    uint8_t b3 = f.read();
    return (uint32_t)b0 | ((uint32_t)b1 << 8) | 
           ((uint32_t)b2 << 16) | ((uint32_t)b3 << 24);
}

// ============================================================
// BMP display
// Reads a 24-bit BMP file and draws to TFT display
// Note: Inverts colours (255-r, 255-g, 255-b) to correct
//       for the way dithered BMPs are saved
// ============================================================
bool displayBMP(const char* filepath) {
    File f = SD.open(filepath);
    if (!f) {
        Serial.printf("Cannot open: %s\n", filepath);
        return false;
    }

    // Check BMP signature
    if (f.read() != 'B' || f.read() != 'M') {
        Serial.println("Not a BMP file");
        f.close();
        return false;
    }

    // Skip file size and reserved fields
    read32(f);  // File size
    read32(f);  // Reserved
    uint32_t dataOffset = read32(f);  // Pixel data offset

    // Read DIB header
    read32(f);  // Header size
    int32_t imgWidth  = (int32_t)read32(f);
    int32_t imgHeight = (int32_t)read32(f);
    read16(f);  // Colour planes
    uint16_t bpp = read16(f);

    if (bpp != 24) {
        Serial.printf("Expected 24bpp, got %d\n", bpp);
        f.close();
        return false;
    }

    // BMP rows are stored bottom-to-top when height is positive
    bool flipped = (imgHeight > 0);
    if (imgHeight < 0) imgHeight = -imgHeight;

    // Row size is padded to 4-byte boundary
    uint32_t rowSize = ((imgWidth * 3 + 3) / 4) * 4;

    Serial.printf("Drawing BMP: %dx%d pixels\n", imgWidth, imgHeight);

    // Draw pixel by pixel
    // (Slow but reliable and works with limited heap memory)
    for (int row = 0; row < imgHeight; row++) {
        // Calculate source row (BMP is bottom-to-top)
        int srcRow = flipped ? (imgHeight - 1 - row) : row;
        uint32_t rowAddr = dataOffset + (uint32_t)srcRow * rowSize;
        
        f.seek(rowAddr);
        
        for (int col = 0; col < imgWidth; col++) {
            // BMP stores as BGR, read in that order
            uint8_t b = f.read();
            uint8_t g = f.read();
            uint8_t r = f.read();
            
            // Invert colours to correct for dithering
            // (Our BMPs save as white-on-black but display as black-on-white)
            tft.drawPixel(col, row, tft.color565(255-r, 255-g, 255-b));
        }
        
        // Skip padding bytes at end of row
        int padding = rowSize - (imgWidth * 3);
        for (int p = 0; p < padding; p++) f.read();
    }

    f.close();
    return true;
}

// ============================================================
// Setup
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(2000);  // Give Serial time to initialise

    Serial.println("\n============================");
    Serial.println("  ESP32-CYD Slow Movie Player");
    Serial.println("============================\n");

    // Configure backlight with PWM
    ledcAttach(TFT_BACKLIGHT_PIN, 5000, 8);
    setBacklight(DIM_BRIGHTNESS);

    // Initialise display
    tft.init();
    tft.setRotation(1);  // Landscape orientation
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawCentreString("Loading...", 160, 110, 2);

    // Initialise SD card
    if (!initSDCard()) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawCentreString("SD Card Error!", 160, 100, 2);
        tft.setTextSize(1);
        tft.drawCentreString("Check card is inserted", 160, 130, 1);
        tft.drawCentreString("and formatted as FAT32", 160, 145, 1);
        while (true) delay(1000);  // Halt
    }

    // Read number of frames from manifest
    totalFrames = readManifest();
    if (totalFrames == 0) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawCentreString("No frames found!", 160, 100, 2);
        tft.setTextSize(1);
        tft.drawCentreString("Check /frames/ directory", 160, 130, 1);
        while (true) delay(1000);  // Halt
    }

    Serial.printf("Ready: %d frames loaded\n", totalFrames);
    Serial.printf("Frame delay: %d ms (%.1f seconds)\n", 
                  FRAME_DELAY_MS, FRAME_DELAY_MS / 1000.0f);
    Serial.printf("Starting from frame %d\n\n", currentFrame);

    tft.fillScreen(TFT_BLACK);
    setBacklight(BACKLIGHT_BRIGHTNESS);
}

// ============================================================
// Main loop
// ============================================================
void loop() {
    // Build filepath for current frame
    char filepath[32];
    snprintf(filepath, sizeof(filepath), "/frames/%05d.bmp", currentFrame);

    Serial.printf("Frame %d/%d: %s\n", currentFrame + 1, totalFrames, filepath);

    // Display the frame
    bool success = displayBMP(filepath);
    
    if (!success) {
        Serial.printf("Warning: Could not display frame %d\n", currentFrame);
    }

    // Advance to next frame (loops back to 0 at end)
    currentFrame = (currentFrame + 1) % totalFrames;

    // Wait for configured delay
    Serial.printf("Waiting %d seconds for next frame...\n\n", 
                  FRAME_DELAY_MS / 1000);
    
    unsigned long startWait = millis();
    while (millis() - startWait < FRAME_DELAY_MS) {
        delay(100);
    }

    // Brief dim before transition (if enabled)
    if (DIM_ON_TRANSITION) {
        setBacklight(DIM_BRIGHTNESS);
        delay(DIM_DURATION_MS);
        setBacklight(BACKLIGHT_BRIGHTNESS);
    }
}
