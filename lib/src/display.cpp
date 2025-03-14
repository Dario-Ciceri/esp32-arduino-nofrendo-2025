extern "C" {
  #include <nes/nes.h>
}

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// Definizione pin
#define PIN_WR          35
#define PIN_RD          48
#define PIN_RS          36
#define PIN_CS          37
#define PIN_RST         -1
#define PIN_BUSY        -1
#define PIN_BL          45

// Macro per swap dei byte per i colori (necessaria per i colori corretti)
#define SWAP16(x) ((x >> 8) | (x << 8))

// Configurazione
#define PWM_CHANNEL     7
#define FREQ_WRITE      60000000  // Aumentato per migliori performance
#define FREQ_PWM        44100
#define TFT_BRIGHTNESS  255

// Dimensioni schermo NES
#define NES_SCREEN_WIDTH  256
#define NES_SCREEN_HEIGHT 240

// Dimensioni display
#define DISPLAY_WIDTH   480
#define DISPLAY_HEIGHT  320

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9488 _panel_instance;
  lgfx::Bus_Parallel16 _bus_instance;
  lgfx::Light_PWM _light_instance;

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
      _panel_instance.setBus(&_bus_instance);
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
      _panel_instance.setLight(&_light_instance);
    }
    
    setPanel(&_panel_instance);
  }
};

// Istanza display
LGFX gfx;

// Scaling arrays
static uint16_t scale_x[DISPLAY_WIDTH];
static uint8_t scale_y[DISPLAY_HEIGHT];

extern int16_t bg_color;
extern uint16_t myPalette[];

extern void display_begin() {
  Serial.println("Initializing display...");
  
  // Inizializza display
  gfx.init();
  
  // Configura SPI/bus ottimizzati
  gfx.initDMA();  // Inizializza DMA se supportata
  
  // Esegui un test del display (più breve)
  gfx.fillScreen(TFT_RED);
  delay(200);
  gfx.fillScreen(TFT_GREEN);
  delay(200);
  gfx.fillScreen(TFT_BLUE);
  delay(200);
  
  // Imposta colore di sfondo
  bg_color = gfx.color565(24, 28, 24); // DARK DARK GREY
  gfx.fillScreen(bg_color);
  
  // Scrivi un messaggio di debug
  gfx.setTextColor(TFT_WHITE, bg_color);
  gfx.setTextSize(2);
  gfx.setCursor(120, 160);
  gfx.println("Display ready!");
  
  // Abilita retroilluminazione al massimo
  ledcSetup(1, 12000, 8);
  ledcAttachPin(PIN_BL, 1);
  ledcWrite(1, TFT_BRIGHTNESS);
  
  Serial.println("Display initialized!");
  
}

extern "C" void display_init() {
  // Precalcola scaling
  for (int x = 0; x < DISPLAY_WIDTH; x++) {
    scale_x[x] = (x * NES_SCREEN_WIDTH) / DISPLAY_WIDTH;
  }
  
  for (int y = 0; y < DISPLAY_HEIGHT; y++) {
    scale_y[y] = (y * NES_SCREEN_HEIGHT) / DISPLAY_HEIGHT;
  }
  
  Serial.println("Display scaling initialized");
}

extern "C" void display_write_frame(const uint8_t *data[]) {
  // Verifica che i dati esistano
  if (!data) return;
  
  // Prepara un buffer di linea per una velocità ottimale
  static uint16_t line_buffer[DISPLAY_WIDTH];
  
  // Disegna direttamente sullo schermo
  gfx.startWrite();
  
  // Imposta la finestra di scrittura una volta sola per l'intero frame
  gfx.setAddrWindow(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
  
  for (int y = 0; y < DISPLAY_HEIGHT; y++) {
    // Prendi la linea sorgente usando lo scaling
    const uint8_t* src = data[scale_y[y]];
    
    // Prerendi i pixel della linea nel buffer
    for (int x = 0; x < DISPLAY_WIDTH; x++) {
      // Applica SWAP16 per invertire i byte del colore come nel codice originale
      line_buffer[x] = SWAP16(myPalette[src[scale_x[x]]]);
    }
    
    // Invia l'intera linea in un'unica operazione (molto più veloce)
    gfx.pushPixels(line_buffer, DISPLAY_WIDTH);
  }
  
  
  
  gfx.endWrite();
}

extern "C" void display_clear() {
  gfx.fillScreen(bg_color);
}