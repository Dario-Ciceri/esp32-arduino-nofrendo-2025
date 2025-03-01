extern "C" {
  #include <nes/nes.h>
}

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// Definizione macro per i pin
#define I2C_PORT_NUM    I2C_NUM_0
#define I2C_PIN_SDA     38
#define I2C_PIN_SCL     39
#define I2C_PIN_INT     40

#define PIN_WR          35
#define PIN_RD          48
#define PIN_RS          36
#define PIN_CS          37
#define PIN_RST         -1
#define PIN_BUSY        -1
#define PIN_BL          45

// Definizione macro per la configurazione
#define PWM_CHANNEL     7
#define FREQ_WRITE      40000000
#define FREQ_PWM        44100
#define TFT_BRIGHTNESS  255  // Valore tra 0 e 255

// Dimensioni dello schermo NES
#define NES_SCREEN_WIDTH  256
#define NES_SCREEN_HEIGHT 240

// Dimensioni del display
#define DISPLAY_WIDTH   480
#define DISPLAY_HEIGHT  320

class LGFX : public lgfx::LGFX_Device {
  lgfx::Bus_Parallel16 _bus_instance;
  lgfx::Panel_ILI9488 _panel_instance;
  lgfx::Light_PWM _light_instance;

  bool init_impl(bool use_reset, bool use_clear) override {
    return lgfx::LGFX_Device::init_impl(use_reset, use_clear);
  }

public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.freq_write = FREQ_WRITE;
      cfg.pin_wr = PIN_WR;
      cfg.pin_rd = PIN_RD;
      cfg.pin_rs = PIN_RS;

      cfg.pin_d0 = 47;
      cfg.pin_d1 = 21;
      cfg.pin_d2 = 14;
      cfg.pin_d3 = 13;
      cfg.pin_d4 = 12;
      cfg.pin_d5 = 11;
      cfg.pin_d6 = 10;
      cfg.pin_d7 = 9;
      cfg.pin_d8 = 3;
      cfg.pin_d9 = 8;
      cfg.pin_d10 = 16;
      cfg.pin_d11 = 15;
      cfg.pin_d12 = 7;
      cfg.pin_d13 = 6;
      cfg.pin_d14 = 5;
      cfg.pin_d15 = 4;
      _bus_instance.config(cfg);
      _panel_instance.bus(&_bus_instance);
    }

    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs = PIN_CS;
      cfg.pin_rst = PIN_RST;
      cfg.pin_busy = PIN_BUSY;
      cfg.offset_rotation = 1;
      cfg.readable = true;
      cfg.invert = false;
      cfg.rgb_order = false;
      cfg.dlen_16bit = true;
      cfg.bus_shared = false;
      _panel_instance.config(cfg);
    }

    {
      auto cfg = _light_instance.config();
      cfg.pin_bl = PIN_BL;
      cfg.invert = false;
      cfg.freq = FREQ_PWM;
      cfg.pwm_channel = PWM_CHANNEL;
      _light_instance.config(cfg);
      _panel_instance.light(&_light_instance);
    }
    setPanel(&_panel_instance);
  }
};

// Crea l'istanza del display
LGFX gfx;

// Array per il mapping dello scaling
static uint16_t scale_x[DISPLAY_WIDTH];
static uint8_t scale_y[DISPLAY_HEIGHT];

extern int16_t bg_color;
extern uint16_t myPalette[];

// Funzione helper per invertire i byte
#define SWAP16(x) ((x >> 8) | (x << 8))

extern void display_begin() {
  gfx.init();
  bg_color = gfx.color565(24, 28, 24); // DARK DARK GREY
  gfx.startWrite();
  gfx.fillScreen(bg_color);
  gfx.endWrite();

  // Attiva retroilluminazione
  //ledcSetup(1, 12000, 8);       // 12 kHz, 8-bit
  ledcAttach(PIN_BL, 12000, 8);     // assegna il pin BL al canale 1
  ledcWrite(1, TFT_BRIGHTNESS); // luminosità 0 - 255
}

extern "C" void display_init() {
  for (int x = 0; x < DISPLAY_WIDTH; x++) {
    scale_x[x] = (x * NES_SCREEN_WIDTH) / DISPLAY_WIDTH;
  }
  for (int y = 0; y < DISPLAY_HEIGHT; y++) {
    scale_y[y] = (y * NES_SCREEN_HEIGHT) / DISPLAY_HEIGHT;
  }
}

extern "C" void display_write_frame(const uint8_t *data[]) {
  static uint16_t buffer[DISPLAY_WIDTH];

  gfx.startWrite();
  for (int y = 0; y < DISPLAY_HEIGHT; y++) {
    const uint8_t* src = data[scale_y[y]];

    for (int x = 0; x < DISPLAY_WIDTH; x++) {
      uint8_t pixel = src[scale_x[x]];
      buffer[x] = SWAP16(myPalette[pixel]);
    }

    gfx.pushImage(0, y, DISPLAY_WIDTH, 1, buffer);
  }
  gfx.endWrite();
}  

extern "C" void display_clear() {
  gfx.startWrite();
  gfx.fillRect(0, 0, gfx.width(), gfx.height(), bg_color);
  gfx.endWrite();
}
