#include <IRremote.hpp>

#define IR_SEND_PIN 26
#define IR_RECEIVE_PIN 27

unsigned long lastSend = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);
  IrSender.begin(IR_SEND_PIN);

  Serial.println("=== TEST IR ===");
  Serial.println("Nadaje POWER co 2 sekundy.");
  Serial.println("Odbiornik nasluchuje na GPIO27.");
}

void loop() {

  // Co 2 sekundy nadaj POWER
  if (millis() - lastSend >= 2000) {
    lastSend = millis();

    Serial.println();
    Serial.println(">>> WYSYLAM POWER");

    IrSender.sendNEC(0x6DD2, 0x04, 0);
  }

  // Sprawdzamy, co odebral odbiornik
  if (IrReceiver.decode()) {

    Serial.print("<<< ODEBRANO: ");
    IrReceiver.printIRResultShort(&Serial);

    IrReceiver.resume();
  }
}