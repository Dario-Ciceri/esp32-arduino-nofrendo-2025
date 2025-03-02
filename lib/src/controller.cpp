#include <Arduino.h>

#include "hw_config.h"

/* controller is GPIO */
#if defined(HW_CONTROLLER_GPIO)

// Structure to store button state
typedef struct {
  uint32_t current;   // Current button state
  uint32_t previous;  // Previous button state
  uint32_t pressed;   // Buttons that were just pressed
  uint32_t released;  // Buttons that were just released
} ButtonState;

static ButtonState buttonState = {0xFFFFFFFF, 0xFFFFFFFF, 0, 0};

// Button bit definitions
#define BTN_UP      (1 << 0)
#define BTN_DOWN    (1 << 1)
#define BTN_LEFT    (1 << 2)
#define BTN_RIGHT   (1 << 3)
#define BTN_SELECT  (1 << 4)
#define BTN_START   (1 << 5)
#define BTN_A       (1 << 6)
#define BTN_B       (1 << 7)
#define BTN_X       (1 << 8)
#define BTN_Y       (1 << 9)

// Analog joystick settings
#define JOYSTICK_DEADZONE  300    // Deadzone to prevent drift
#define JOYSTICK_CENTER    2048   // Center position (for 12-bit ADC)
#define JOYSTICK_MAX       4095   // Maximum value

extern "C" void controller_init()
{
#if defined(HW_CONTROLLER_GPIO_ANALOG_JOYSTICK)
  // Initialize analog pins
  pinMode(HW_CONTROLLER_GPIO_UP_DOWN, INPUT);
  pinMode(HW_CONTROLLER_GPIO_LEFT_RIGHT, INPUT);
  
  // Set ADC resolution to 12 bits if supported
  #if defined(ESP32)
  analogReadResolution(12);
  #endif

#else  /* !defined(HW_CONTROLLER_GPIO_ANALOG_JOYSTICK) */
  // Initialize digital pins with pull-up resistors
  pinMode(HW_CONTROLLER_GPIO_UP, INPUT_PULLUP);
  pinMode(HW_CONTROLLER_GPIO_DOWN, INPUT_PULLUP);
  pinMode(HW_CONTROLLER_GPIO_LEFT, INPUT_PULLUP);
  pinMode(HW_CONTROLLER_GPIO_RIGHT, INPUT_PULLUP);
#endif /* !defined(HW_CONTROLLER_GPIO_ANALOG_JOYSTICK) */

  // Initialize common button pins
  pinMode(HW_CONTROLLER_GPIO_SELECT, INPUT_PULLUP);
  pinMode(HW_CONTROLLER_GPIO_START, INPUT_PULLUP);
  pinMode(HW_CONTROLLER_GPIO_A, INPUT_PULLUP);
  pinMode(HW_CONTROLLER_GPIO_B, INPUT_PULLUP);
  pinMode(HW_CONTROLLER_GPIO_X, INPUT_PULLUP);
  pinMode(HW_CONTROLLER_GPIO_Y, INPUT_PULLUP);
  
  // Initialize button state
  buttonState.current = 0xFFFFFFFF;
  buttonState.previous = 0xFFFFFFFF;
  buttonState.pressed = 0;
  buttonState.released = 0;
}

