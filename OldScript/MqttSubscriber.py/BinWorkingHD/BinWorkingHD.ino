#include <SPI.h>  
#include <LoRa.h> 

#define LORA_CS_PIN     53  // Chip Select (NSS)
#define LORA_RESET_PIN  9   // Reset (RST)
#define LORA_IRQ_PIN    2   // Interrupt (DIO0)

const byte START_SEQUENCE = 0x01; 
const byte MY_ADDRESS = 0xAB;     

const long interval = 2000; 
unsigned long previousMillis = 0;

byte rx_buffer[32];

void setup() {
  Serial.begin(9600);
  while (!Serial); 
  
  LoRa.setPins(LORA_CS_PIN, LORA_RESET_PIN, LORA_IRQ_PIN);

  if (!LoRa.begin(433E6)) { 
    Serial.println("error");
    while (1); 
  }
  Serial.println("LoRa init ok!");
  
  LoRa.receive();
}

uint16_t calculateCRC16(byte *data, int length) {
  uint16_t crc = 0xFFFF; 
  
  for (int i = 0; i < length; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (int j = 0; j < 8; j++) {
      if (crc & 0x8000) {
        crc = (crc << 1) ^ 0x1021;
      } else {
        crc = crc << 1;
      }
    }
  }
  return crc;
}

void loop() {
  int packetSize = LoRa.parsePacket();
  
  if (packetSize) {
    //handling msg from bridge
    byte startSQ = (byte)LoRa.read();
    byte addr = (byte)LoRa.read();
    if(startSQ==START_SEQUENCE and addr==MY_ADDRESS){
        int len = 0;
        while (LoRa.available()) {
          if (len < sizeof(rx_buffer)) {
            rx_buffer[len] = (byte)LoRa.read();
            Serial.print((char)rx_buffer[len]);
            len++;
          } else {
            LoRa.read(); 
          }
        }
        Serial.println("");
  
        uint16_t receivedCRC = calculateCRC16(rx_buffer, len);
        
        Serial.print("Pacchetto ricevuto, len=");
        Serial.print(len);
        Serial.print(", CRC calcolato: ");
        Serial.println(receivedCRC, HEX);

        LoRa.beginPacket();
        LoRa.write(START_SEQUENCE);
        
        LoRa.write((byte)(receivedCRC & 0xFF)); 
        LoRa.write((byte)(receivedCRC >> 8));  
        
        LoRa.endPacket();
    }
    
  } else {
    //sending data to bridge
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;
      
      Serial.println("sending packet");
      
      LoRa.beginPacket();
      LoRa.write(START_SEQUENCE);
      LoRa.print("HellLora\n");
      LoRa.endPacket();
    }
  }
}