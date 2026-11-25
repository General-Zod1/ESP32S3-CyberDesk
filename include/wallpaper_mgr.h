// ============================================================
//  wallpaper_mgr.h  —  Custom Wallpaper (JPG & Animated GIF)
//                      Manager for ST7789V 2.4" TFT with LittleFS
// ============================================================
#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <LittleFS.h>
#include <TJpg_Decoder.h>
#include <AnimatedGIF.h>
#include <Preferences.h>
#include <vector>
#include "config.h"
#include "colors.h"

enum WallpaperMode {
    WALL_MODE_MATRIX = 0,
    WALL_MODE_IMAGE  = 1,
    WALL_MODE_GIF    = 2
};

// Static file handle and TFT pointer for decoder callbacks
static File       _activeGifFile;
static TFT_eSPI*  _wallTft = nullptr;

// ── TJpg_Decoder Callback ────────────────────────────────────
static bool _tftJpgOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    if (!_wallTft) return false;
    if (y >= SCREEN_H) return false;
    _wallTft->pushImage(x, y, w, h, bitmap);
    return true;
}

// ── AnimatedGIF Callbacks ────────────────────────────────────
static void* _gifOpen(const char *szFilename, int32_t *pFileSize) {
    _activeGifFile = LittleFS.open(szFilename, "r");
    if (_activeGifFile) {
        *pFileSize = _activeGifFile.size();
        return (void*)&_activeGifFile;
    }
    return NULL;
}

static void _gifClose(void *pHandle) {
    if (_activeGifFile) {
        _activeGifFile.close();
    }
}

static int32_t _gifRead(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
    int32_t iBytesRead = iLen;
    if ((pFile->iSize - pFile->iPos) < iLen)
        iBytesRead = pFile->iSize - pFile->iPos;
    if (iBytesRead <= 0) return 0;
    _activeGifFile.seek(pFile->iPos);
    int32_t bytesRead = _activeGifFile.read(pBuf, iBytesRead);
    pFile->iPos += bytesRead;
    return bytesRead;
}

static int32_t _gifSeek(GIFFILE *pFile, int32_t iPosition) {
    if (iPosition < 0) iPosition = 0;
    else if (iPosition >= pFile->iSize) iPosition = pFile->iSize - 1;
    pFile->iPos = iPosition;
    _activeGifFile.seek(pFile->iPos);
    return iPosition;
}

static void _gifDraw(GIFDRAW *pDraw) {
    if (!_wallTft) return;
    int x, y, iWidth;
    iWidth = pDraw->iWidth;
    if (iWidth + pDraw->iX > SCREEN_W)
        iWidth = SCREEN_W - pDraw->iX;
    y = pDraw->iY + pDraw->y;
    if (y >= SCREEN_H || pDraw->iX >= SCREEN_W || iWidth < 1) return;

    uint8_t *s = pDraw->pPixels;
    uint16_t *usPalette = pDraw->pPalette;

    if (pDraw->ucDisposalMethod == 2) { // Restore to background
        for (x = 0; x < iWidth; x++) {
            if (s[x] == pDraw->ucTransparent)
                s[x] = pDraw->ucBackground;
        }
        pDraw->ucHasTransparency = 0;
    }

    static uint16_t lineBuf[SCREEN_W];
    if (pDraw->ucHasTransparency) {
        uint8_t ucTransparent = pDraw->ucTransparent;
        int startX = 0;
        int len = 0;
        for (x = 0; x < iWidth; x++) {
            if (s[x] != ucTransparent) {
                lineBuf[len++] = usPalette[s[x]];
            } else {
                if (len > 0) {
                    _wallTft->pushImage(pDraw->iX + startX, y, len, 1, lineBuf);
                    len = 0;
                }
                startX = x + 1;
            }
        }
        if (len > 0) {
            _wallTft->pushImage(pDraw->iX + startX, y, len, 1, lineBuf);
        }
    } else {
        for (x = 0; x < iWidth; x++) {
            lineBuf[x] = usPalette[s[x]];
        }
        _wallTft->pushImage(pDraw->iX, y, iWidth, 1, lineBuf);
    }
}