extern "C" uint32_t controller_read_input()
{
  // Save previous state
  buttonState.previous = buttonState.current;
  
  // Start with all buttons released (1 = not pressed in GPIO input)
  uint32_t u = 1, d = 1, l = 1, r = 1;
  uint32_t s, t, a, b, x, y;

#if defined(HW_CONTROLLER_GPIO_ANALOG_JOYSTICK)
  // Read analog values
  #if defined(HW_CONTROLLER_GPIO_REVERSE_UD)
  int joyY = JOYSTICK_MAX - analogRead(HW_CONTROLLER_GPIO_UP_DOWN);
  #else /* !defined(HW_CONTROLLER_GPIO_REVERSE_UD) */
  int joyY = analogRead(HW_CONTROLLER_GPIO_UP_DOWN);
  #endif /* !defined(HW_CONTROLLER_GPIO_REVERSE_UD) */

  #if defined(HW_CONTROLLER_GPIO_REVERSE_LF)
  int joyX = JOYSTICK_MAX - analogRead(HW_CONTROLLER_GPIO_LEFT_RIGHT);
  #else /* !defined(HW_CONTROLLER_GPIO_REVERSE_LF) */
  int joyX = analogRead(HW_CONTROLLER_GPIO_LEFT_RIGHT);
  #endif /* !defined(HW_CONTROLLER_GPIO_REVERSE_LF) */

  // Optional: print joystick values for debugging
  // Serial.printf("joyX: %d, joyY: %d\n", joyX, joyY);

#if defined(ARDUINO_ODROID_ESP32)
  // ODROID-specific joystick mapping
  if (joyY > 2048 + 1024)
  {
    u = 0;  // Up pressed
    d = 1;  // Down not pressed
  }
  else if (joyY > 1024)
  {
    u = 1;  // Up not pressed
    d = 0;  // Down pressed
  }
  else
  {
    // Allow for diagonal input (both up and down can be 0 simultaneously)
    // This will be important for games that rely on diagonal movements
    if (joyY < 512) {
      u = 0;  // Up pressed
      d = 0;  // Down pressed
    } else {
      u = 1;  // Up not pressed
      d = 1;  // Down not pressed
    }
  }
  
  if (joyX > 2048 + 1024)
  {
    l = 0;  // Left pressed
    r = 1;  // Right not pressed
  }
  else if (joyX > 1024)
  {
    l = 1;  // Left not pressed
    r = 0;  // Right pressed
  }
  else
  {
    // Allow for diagonal input (both left and right can be 0 simultaneously)
    if (joyX < 512) {
      l = 0;  // Left pressed
      r = 0;  // Right pressed
    } else {
      l = 1;  // Left not pressed
      r = 1;  // Right not pressed
    }
  }

#else /* !defined(ARDUINO_ODROID_ESP32) */
  // Standard ESP32 joystick mapping
  // Allow more flexible control by checking specific thresholds
  
  // Y-axis (Up/Down)
  if (joyY > JOYSTICK_CENTER + JOYSTICK_DEADZONE) {
    d = 0;  // Down pressed
  } else {
    d = 1;  // Down not pressed
  }
  
  if (joyY < JOYSTICK_CENTER - JOYSTICK_DEADZONE) {
    u = 0;  // Up pressed
  } else {
    u = 1;  // Up not pressed
  }
  
  // X-axis (Left/Right)
  if (joyX > JOYSTICK_CENTER + JOYSTICK_DEADZONE) {
    r = 0;  // Right pressed
  } else {
    r = 1;  // Right not pressed
  }
  
  if (joyX < JOYSTICK_CENTER - JOYSTICK_DEADZONE) {
    l = 0;  // Left pressed
  } else {
    l = 1;  // Left not pressed
  }

#endif /* !defined(ARDUINO_ODROID_ESP32) */

  // Check if we're also using digital d-pad inputs in addition to analog
  #if defined(HW_CONTROLLER_GPIO_USE_DPAD_WITH_ANALOG)
  // Read digital D-pad if configured
  if (digitalRead(HW_CONTROLLER_GPIO_UP) == LOW) u = 0;     // Up
  if (digitalRead(HW_CONTROLLER_GPIO_DOWN) == LOW) d = 0;   // Down
  if (digitalRead(HW_CONTROLLER_GPIO_LEFT) == LOW) l = 0;   // Left
  if (digitalRead(HW_CONTROLLER_GPIO_RIGHT) == LOW) r = 0;  // Right
  #endif

#else  /* !defined(HW_CONTROLLER_GPIO_ANALOG_JOYSTICK) */
  // Read digital inputs
  u = digitalRead(HW_CONTROLLER_GPIO_UP);
  d = digitalRead(HW_CONTROLLER_GPIO_DOWN);
  l = digitalRead(HW_CONTROLLER_GPIO_LEFT);
  r = digitalRead(HW_CONTROLLER_GPIO_RIGHT);
#endif /* !defined(HW_CONTROLLER_GPIO_ANALOG_JOYSTICK) */

  // Read other buttons
  s = digitalRead(HW_CONTROLLER_GPIO_SELECT);
  t = digitalRead(HW_CONTROLLER_GPIO_START);
  a = digitalRead(HW_CONTROLLER_GPIO_A);
  b = digitalRead(HW_CONTROLLER_GPIO_B);
  x = digitalRead(HW_CONTROLLER_GPIO_X);
  y = digitalRead(HW_CONTROLLER_GPIO_Y);

  // Build the button state (0=pressed, 1=not pressed)
  buttonState.current = 0xFFFFFFFF ^ (
    (!u << 0) | (!d << 1) | (!l << 2) | (!r << 3) | 
    (!s << 4) | (!t << 5) | (!a << 6) | (!b << 7) | 
    (!x << 8) | (!y << 9)
  );
  
  // Calculate buttons that were just pressed or released
  buttonState.pressed = (~buttonState.current) & buttonState.previous;
  buttonState.released = buttonState.current & (~buttonState.previous);
  
  return buttonState.current;
}

// New function to check if a specific button was just pressed
extern "C" bool controller_button_pressed(uint32_t button_mask)
{
  return (buttonState.pressed & button_mask) != 0;
}

// New function to check if a specific button was just released
extern "C" bool controller_button_released(uint32_t button_mask)
{
  return (buttonState.released & button_mask) != 0;
}

