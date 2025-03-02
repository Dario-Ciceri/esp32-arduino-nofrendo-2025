/* Arduino Nofrendo
 * Please check hw_config.h and display.cpp for configuration details
 */
#include <esp_wifi.h>
#include <esp_task_wdt.h>
#include <FFat.h>
#include <SPIFFS.h>
#include <SD.h>
#include <SD_MMC.h>

#include <Arduino_GFX_Library.h>

#include "hw_config.h"
#include "controller.cpp"

extern "C"
{
#include <nofrendo.h>
}

int16_t bg_color;
extern Arduino_TFT *gfx;
extern void display_begin();
extern uint32_t controller_read_input();

TaskHandle_t controllerTaskHandle;

volatile uint32_t nesInputState = 0xFFFFFFFF; // Stato "tutto rilasciato"

// Nel task del gamepad (core 1):
void controllerTask(void *parameter) {
  while (true) {
    xboxController.onLoop();
    controller_read_input();
    // nesInputState = controller_read_input();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void emuTask(void *parameter) {
    Serial.println("Starting NoFrendo Emulator...");
    char *argv[1];
    argv[0] = (char *)parameter;
    nofrendo_main(1, argv);
    Serial.println("NoFrendo ended!");
    vTaskDelete(NULL); // Termina il task dopo l'esecuzione dell'emulatore
}

void setup() {
    Serial.begin(115200);

    if (psramInit()) {
        Serial.println("PSRAM initialized successfully");
    } else {
        Serial.println("PSRAM initialization failed");
    }

    // turn off WiFi
    esp_wifi_deinit();

    // disable Core 0 WDT
    TaskHandle_t idle_0 = xTaskGetIdleTaskHandleForCPU(0);
    esp_task_wdt_delete(idle_0);

    // start display
    display_begin();

    // filesystem defined in hw_config.h
    FILESYSTEM_BEGIN

    // find first rom file (*.nes)
    File root = filesystem.open("/");
    char *romPath = "/";
    if (root) {
        File file = root.openNextFile();
        while (file) {
            if (!file.isDirectory()) {
                char *filename = (char *)file.name();
                int8_t len = strlen(filename);
                if (strstr(strlwr(filename + (len - 4)), ".nes")) {
                    static char fullFilename[256];
                    sprintf(fullFilename, "%s/%s", FSROOT, filename);
                    Serial.println(fullFilename);
                    romPath = fullFilename;
                    break;
                }
            }
            file = root.openNextFile();
        }
    } else {
        Serial.println("Filesystem mount failed! Please check hw_config.h settings.");
        gfx->println("Filesystem mount failed! Please check hw_config.h settings.");
    }

    // Avvia il task del controller sul core 0
    xTaskCreatePinnedToCore(
        controllerTask,
        "ControllerTask",
        8192,
        NULL,
        2,
        NULL,
        0 // Core 0
    );

    // Avvia il task dell'emulatore sul core 1
    xTaskCreatePinnedToCore(
        emuTask,
        "EmulatorTask",
        16384, // Maggiore stack per l'emulatore
        (void *)romPath,
        3, // Priorità più alta per l'emulatore
        NULL,
        1 // Core 1
    );
}

void loop() {
    vTaskDelay(portMAX_DELAY);
}

