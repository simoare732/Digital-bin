
#include <SPI.h>  
#include <LoRa.h> 

#define LORA_CS_PIN     10 
#define LORA_RESET_PIN  9 
#define LORA_IRQ_PIN    2   

void setup() {
  Serial.begin(9600);
  while (!Serial); 
  
  LoRa.setPins(LORA_CS_PIN, LORA_RESET_PIN, LORA_IRQ_PIN);
  
  if (!LoRa.begin(433E6)) { 
    while (1);
  }
}

void loop() {
    int packetSize = LoRa.parsePacket();
  
    if (packetSize) {
    String receivedData = "";
    while (LoRa.available()) {
      receivedData += (char)LoRa.read();
    }

    Serial.print(receivedData);
    Serial.println("");
  }
}
