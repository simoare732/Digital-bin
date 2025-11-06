#include <SPI.h>  
#include <LoRa.h> 

#define LORA_CS_PIN     10  // Chip Select (NSS)
#define LORA_RESET_PIN  9   // Reset (RST)
#define LORA_IRQ_PIN    2   // Interrupt (DIO0)

String tx_msg = ""; 

void setup() {
  // Start Serial for debugging
  Serial.begin(9600);
  while (!Serial); 
  
  LoRa.setPins(LORA_CS_PIN, LORA_RESET_PIN, LORA_IRQ_PIN);

  if (!LoRa.begin(433E6)) { 
    while (1); 
  }
}

void loop() {
    
   LoRa.receive();
   
  int packetSize = LoRa.parsePacket();
  
  if (packetSize) {
    String receivedData = "";
    while (LoRa.available()) {
      receivedData += (char)LoRa.read();
    }

    if (receivedData == "ping") {
      
      if (tx_msg.length() > 0) {

        LoRa.beginPacket();
        LoRa.print(tx_msg);
        LoRa.endPacket();
        
      }

      tx_msg = ""; 
    }
  } 

  if (Serial.available() > 0) {
    String newMessage = Serial.readStringUntil('\n');
    newMessage.trim();

    if (newMessage.length() > 0) {
      tx_msg = newMessage;
    }
  }
}