// ============================================================
//  WallpaperMgr Class
// ============================================================
class WallpaperMgr {
public:
    void begin(TFT_eSPI* tft) {
        _tft = tft;
        _wallTft = tft;

        // Mount LittleFS with auto-format if needed
        if (!LittleFS.begin(true)) {
            Serial.println("[WALL] LittleFS Mount Failed");
        } else {
            Serial.printf("[WALL] LittleFS Mounted. Total: %u KB, Used: %u KB\n",
                (uint32_t)(LittleFS.totalBytes() / 1024),
                (uint32_t)(LittleFS.usedBytes() / 1024));
        }

        // Setup TJpg_Decoder
        TJpgDec.setJpgScale(1);
        TJpgDec.setSwapBytes(true);
        TJpgDec.setCallback(_tftJpgOutput);

        // Setup AnimatedGIF
        _gif.begin(GIF_PALETTE_RGB565_BE);

        // Load saved preferences
        _prefs.begin("wall", false);
        _mode = (WallpaperMode)_prefs.getUChar("mode", (uint8_t)WALL_MODE_MATRIX);
        _activeFile = _prefs.getString("file", "");
        _clockOverlay = _prefs.getBool("clock_ovr", true);

        refreshFileList();

        // If no file set but files exist, select first
        if (_activeFile.length() == 0 && !_fileList.empty()) {
            _activeFile = _fileList[0];
            _detectModeForFile(_activeFile);
        }

        if (_mode == WALL_MODE_GIF && _activeFile.length() > 0) {
            _openGif(_activeFile.c_str());
        }
    }

    void refreshFileList() {
        _fileList.clear();
        File root = LittleFS.open("/");
        if (!root || !root.isDirectory()) return;

        File f = root.openNextFile();
        while (f) {
            String name = f.name();
            if (!name.startsWith("/")) name = "/" + name;
            String lower = name;
            lower.toLowerCase();
            if (lower.endsWith(".jpg") || lower.endsWith(".jpeg") || lower.endsWith(".gif")) {
                _fileList.push_back(name);
            }
            f = root.openNextFile();
        }
    }

    const std::vector<String>& getFiles() const { return _fileList; }

    WallpaperMode getMode() const { return _mode; }

    void setMode(WallpaperMode m) {
        _mode = m;
        _prefs.putUChar("mode", (uint8_t)_mode);
        if (_mode == WALL_MODE_GIF && _activeFile.length() > 0) {
            _openGif(_activeFile.c_str());
        }
        _dirty = true;
    }

    void cycleMode() {
        uint8_t next = ((uint8_t)_mode + 1) % 3;
        setMode((WallpaperMode)next);
    }

    const char* getModeName() const {
        switch (_mode) {
            case WALL_MODE_MATRIX: return "MATRIX RAIN";
            case WALL_MODE_IMAGE:  return "CUSTOM PIC";
            case WALL_MODE_GIF:    return "ANIMATED GIF";
        }
        return "UNKNOWN";
    }

    String getActiveFile() const { return _activeFile; }

    bool setActiveFile(const String& filename) {
        String path = filename;
        if (!path.startsWith("/")) path = "/" + path;

        if (!LittleFS.exists(path)) {
            Serial.printf("[WALL] File does not exist: %s\n", path.c_str());
            return false;
        }

        _activeFile = path;
        _prefs.putString("file", _activeFile);
        _detectModeForFile(_activeFile);
        refreshFileList();

        if (_mode == WALL_MODE_GIF) {
            _openGif(_activeFile.c_str());
        }

        _dirty = true;
        Serial.printf("[WALL] Active wallpaper set to: %s (mode: %s)\n",
            _activeFile.c_str(), getModeName());
        return true;
    }

    void nextFile() {
        if (_fileList.empty()) return;
        int idx = _findFileIndex(_activeFile);
        idx = (idx + 1) % _fileList.size();
        setActiveFile(_fileList[idx]);
    }

    void prevFile() {
        if (_fileList.empty()) return;
        int idx = _findFileIndex(_activeFile);
        idx = (idx + _fileList.size() - 1) % _fileList.size();
        setActiveFile(_fileList[idx]);
    }

    bool deleteFile(const String& filename) {
        String path = filename;
        if (!path.startsWith("/")) path = "/" + path;
        if (LittleFS.exists(path)) {
            if (_activeFile == path) {
                _gif.close();
                _gifOpenState = false;
            }
            LittleFS.remove(path);
            refreshFileList();
            if (_activeFile == path) {
                if (!_fileList.empty()) {
                    setActiveFile(_fileList[0]);
                } else {
                    _activeFile = "";
                    _prefs.putString("file", "");
                    setMode(WALL_MODE_MATRIX);
                }
            }
            return true;
        }
        return false;
    }

    bool hasClockOverlay() const { return _clockOverlay; }
    void setClockOverlay(bool enable) {
        _clockOverlay = enable;
        _prefs.putBool("clock_ovr", _clockOverlay);
        _dirty = true;
    }
    void toggleClockOverlay() { setClockOverlay(!_clockOverlay); }

    void markDirty() { _dirty = true; }
    bool isDirty() const { return _dirty; }

    // Renders the background wallpaper onto screen
    void renderBackground(bool force = false) {
        if (_mode == WALL_MODE_MATRIX) return;

        if (_mode == WALL_MODE_IMAGE) {
            if (_dirty || force) {
                _dirty = false;
                if (_activeFile.length() > 0 && LittleFS.exists(_activeFile)) {
                    uint16_t w = 0, h = 0;
                    TJpgDec.getFsJpgSize(&w, &h, _activeFile.c_str(), LittleFS);
                    int16_t x = max(0, (SCREEN_W - (int)w) / 2);
                    int16_t y = max(0, (SCREEN_H - (int)h) / 2);
                    TJpgDec.drawFsJpg(x, y, _activeFile.c_str(), LittleFS);
                } else {
                    drawProceduralCyberpunk(*_tft);
                }
            }
        } else if (_mode == WALL_MODE_GIF) {
            updateGifFrame();
        }
    }

