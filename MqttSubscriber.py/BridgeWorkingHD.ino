#include <SPI.h>  
#include <LoRa.h> 

#define LORA_CS_PIN     10
#define LORA_RESET_PIN  9 
#define LORA_IRQ_PIN    2 

const byte START_SEQUENCE = 0x01; 
const byte binAddress = 0xAB;

byte buffer_msg[32]; 
int pos = 0;

bool waitingForAck = false;
const long ackTimeout = 10000;
unsigned long lastSendTime = 0;
uint16_t lastSentCRC = 0;
uint8_t max_retry = 3;

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

void send_data() {
  lastSentCRC = calculateCRC16(buffer_msg, pos);
  
  LoRa.beginPacket();
  LoRa.write(START_SEQUENCE);
  LoRa.write(binAddress);
  
  int counter = 0;
  while (counter < pos) {
    LoRa.write(buffer_msg[counter]); 
    counter++;
  }
  LoRa.endPacket();

  LoRa.receive(); 

  waitingForAck = true;
  lastSendTime = millis();
}

void handle_lora_reception() {
  byte startHex = (byte)LoRa.read();
  
  if (startHex == START_SEQUENCE) { 
    if (waitingForAck && LoRa.available() == 2) {
      
      byte lowByte = LoRa.read();
      byte highByte = LoRa.read();
      uint16_t receivedCRC = (highByte << 8) | lowByte;

      if (receivedCRC == lastSentCRC) {
        Serial.println("ValidACK!");
        waitingForAck = false;
        pos=0;
      } else {
        Serial.print("INVALID ACK Expected: ");
        Serial.print(lastSentCRC, HEX);
        Serial.print(" RECEIVED: ");
        Serial.println(receivedCRC, HEX);
      }
      
    } else {
      String msg = "";
      while (LoRa.available()) {
        msg += (char)LoRa.read();
      }
      Serial.print(msg);
      Serial.println("");
    }
  } else {
    while(LoRa.available()) LoRa.read(); 
  }
}

void ackReceived() {
  waitingForAck = false;
  pos = 0; // Svuota il buffer, pronto per nuovi dati
}


void loop() {


  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    handle_lora_reception();
  }

  if (!waitingForAck && Serial.available() > 0) {
    delay(20);
    while (Serial.available()) {
      if (pos < sizeof(buffer_msg)) {
        buffer_msg[pos] = (byte)Serial.read();
        pos++;
      } else {
        Serial.read();
      }
    }
  }

  if (waitingForAck) {
    if (millis() - lastSendTime > ackTimeout) {
      if(max_retry>0){
        Serial.println("ACK TIMEOUT. Riprovo...");
        send_data();
        max_retry--;
      }else{
        max_retry=3;
        waitingForAck=false;
        pos=0;
        Serial.println("BIN NOT AVAIABLE!");
      }
    }
    
  } else {
    if (pos > 0) {
      Serial.println("Dati pronti, inizio invio...");
      send_data();
    }
  }
}