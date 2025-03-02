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

// Configurazione
#define PWM_CHANNEL     7
#define FREQ_WRITE      40000000  // 40MHz per ILI9488
#define FREQ_PWM        44100
#define TFT_BRIGHTNESS  255

// Dimensioni schermo NES
#define NES_SCREEN_WIDTH  256
#define NES_SCREEN_HEIGHT 240

// Dimensioni display
#define DISPLAY_WIDTH   480
#define DISPLAY_HEIGHT  320

class LGFX : public lgfx::LGFX_Device {
  lgfx::Bus_Parallel16 _bus_instance;
  lgfx::Panel_ILI9488 _panel_instance;
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

// Istanza display
LGFX gfx;

// Buffer di frame ottimizzato per DMA
typedef struct {
  uint16_t* buffer;
  bool ready;
} frame_buffer_t;

static frame_buffer_t fb[2];
static int current_fb = 0;
static int display_fb = 1;

// Semafori per sincronizzazione
static SemaphoreHandle_t frame_semaphore = nullptr;

// Arrays di lookup per scaling
static uint16_t* scale_lookup_x = nullptr;
static uint8_t* scale_lookup_y = nullptr;

// Statistiche FPS
static uint32_t fps_count = 0;
static uint32_t fps_timer = 0;
static uint8_t current_fps = 0;
static bool show_fps = true;

extern int16_t bg_color;
extern uint16_t myPalette[];

// Cache palette convertita
static uint16_t converted_palette[256];
static bool palette_cached = false;

// Task rendering 
void render_task(void* parameter) {
  char fps_text[16];
  
  while (true) {
    // Attendi che ci sia un frame da renderizzare
    if (xSemaphoreTake(frame_semaphore, portMAX_DELAY) == pdTRUE) {
      // Se il buffer è pronto, invialo al display
      if (fb[display_fb].ready) {
        // Trasferisci al display tramite DMA
        gfx.pushImage(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, fb[display_fb].buffer);
        
        // Aggiorna FPS counter
        fps_count++;
        uint32_t now = millis();
        if (now - fps_timer >= 1000) {
          current_fps = fps_count;
          fps_count = 0;
          fps_timer = now;
          
          // Mostra FPS se richiesto
          if (show_fps) {
            sprintf(fps_text, "FPS:%d", current_fps);
            
            gfx.setTextColor(TFT_YELLOW, TFT_BLACK);
            gfx.setCursor(DISPLAY_WIDTH - 65, 10);
            gfx.print(fps_text);
          }
        }
        
        fb[display_fb].ready = false;
      }
      
      // Scambia i buffer
      int tmp = display_fb;
      display_fb = current_fb;
      current_fb = tmp;
    }
  }
}

extern void display_begin() {
  // Inizializza il display
  gfx.init();
  bg_color = gfx.color565(24, 28, 24); // DARK DARK GREY
  
  // Schermata iniziale
  gfx.fillScreen(bg_color);
  
  // Attiva retroilluminazione
  ledcSetup(1, 12000, 8);
  ledcAttachPin(PIN_BL, 1);
  ledcWrite(1, TFT_BRIGHTNESS);
  
  // Inizializza lookup tables per scaling veloce
  if (psramFound()) {
    scale_lookup_x = (uint16_t*)ps_malloc(DISPLAY_WIDTH * sizeof(uint16_t));
    scale_lookup_y = (uint8_t*)ps_malloc(DISPLAY_HEIGHT * sizeof(uint8_t));
    
    // Alloca buffer allineati per DMA
    fb[0].buffer = (uint16_t*)heap_caps_malloc(DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    fb[1].buffer = (uint16_t*)heap_caps_malloc(DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
  } else {
    // Fallback a memoria normale
    scale_lookup_x = (uint16_t*)malloc(DISPLAY_WIDTH * sizeof(uint16_t));
    scale_lookup_y = (uint8_t*)malloc(DISPLAY_HEIGHT * sizeof(uint8_t));
    
    fb[0].buffer = (uint16_t*)malloc(DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t));
    fb[1].buffer = (uint16_t*)malloc(DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t));
  }
  
  // Precalcola i fattori di scaling
  for (int x = 0; x < DISPLAY_WIDTH; x++) {
    scale_lookup_x[x] = (x * NES_SCREEN_WIDTH) / DISPLAY_WIDTH;
  }
  
  for (int y = 0; y < DISPLAY_HEIGHT; y++) {
    scale_lookup_y[y] = (y * NES_SCREEN_HEIGHT) / DISPLAY_HEIGHT;
  }
  
  // Pulisci i buffer
  for (int i = 0; i < 2; i++) {
    if (fb[i].buffer) {
      for (int j = 0; j < DISPLAY_WIDTH * DISPLAY_HEIGHT; j++) {
        fb[i].buffer[j] = bg_color;
      }
    }
    fb[i].ready = false;
  }
  
  // Crea semafori
  frame_semaphore = xSemaphoreCreateBinary();
  
  // Crea task di rendering su core 0
  xTaskCreatePinnedToCore(
    render_task,
    "RenderTask",
    4096,
    NULL,
    1,
    NULL,
    0  // Core 0
  );
  
  fps_timer = millis();
}

extern "C" void display_init() {
  // Precalcola palette per accelerare il rendering
  if (!palette_cached) {
    for (int i = 0; i < 256; i++) {
      converted_palette[i] = myPalette[i];
    }
    palette_cached = true;
  }
}

extern "C" void display_write_frame(const uint8_t *data[]) {
  // Verifica che i buffer siano inizializzati
  if (!fb[current_fb].buffer) return;
  
  uint16_t* dst = fb[current_fb].buffer;
  
  // Scaling ottimizzato con unrolling del ciclo
  for (int y = 0; y < DISPLAY_HEIGHT; y++) {
    const uint8_t* src = data[scale_lookup_y[y]];
    uint16_t* row = &dst[y * DISPLAY_WIDTH];
    
    // Processa 4 pixel alla volta
    int x = 0;
    for (; x <= DISPLAY_WIDTH - 4; x += 4) {
      row[x]   = converted_palette[src[scale_lookup_x[x]]];
      row[x+1] = converted_palette[src[scale_lookup_x[x+1]]];
      row[x+2] = converted_palette[src[scale_lookup_x[x+2]]];
      row[x+3] = converted_palette[src[scale_lookup_x[x+3]]];
    }
    
    // Processa i pixel rimanenti
    for (; x < DISPLAY_WIDTH; x++) {
      row[x] = converted_palette[src[scale_lookup_x[x]]];
    }
  }
  
  // Segna il buffer come pronto
  fb[current_fb].ready = true;
  
  // Notifica il task di rendering
  xSemaphoreGive(frame_semaphore);
}

extern "C" void display_toggle_fps() {
  show_fps = !show_fps;
  
  // Se disabilitato, pulisci l'area FPS
  if (!show_fps) {
    gfx.fillRect(DISPLAY_WIDTH - 70, 0, 70, 20, bg_color);
  }
}

extern "C" void display_clear() {
  if (!fb[0].buffer || !fb[1].buffer) return;
  
  // Pulisci entrambi i buffer
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < DISPLAY_WIDTH * DISPLAY_HEIGHT; j++) {
      fb[i].buffer[j] = bg_color;
    }
    fb[i].ready = false;
  }
  
  // Pulisci lo schermo
  gfx.fillScreen(bg_color);
}