    // Step GIF frame according to frame timing
    void updateGifFrame() {
        if (_mode != WALL_MODE_GIF) return;

        uint32_t now = millis();
        if (now < _nextGifFrameTime) return;

        if (!_gifOpenState && _activeFile.length() > 0) {
            _openGif(_activeFile.c_str());
        }

        if (_gifOpenState) {
            int delayMs = 50;
            int res = _gif.playFrame(true, &delayMs);
            if (res == 0) {
                // End of animation -> rewind for continuous loop
                _gif.reset();
            }
            if (delayMs < 20) delayMs = 50;
            _nextGifFrameTime = now + delayMs;
        } else {
            // If no GIF exists, draw animated procedural neon cyberpunk
            static uint32_t lastProc = 0;
            if (now - lastProc > 60) {
                lastProc = now;
                drawProceduralCyberpunk(*_tft);
            }
        }
    }

    // Built-in procedural Cyberpunk Synthwave sunset & grid (no files needed!)
    static void drawProceduralCyberpunk(TFT_eSPI& tft) {
        static uint8_t animStep = 0;
        animStep = (animStep + 1) % 16;

        // Dark sky with stars
        tft.fillRect(0, 0, SCREEN_W, 140, C_BG);

        // Neon Cyber Sun (gradient bands)
        int16_t sunX = SCREEN_W / 2;
        int16_t sunY = 90;
        int16_t sunR = 40;
        for (int r = sunR; r > 0; r -= 2) {
            uint16_t col = blend565(C_YELLOW, C_MAGENTA, (r * 255) / sunR);
            tft.drawCircle(sunX, sunY, r, col);
        }
        // Sun horizontal laser cuts
        for (int cut = 70; cut < 120; cut += 6) {
            tft.drawFastHLine(sunX - sunR, cut, sunR * 2, C_BG);
        }

        // Horizon neon glow line
        tft.drawFastHLine(0, 140, SCREEN_W, C_MAGENTA);
        tft.drawFastHLine(0, 141, SCREEN_W, C_CYAN);

        // Perspective Synthwave Grid (y = 142 .. 276)
        tft.fillRect(0, 142, SCREEN_W, 134, C_BG);

        // Moving horizontal perspective lines
        for (int i = 1; i <= 7; i++) {
            int py = 142 + (i * i * 134) / 49 + (animStep * i) / 16;
            if (py >= 142 && py < 276) {
                uint16_t gc = blend565(C_CYAN, C_MAGENTA, i * 36);
                tft.drawFastHLine(0, py, SCREEN_W, gc);
            }
        }

        // Converging vertical grid lines
        for (int vx = -4; vx <= 4; vx++) {
            int xTop = sunX + vx * 12;
            int xBot = sunX + vx * 65;
            tft.drawLine(xTop, 142, xBot, 276, C_MAGENTA);
        }
    }

private:
    TFT_eSPI*            _tft = nullptr;
    Preferences          _prefs;
    WallpaperMode        _mode = WALL_MODE_MATRIX;
    String               _activeFile = "";
    bool                 _clockOverlay = true;
    bool                 _dirty = true;
    std::vector<String>  _fileList;

    AnimatedGIF          _gif;
    bool                 _gifOpenState = false;
    uint32_t             _nextGifFrameTime = 0;

    void _detectModeForFile(const String& path) {
        String lower = path;
        lower.toLowerCase();
        if (lower.endsWith(".gif")) {
            setMode(WALL_MODE_GIF);
        } else if (lower.endsWith(".jpg") || lower.endsWith(".jpeg")) {
            setMode(WALL_MODE_IMAGE);
        }
    }

    void _openGif(const char* path) {
        if (_gifOpenState) {
            _gif.close();
            _gifOpenState = false;
        }
        if (LittleFS.exists(path)) {
            if (_gif.open(path, _gifOpen, _gifClose, _gifRead, _gifSeek, _gifDraw)) {
                _gifOpenState = true;
                _nextGifFrameTime = millis();
                Serial.printf("[WALL] GIF opened: %dx%d canvas\n",
                    _gif.getCanvasWidth(), _gif.getCanvasHeight());
            } else {
                Serial.printf("[WALL] Failed to open GIF: %s\n", path);
            }
        }
    }

    int _findFileIndex(const String& path) const {
        for (size_t i = 0; i < _fileList.size(); i++) {
            if (_fileList[i] == path) return i;
        }
        return 0;
    }
};