// New function to check if a specific button is currently held down
extern "C" bool controller_button_down(uint32_t button_mask)
{
  return (buttonState.current & button_mask) == 0;
}

/* controller is I2C M5Stack CardKB */
#elif defined(HW_CONTROLLER_I2C_M5CARDKB)

#include <Wire.h>

#define I2C_M5CARDKB_ADDR 0x5f
#define READ_BIT I2C_MASTER_READ /*!< I2C master read */
#define ACK_CHECK_EN 0x1         /*!< I2C master will check ack from slave */
#define NACK_VAL 0x1             /*!< I2C nack value */

static uint32_t lastButtonState = 0xFFFFFFFF;
static uint32_t currentButtonState = 0xFFFFFFFF;

extern "C" void controller_init()
{
  Wire.begin();
}

extern "C" uint32_t controller_read_input()
{
  // Save previous state
  lastButtonState = currentButtonState;
  
  // Start with all buttons released
  uint32_t value = 0xFFFFFFFF;

  Wire.requestFrom(I2C_M5CARDKB_ADDR, 1);
  while (Wire.available())
  {
    char c = Wire.read(); // receive a byte as character
    if (c != 0)
    {
      switch (c)
      {
      case 181: // up
        value ^= (1 << 0);
        break;
      case 182: // down
        value ^= (1 << 1);
        break;
      case 180: // left
        value ^= (1 << 2);
        break;
      case 183: // right
        value ^= (1 << 3);
        break;
      case ' ': // select
        value ^= (1 << 4);
        break;
      case 13: // enter -> start
        value ^= (1 << 5);
        break;
      case 'k': // A
        value ^= (1 << 6);
        break;
      case 'l': // B
        value ^= (1 << 7);
        break;
      case 'o': // X
        value ^= (1 << 8);
        break;
      case 'p': // Y
        value ^= (1 << 9);
        break;
      // Add more mappings as needed
      case 'w': // also map to up
        value ^= (1 << 0);
        break;
      case 's': // also map to down
        value ^= (1 << 1);
        break;
      case 'a': // also map to left
        value ^= (1 << 2);
        break;
      case 'd': // also map to right
        value ^= (1 << 3);
        break;
      }
    }
  }

  currentButtonState = value;
  return value;
}

/* controller is I2C BBQ10Keyboard */
#elif defined(HW_CONTROLLER_I2C_BBQ10KB)

#include <Wire.h>
#include <BBQ10Keyboard.h>
BBQ10Keyboard keyboard;
static uint32_t value = 0xFFFFFFFF;

extern "C" void controller_init()
{
  Wire.begin();
  keyboard.begin();
  keyboard.setBacklight(0.2f);
}

extern "C" uint32_t controller_read_input()
{
  int keyCount = keyboard.keyCount();
  while (keyCount--)
  {
    const BBQ10Keyboard::KeyEvent key = keyboard.keyEvent();
    String state = "pressed";
    if (key.state == BBQ10Keyboard::StateLongPress)
      state = "held down";
    else if (key.state == BBQ10Keyboard::StateRelease)
      state = "released";

    // Serial.printf("key: '%c' (dec %d, hex %02x) %s\r\n", key.key, key.key, key.key, state.c_str());

    uint32_t bit = 0;
    if (key.key != 0)
    {
      switch (key.key)
      {
      case 'w': // up
        bit = (1 << 0);
        break;
      case 'z': // down
        bit = (1 << 1);
        break;
      case 'a': // left
        bit = (1 << 2);
        break;
      case 'd': // right
        bit = (1 << 3);
        break;
      case ' ': // select
        bit = (1 << 4);
        break;
      case 10: // enter -> start
        bit = (1 << 5);
        break;
      case 'k': // A
        bit = (1 << 6);
        break;
      case 'l': // B
        bit = (1 << 7);
        break;
      case 'o': // X
        bit = (1 << 8);
        break;
      case 'p': // Y
        bit = (1 << 9);
        break;
      // Add additional key mappings
      case 's': // also map to down
        bit = (1 << 1);
        break;
      }
      if (key.state == BBQ10Keyboard::StatePress)
      {
        value ^= bit;
      }
      else if (key.state == BBQ10Keyboard::StateRelease)
      {
        value |= bit;
      }
    }
  }

  return value;
}

/* Xbox controller support */
#elif defined(XBOX_CONTROLLER)

#include <XboxSeriesXControllerESP32_asukiaaa.hpp>
XboxSeriesXControllerESP32_asukiaaa::Core xboxController;

// Structure to store Xbox controller button state
typedef struct {
  uint32_t current;   // Current button state
  uint32_t previous;  // Previous button state
  uint32_t pressed;   // Buttons that were just pressed
  uint32_t released;  // Buttons that were just released
} XboxButtonState;

