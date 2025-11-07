#include <SPI.h>  
#include <LoRa.h> 

#define LORA_CS_PIN     10  // Chip Select (NSS)
#define LORA_RESET_PIN  9   // Reset (RST)
#define LORA_IRQ_PIN    2   // Interrupt (DIO0)

const byte RX = 0xAA; // Code of the LoRa Receiver
const long timeout_ack = 10000;
long last_try = 0;

String tx_msg = ""; 

bool ack=false;
byte buffer_msg[32];
int pos=0;

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

void send_serial_data(){
  while(!ack){
    if(millis() - last_try >= timeout_ack){
      last_try = millis();
      LoRa.beginPacket();
      LoRa.write(0x01);  
  
      int counter=0;
      while (counter<pos) {
        Serial.println(buffer_msg[counter]);
        LoRa.write(buffer_msg[counter]); // Scrive i byte binari
        counter++;
      }
      LoRa.endPacket();
    }
    
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      byte recipient = LoRa.read();
  
      String incoming = "";
      while (LoRa.available()) {
        incoming += (char)LoRa.read();
      }
  
      if(recipient == RX){
        if(incoming=="ack"){
          ack=true;
        }else{
          Serial.println(incoming);
        }
      }
    }
  }
  ack=false;
  pos=0;
  last_try = 0;
}

void receive_serial_data(){
  while (Serial.available()) {
    buffer_msg[pos] = (byte)Serial.read(); // Scrive i byte binari
    pos++;
  }
  Serial.print("BUFFER: ");
  for(int i=0; i<pos; i++){
    Serial.print(buffer_msg[i]);  
  }
  Serial.println("");
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
    delay(10);
    receive_serial_data();
    send_serial_data();
  }
}