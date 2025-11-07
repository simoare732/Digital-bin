#include <SPI.h>  
#include <LoRa.h> 

#define LORA_CS_PIN     10  // Chip Select (NSS)
#define LORA_RESET_PIN  9   // Reset (RST)
#define LORA_IRQ_PIN    2   // Interrupt (DIO0)

const byte RX = 0xAA; // Code of the LoRa Receiver

String tx_msg = ""; 

void setup() {
  // Start Serial for debugging
  Serial.begin(9600);
  while (!Serial); 
  
  LoRa.setPins(LORA_CS_PIN, LORA_RESET_PIN, LORA_IRQ_PIN);

  if (!LoRa.begin(433E6)) { 
    while (1); 
  }

  //LoRa.receive();
}

void loop() {

  //Ricezione
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    byte recipient = LoRa.read();

    String incoming = "";
    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }

    if(recipient == RX){
      Serial.println(incoming);
    }
  }

  // Messages from Bridge
  if (Serial.available() > 0) {
    LoRa.beginPacket();
    LoRa.write(0x01);
    
    while (Serial.available()) {
      LoRa.write(Serial.read()); // Scrive i byte binari
    }
    LoRa.endPacket();
    
    // TORNA SUBITO IN ASCOLTO SU LORA
    LoRa.receive(); 
  }  
  int packetSize = LoRa.parsePacket();
  if (packetSize > 0) {
    while (LoRa.available()) {
      Serial.write(LoRa.read()); // Scrive i dati (inclusi \n)
    }
    // (Non serve tornare in LoRa.receive() perché 
    // l'abbiamo già fatto dopo l'invio al punto 1)
  } 
}