static XboxButtonState xboxButtonState = {0xFFFFFFFF, 0xFFFFFFFF, 0, 0};

extern "C" void controller_init()
{
  // Serial.println("Starting NimBLE Client");
  xboxController.begin();
}

extern "C" uint32_t controller_read_input()
{
  // Save previous state
  xboxButtonState.previous = xboxButtonState.current;
  
  if (xboxController.isConnected())
  {
    if (xboxController.isWaitingForFirstNotification())
    {
      // Serial.println("waiting for first notification");
      return xboxButtonState.current; // Return last state while waiting
    }
    else
    {
      uint32_t bitmask = 0;

      // Map Xbox controller buttons to NES buttons
      if (xboxController.xboxNotif.btnDirUp)
        bitmask |= (1 << 0);  // Up
      if (xboxController.xboxNotif.btnDirDown)
        bitmask |= (1 << 1);  // Down
      if (xboxController.xboxNotif.btnDirLeft)
        bitmask |= (1 << 2);  // Left
      if (xboxController.xboxNotif.btnDirRight)
        bitmask |= (1 << 3);  // Right
      if (xboxController.xboxNotif.btnSelect)
        bitmask |= (1 << 4);  // Select
      if (xboxController.xboxNotif.btnStart)
        bitmask |= (1 << 5);  // Start
      if (xboxController.xboxNotif.btnA)
        bitmask |= (1 << 6);  // A
      if (xboxController.xboxNotif.btnB)
        bitmask |= (1 << 7);  // B
      if (xboxController.xboxNotif.btnX)
        bitmask |= (1 << 8);  // X
      if (xboxController.xboxNotif.btnY)
        bitmask |= (1 << 9);  // Y

      // Support for analog sticks as digital directional input
      #if defined(XBOX_CONTROLLER_USE_ANALOG_AS_DPAD)
      // Left analog stick
      int leftStickX = xboxController.xboxNotif.joyLHori;
      int leftStickY = xboxController.xboxNotif.joyLVert;
      
      // Right analog stick
      int rightStickX = xboxController.xboxNotif.joyRHori;
      int rightStickY = xboxController.xboxNotif.joyRVert;
      
      // Define thresholds for considering analog as button press
      const int ANALOG_THRESHOLD = 8000;  // Adjust as needed
      
      // Map left analog stick to D-pad
      if (leftStickY > ANALOG_THRESHOLD)
        bitmask |= (1 << 0);  // Up
      if (leftStickY < -ANALOG_THRESHOLD)
        bitmask |= (1 << 1);  // Down
      if (leftStickX < -ANALOG_THRESHOLD)
        bitmask |= (1 << 2);  // Left
      if (leftStickX > ANALOG_THRESHOLD)
        bitmask |= (1 << 3);  // Right
      #endif

      // Optional: Add triggers as additional buttons
      #if defined(XBOX_CONTROLLER_USE_TRIGGERS)
      if (xboxController.xboxNotif.trigLT > 200)  // Adjust threshold as needed
        bitmask |= (1 << 10);  // New button (if supported)
      if (xboxController.xboxNotif.trigRT > 200)  // Adjust threshold as needed
        bitmask |= (1 << 11);  // New button (if supported)
      #endif

      // Update button state
      xboxButtonState.current = 0xFFFFFFFF ^ bitmask;
      
      // Calculate buttons that were just pressed or released
      xboxButtonState.pressed = (~xboxButtonState.current) & xboxButtonState.previous;
      xboxButtonState.released = xboxButtonState.current & (~xboxButtonState.previous);

      // Serial.print("Command: ");
      // Serial.println(0xFFFFFFFF ^ bitmask);

      return xboxButtonState.current;
    }
  }
  else
  {
    // Serial.println("Controller not connected");
    if (xboxController.getCountFailedConnection() > 2)
    {
      // Serial.println("Restarting ESP...");
      ESP.restart();
    }
    return 0xFFFFFFFF;
  }
}

// Function to check if a specific Xbox button was just pressed
extern "C" bool xbox_button_pressed(uint32_t button_mask)
{
  return (xboxButtonState.pressed & button_mask) != 0;
}

// Function to check if a specific Xbox button was just released
extern "C" bool xbox_button_released(uint32_t button_mask)
{
  return (xboxButtonState.released & button_mask) != 0;
}

// Function to check if a specific Xbox button is currently held down
extern "C" bool xbox_button_down(uint32_t button_mask)
{
  return (xboxButtonState.current & button_mask) == 0;
}

#else /* no controller defined */

extern "C" void controller_init()
{
  // Serial.printf("GPIO controller disabled in menuconfig; no input enabled.\n");
}

extern "C" uint32_t controller_read_input()
{
  return 0xFFFFFFFF;
}

#endif /* no controller defined */