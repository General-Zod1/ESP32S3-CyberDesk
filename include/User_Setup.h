// TFT_eSPI User Setup for ESP32-S3 + ST7789V 2.4" Portrait
// Pins: CS=10, RST=5, DC=7, MOSI=11, MISO=13, SCLK=12, BL=38

#define USER_SETUP_LOADED 1

// Driver
#define ST7789_DRIVER

// Display dimensions (PORTRAIT: 240 wide x 320 tall)
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// HSPI pins
#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_CS   10
#define TFT_DC    7
#define TFT_RST   5
#define TFT_MISO 13
#define TFT_BL   38

// Use default FSPI port (shared with touch)
// #define USE_HSPI_PORT

// Color inversion
#define TFT_INVERSION_ON

// SPI frequency
#define SPI_FREQUENCY      80000000
#define SPI_READ_FREQUENCY 20000000
#define SPI_TOUCH_FREQUENCY  2500000

// Fonts
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT
