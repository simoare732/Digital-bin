
#include <SPI.h> 
#include <LoRa.h> 
#include <MFRC522.h>
//Dependencies for qrcode
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>

#include <SR04.h>
#include <Servo.h>

// ----- LoRa Pins -----
#define LORA_CS_PIN     53  // Chip Select (NSS) - Pin SS hardware del Mega
#define LORA_RESET_PIN  7   // Reset (RST)
#define LORA_IRQ_PIN    2   // Interrupt (DIO0)

// ----- Ultrasonic Sensor Pins -----
#define TRIG_PIN 31
#define ECHO_PIN 30

// ----- Servo Pin -----
#define SERVO_PIN 9

// ----- RFID Pin ------
#define RC522_SS_PIN 47
#define RC522_RST_PIN 48

// ----- Impostazioni Display -----
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 
#define OLED_RESET -1    
#define SCREEN_ADDRESS 0x3C 

SR04 sr04 = SR04(ECHO_PIN,TRIG_PIN);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28); // BNO055
MFRC522 rfid(RC522_SS_PIN, RC522_RST_PIN);
Servo servo;

byte defaultCard[4] = { 0xA3, 0xB3, 0x14, 0x35 };

const int id = 1;

const byte RX = 0xAA; // Code of the LoRa Receiver
const long lora_frequency = 433E6; // Frequency of LoRa module

const unsigned long delay_fill = 3000;  // Delay between two measurements of fill
unsigned long last_fill = 0;   // Timestamp of last measurement

const unsigned long delay_overturn = 10000;  // Delay between two measurements of overturn
unsigned long last_overturn = 0;  // Timestamp of last measurement   

enum RXState{
   WAIT_FOR_START,
   WAIT_FOR_TYPE,
   RECEIVING_PAYLOAD
};
RXState currentRxState = WAIT_FOR_START;
byte cmd; //Command received
char payloadBuffer[100]; //Buffer where receive the payload from Bridge
int bufferIndex = 0;

const byte CMD_UNKNOWN = 0x00;
const byte CMD_LOCK = 0x01;
const byte CMD_LCD = 0x02;

const float lat = 44.65;
const float lon = 10.93;

//Normal position of BNO055
const float turnY = 0.40;
const float turnZ = 9.80;

void send_position(){
  LoRa.beginPacket();
  LoRa.write(RX);
  LoRa.print("POSITION,");
  LoRa.print(id);
  LoRa.print(",");
  LoRa.print(lat);
  LoRa.print(",");
  LoRa.println(lon);
  LoRa.endPacket();
}

void send_fill(){
  long distance=sr04.Distance();

  LoRa.beginPacket();
  LoRa.write(RX);
  LoRa.print("FILL,");
  LoRa.print(id);
  LoRa.print(",");
  LoRa.print(distance);
  LoRa.endPacket();

  //LoRa.receive();
}

void send_overturn(){
  imu::Vector<3> gravity = bno.getVector(Adafruit_BNO055::VECTOR_GRAVITY);
  bool isOverturn = false;
  if(abs(gravity.y() - turnY) > 1 && abs(gravity.z() - turnZ) > 1)
    isOverturn = true;

  LoRa.beginPacket();
  LoRa.write(RX);
  LoRa.print("OVERTURN,");
  LoRa.print(id);
  LoRa.print(",");
  LoRa.print(isOverturn);
  LoRa.endPacket();

  //LoRa.receive();
}

void checkSerialCommands() {
  int packetSize = LoRa.parsePacket();

  if(packetSize){
    byte recipient = LoRa.read();
    if((int)recipient != id){  
      while(LoRa.available()){LoRa.read();}
      return;
    }

    while (LoRa.available()) {
      byte inByte = LoRa.read();
      Serial.println(inByte);

      switch (currentRxState) {
        
        case WAIT_FOR_START:
          // 1. Aspettiamo solo il byte d'inizio
          if (inByte == 0xFE) {
            //byte recipient = LoRa.read();  //Destinatario
            //byte sender = LoRa.read();  // Mittente
            //if(recipient == id && sender == RX)
            currentRxState = WAIT_FOR_TYPE; // Trovato! Passa allo stato successivo
          }
          // Ignora qualsiasi altro byte
          break;

        case WAIT_FOR_TYPE:
          // 2. Il primo byte dopo START è il tipo di comando
          cmd = inByte;

          bufferIndex = 0;           // Resetta l'indice del buffer
          currentRxState = RECEIVING_PAYLOAD; // Passa alla ricezione dati
          break;

        case RECEIVING_PAYLOAD:
          // 3. Stiamo ricevendo i dati
          if (inByte == 0xFF) {
            // Trovato byte di fine! Pacchetto completo.
            payloadBuffer[bufferIndex] = '\0'; // Termina la stringa
            
            // Esegui il comando
            processCommand(cmd, String(payloadBuffer));
            
            // Torna allo stato iniziale, pronto per il prossimo pacchetto
            currentRxState = WAIT_FOR_START; 
            
          } else {
            // Aggiungi il byte al buffer del payload
            if (bufferIndex < sizeof(payloadBuffer) - 1) {
              payloadBuffer[bufferIndex] = (char)inByte;
              bufferIndex++;
            }
            // (Se il buffer è pieno, i dati extra vengono ignorati
            //  fino all'arrivo di END_BYTE, prevenendo overflow)
          }
          break;
      }
    }
  }
}

void processCommand(byte commandType, String payload){
  Serial.println(payload);
  switch(commandType){
    case CMD_LOCK:
      if(payload.toInt() == 1) servo.write(90);
      if(payload.toInt() == 0) servo.write(0);
      break;
    
    case CMD_LCD:
      int commaIndex = payload.indexOf(',');
      int distance = 0;
      char dir = '?';
      if(commaIndex != -1){
        String distanceStr = payload.substring(0,commaIndex);
        distanceStr.trim();
        distance = distanceStr.toInt();

        String dirStr = payload.substring(commaIndex+1);
        dirStr.trim();

        if(distance == 0 && dirStr == 'ok'){
          display.clearDisplay();
          display.display();
          break;
        }
        
        if(dirStr.length() > 0)dir = dirStr[0];
      }
      showDirections(distance, dir);
      break;
    
    default:
      break;
  }
}

void showDirections(int distance, char dir) {
  
  // 1. Pulisci il buffer (sfondo NERO)
  display.clearDisplay(); 
  
  // 2. Imposta il colore del testo/disegno su BIANCO
  display.setTextColor(SSD1306_WHITE); 

  // --- Linea 1: "XXX m" (es. 100 m) ---
  
  String distStr = String(distance) + " m"; 
  display.setTextSize(2); // Dimensione testo media
  
  int16_t x1_text, y1_text;
  uint16_t w_text, h_text;
  
  display.getTextBounds(distStr, 0, 0, &x1_text, &y1_text, &w_text, &h_text);
  
  int16_t x_pos_1 = (SCREEN_WIDTH - w_text) / 2;
  int16_t y_pos_1 = 4; // 4 pixel dall'alto (nella zona gialla)
  
  display.setCursor(x_pos_1, y_pos_1);
  display.print(distStr);

  // --- Linea 2: Disegna la Freccia Completa ---
  
  // Punto di riferimento centrale per la freccia (nella zona blu)
  int16_t center_x = SCREEN_WIDTH / 2; // Centro X = 64
  int16_t center_y = 44; // Centro Y (verticale) nella zona blu (16-64)
  
  int16_t arrowHeadSize = 8; // Dimensione della punta del triangolo
  int16_t arrowShaftWidth = 4; // Larghezza dell'asta della freccia
  int16_t arrowShaftLength = 16; // Lunghezza dell'asta della freccia

  switch (dir) {
    
    case 'U': // Freccia SU
      // Asta (rettangolo verticale)
      display.fillRect(
        center_x - arrowShaftWidth / 2, // X inizio asta
        center_y - arrowShaftLength / 2, // Y inizio asta
        arrowShaftWidth, // Larghezza
        arrowShaftLength, // Altezza
        SSD1306_WHITE
      );
      // Punta (triangolo)
      display.fillTriangle(
        center_x, center_y - arrowShaftLength / 2 - arrowHeadSize, // Punta in alto
        center_x - arrowHeadSize, center_y - arrowShaftLength / 2, // Base sinistra
        center_x + arrowHeadSize, center_y - arrowShaftLength / 2, // Base destra
        SSD1306_WHITE
      );
      break;

    case 'R': // Freccia DESTRA
      // Asta (rettangolo orizzontale)
      display.fillRect(
        center_x - arrowShaftLength / 2, // X inizio asta
        center_y - arrowShaftWidth / 2, // Y inizio asta
        arrowShaftLength, // Lunghezza
        arrowShaftWidth, // Larghezza
        SSD1306_WHITE
      );
      // Punta (triangolo)
      display.fillTriangle(
        center_x + arrowShaftLength / 2 + arrowHeadSize, center_y, // Punta a destra
        center_x + arrowShaftLength / 2, center_y - arrowHeadSize, // Base alta
        center_x + arrowShaftLength / 2, center_y + arrowHeadSize, // Base bassa
        SSD1306_WHITE
      );
      break;

    case 'L': // Freccia SINISTRA
      // Asta (rettangolo orizzontale)
      display.fillRect(
        center_x - arrowShaftLength / 2, // X inizio asta
        center_y - arrowShaftWidth / 2, // Y inizio asta
        arrowShaftLength, // Lunghezza
        arrowShaftWidth, // Larghezza
        SSD1306_WHITE
      );
      // Punta (triangolo)
      display.fillTriangle(
        center_x - arrowShaftLength / 2 - arrowHeadSize, center_y, // Punta a sinistra
        center_x - arrowShaftLength / 2, center_y - arrowHeadSize, // Base alta
        center_x - arrowShaftLength / 2, center_y + arrowHeadSize, // Base bassa
        SSD1306_WHITE
      );
      break;

    case 'D': // Freccia GIÙ
      // Asta (rettangolo verticale)
      display.fillRect(
        center_x - arrowShaftWidth / 2, // X inizio asta
        center_y - arrowShaftLength / 2, // Y inizio asta
        arrowShaftWidth, // Larghezza
        arrowShaftLength, // Altezza
        SSD1306_WHITE
      );
      // Punta (triangolo)
      display.fillTriangle(
        center_x, center_y + arrowShaftLength / 2 + arrowHeadSize, // Punta in basso
        center_x - arrowHeadSize, center_y + arrowShaftLength / 2, // Base sinistra
        center_x + arrowHeadSize, center_y + arrowShaftLength / 2, // Base destra
        SSD1306_WHITE
      );
      break;

    default:
      // Se il carattere non è riconosciuto, stampa un '?' al centro
      display.setTextSize(3);
      display.getTextBounds("?", 0, 0, &x1_text, &y1_text, &w_text, &h_text);
      display.setCursor((SCREEN_WIDTH - w_text) / 2, 35);
      display.print("?");
      break;
  }

  // 3. Invia il buffer al display per mostrarlo
  display.display();
}

/**
 * @brief Controlla la presenza di un tag RFID e muove il servo se l'UID corrisponde.
 * QUESTA E' LA FUNZIONE CHE HAI CHIESTO.
 */
void checkRFID() {
  // Cerca un nuovo tag (questa funzione non è bloccante)
  if (!rfid.PICC_IsNewCardPresent()) {
    return; // Niente tag, esci
  }

  // Prova a leggerlo
  if (!rfid.PICC_ReadCardSerial()) {
    return; // Lettura fallita, esci
  }

  // Confronta l'UID letto con il tuo UID di default
  if (compareUID(rfid.uid.uidByte, defaultCard, rfid.uid.size)) {    
    servo.write(90); // Muove il servo a 90 gradi
    delay(300);     // Attende 1 secondo
    servo.write(0);  // Ritorna alla posizione 0
    
  }

  // "Parcheggia" il tag per evitare di leggerlo di nuovo subito
  rfid.PICC_HaltA();
}

/**
 * @brief Funzione helper per confrontare due array di byte (gli UID).
 * @return true se sono identici, false altrimenti.
 */
bool compareUID(byte scannedUID[], byte masterUID[], byte size) {
  for (byte i = 0; i < size; i++) {
    if (scannedUID[i] != masterUID[i]) {
      return false; // Appena trova un byte diverso, esce
    }
  }
  return true; // Se il loop finisce senza differenze, sono identici
}

void setup() {
  servo.attach(SERVO_PIN);
  LoRa.setPins(LORA_CS_PIN, LORA_RESET_PIN, LORA_IRQ_PIN);
  Serial.begin(9600);
  delay(1000);
  //digitalWrite(LED, LOW);

  SPI.begin();
  rfid.PCD_Init();
  rfid.PCD_DumpVersionToSerial(); 

  if (!LoRa.begin(433E6)) { 
    Serial.println("LoRa init failed!");
    while (1); 
  }

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
   Serial.println(F("Inizializzazione SSD1306 fallita")); 
   while(1);
  }

  if (!bno.begin()) {
    Serial.print("BNO055 non rilevato! Controlla i collegamenti I2C.");
    while (1);
  }
  bno.setExtCrystalUse(true);

  send_position();

  display.clearDisplay();
  display.display();

}

void loop() {
   if(millis() - last_fill >= delay_fill){
      last_fill = millis();
      send_fill();
   }
   
   if(millis() - last_overturn >= delay_overturn){
      last_overturn = millis();
      send_overturn();
   }

   checkSerialCommands();

   checkRFID();
}
