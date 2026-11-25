// ============================================================
//  colors.h  —  Cyberpunk / Blade Runner palette
// ============================================================
#pragma once
#include <TFT_eSPI.h>

// TFT_eSPI uses RGB565
// Cyberpunk neon palette
#define C_BG           0x0000   // Pure black
#define C_BG_DARK      0x0821   // Very dark blue-black
#define C_CYAN         0x07FF   // Neon cyan
#define C_CYAN_DIM     0x0557   // Dimmed cyan
#define C_MAGENTA      0xF81F   // Neon magenta/pink
#define C_MAGENTA_DIM  0x7809   // Dimmed magenta
#define C_YELLOW       0xFFE0   // Neon yellow
#define C_ORANGE       0xFC60   // Neon orange
#define C_GREEN        0x07E0   // Matrix green
#define C_GREEN_DIM    0x0320   // Dim matrix green
#define C_RED          0xF800   // Alarm red
#define C_WHITE        0xFFFF   // White
#define C_GREY         0x7BEF   // Mid grey
#define C_DARK_GREY    0x2104   // Dark grey
#define C_PANEL        0x0841   // Panel background (dark blue)
#define C_PANEL_BORDER 0x07FF   // Cyan border

// Blade Runner amber/orange
#define C_BR_AMBER     0xFD20   // Amber
#define C_BR_ORANGE    0xFC80   // Warm orange
#define C_BR_RED       0xF8C0   // Dusty red

// RGB565 helpers
inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Alpha-blend two RGB565 colours (a = 0..255)
inline uint16_t blend565(uint16_t fg, uint16_t bg, uint8_t a) {
    uint8_t ri = (fg >> 11) & 0x1F;
    uint8_t gi = (fg >> 5)  & 0x3F;
    uint8_t bi =  fg        & 0x1F;
    uint8_t ro = (bg >> 11) & 0x1F;
    uint8_t go = (bg >> 5)  & 0x3F;
    uint8_t bo =  bg        & 0x1F;
    uint8_t rm = ri + ((ro - ri) * (255 - a) / 255);
    uint8_t gm = gi + ((go - gi) * (255 - a) / 255);
    uint8_t bm = bi + ((bo - bi) * (255 - a) / 255);
    return (rm << 11) | (gm << 5) | bm;
}
