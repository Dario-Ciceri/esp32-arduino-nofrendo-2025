#include <Arduino.h>
extern "C" {
  #include "SerialWrapper.h"
}

void my_log(const char *msg) {
  Serial.println(msg);
}