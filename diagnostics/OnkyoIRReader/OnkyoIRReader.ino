#include <IRremote.hpp>

#define IR_RECEIVE_PIN 27

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Onkyo Remote - odbiornik IR gotowy.");
  Serial.println("Nacisnij przycisk na pilocie...");

  IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);
}

void loop() {
  if (IrReceiver.decode()) {
    IrReceiver.printIRResultShort(&Serial);
    IrReceiver.resume();
  }
}