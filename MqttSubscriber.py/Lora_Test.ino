#include <SPI.h> 
#include <LoRa.h> 

#define LORA_CS_PIN     53  // Chip Select (NSS) - Pin SS hardware del Mega
#define LORA_RESET_PIN  9   // Reset (RST)
#define LORA_IRQ_PIN    2   // Interrupt (DIO0)

void setup() {
  Serial.begin(9600);
  while (!Serial);
  Serial.println("DigitalBin - LoRa Sender");

  LoRa.setPins(LORA_CS_PIN, LORA_RESET_PIN, LORA_IRQ_PIN);

  if (!LoRa.begin(433E6)) { 
    Serial.println("LoRa init failed!");
    while (1); 
  }
  
  Serial.println("LoRa init success!");
}

void loop() {
  Serial.print("Sending data: ");

  LoRa.beginPacket();
  LoRa.print("testing string");
  LoRa.endPacket();
  delay(20000); 
}
