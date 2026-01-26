#include <SPI.h> 
#include <LoRa.h> 
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>

#include <HCSR04.h>
#include <Servo.h>

// ----- Buzzer pin -----
#define BUZZER_PIN 8

// ----- LoRa Pins -----
#define LORA_CS_PIN     53  // Chip Select (NSS)
#define LORA_RESET_PIN  9   // Reset (RST)
#define LORA_IRQ_PIN    2   // Interrupt (DIO0)

// ----- Ultrasonic Sensor Pins -----
#define TRIG_PIN 31
#define ECHO_PIN 30

// ----- Servo Pin -----
#define SERVO_PIN 10

// ----- RFID Pin ------
#define RC522_SS_PIN 49
#define RC522_RST_PIN 48

// ------ Display settings -----
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 
#define OLED_RESET -1    
#define SCREEN_ADDRESS 0x3C

// Ultrasonic sensor 
UltraSonicDistanceSensor distanceSensor(TRIG_PIN, ECHO_PIN);
// Display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
// Rollover sensor
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28); // BNO055


// NFC Sensor
MFRC522 rfid(RC522_SS_PIN, RC522_RST_PIN);
// Servo motor
Servo servo;

// Hex code of the accepted card
byte defaultCard[4] = { 0xA3, 0xB3, 0x14, 0x35 };

// Bin id
const int id = 1;
// Addressing for LoRa
const byte START_SEQUENCE = 0x01; 
const byte MY_ADDRESS = 0xAB;     

const long lora_frequency = 433E6; // Frequency of LoRa module

const unsigned long delay_fill = 10000;  // Delay between two measurements of fill
unsigned long last_fill = 0;   // Timestamp of last measurement
const unsigned int max_distance = 38; // Height in cm of the bin

const unsigned long delay_overturn = 15000;  // Delay between two measurements of overturn
unsigned long last_overturn = 0;  // Timestamp of last measurement   

// States of communication LoRa
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

const float lat = 41.9028;
const float lon = 12.4964;

//Normal position of BNO055
const float turnY = 9.63;
const float turnZ = -0.62;


// Function to send position of bin from Arduino on Bin to Arduino Receiver
void send_position(){
  LoRa.beginPacket();
  LoRa.write(START_SEQUENCE);
  LoRa.print("POSITION,");
  LoRa.print(id);
  LoRa.print(",");
  LoRa.print(lat);
  LoRa.print(",");
  LoRa.println(lon);
  LoRa.endPacket();
}

/*
//OLD
// Function to send level of filling (%) from Arduino on Bin to Arduino Receiver
void send_fill(){
  long distanceRaw=sr04.Distance();
  int distance = converte_distance(distanceRaw);

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.print(" , DistanceRaw: ");
  Serial.println(distanceRaw);

  LoRa.beginPacket();
  LoRa.write(START_SEQUENCE);
  LoRa.print("FILL,");
  LoRa.print(id);
  LoRa.print(",");
  LoRa.print(distance);
  LoRa.endPacket();
}*/

bool isOpen = true;

void send_fill(){
  float reading = distanceSensor.measureDistanceCm();
  
  if(reading < 0) reading = max_distance; 

  int distanceRaw = (int)reading;
  int distancePercent = converte_distance(distanceRaw);

  Serial.print("Dist: "); Serial.print(distancePercent);
  Serial.print("% , Raw: "); Serial.println(distanceRaw);


  // Make LoRa packet
  LoRa.beginPacket();
  LoRa.write(START_SEQUENCE);
  LoRa.print("FILL,");
  LoRa.print(id);
  LoRa.print(",");
  LoRa.print(distancePercent);
  LoRa.endPacket();
}

// Function to send a boolean of overturn from Arduino on Bin to Arduino Receiver
void send_overturn(){
  imu::Vector<3> gravity = bno.getVector(Adafruit_BNO055::VECTOR_GRAVITY);
  
  bool isOverturn = false;
  if(abs(gravity.y() - turnY) > 1 && abs(gravity.z() - turnZ) > 1)
    isOverturn = true;
  
  LoRa.beginPacket();
  LoRa.write(START_SEQUENCE);
  LoRa.print("OVERTURN,");
  LoRa.print(id);
  LoRa.print(",");
  LoRa.print(isOverturn);
  LoRa.endPacket();
}

// Function to convert the filling level from cm to %
int converte_distance(int d){
  int dist = constrain(d, 0, max_distance);
  int distanceConverted = map(dist, 0, max_distance, 100, 0);

  return distanceConverted;
}

// Function to calculate CRC
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

// Function to send CRC back to the device as Acknowledge 
void send_CRC(byte *rx_buffer, int pos_buf){
      uint16_t receivedCRC = calculateCRC16(rx_buffer, pos_buf);
      LoRa.beginPacket();
      LoRa.write(START_SEQUENCE);
        
      LoRa.write((byte)(receivedCRC & 0xFF)); 
      LoRa.write((byte)(receivedCRC >> 8));  
        
      LoRa.endPacket();
      pos_buf=0;
}

// Function to check the command sent from device. It reads the command byte per byte using FSM. 
// When the Arduino has read the message, it sends back an Ack message to device
void checkSerialCommands() {
  byte rx_buffer[32];
  int pos_buf=0;
  int packetSize = LoRa.parsePacket();

  // If there are some messages in LoRa buffer, read them
  if(packetSize){
    // Start reading the first two bytes, to ensure the message is for it
    byte startSQ = (byte)LoRa.read();
    byte addr = (byte)LoRa.read();
    if(startSQ==START_SEQUENCE and addr==MY_ADDRESS){
      // For each byte, read it
      while (LoRa.available()) {
        byte inByte = LoRa.read();
        rx_buffer[pos_buf] = inByte;
        pos_buf++;
      
        Serial.println(inByte);

        switch (currentRxState) {
          
          case WAIT_FOR_START:
            // 1. We wait the starting byte FE
            if (inByte == 0xFE) {
              currentRxState = WAIT_FOR_TYPE; // Ok, we pass to next state
            }
            // If other bytes are read, ignore them
            break;

          case WAIT_FOR_TYPE:
            // 2. The first byte read after the starting byte, is the type of command
            cmd = inByte;

            bufferIndex = 0;           // Reset buffer index
            currentRxState = RECEIVING_PAYLOAD; // Now we pass to receive data
            break;

          case RECEIVING_PAYLOAD:
            // 3. We are receiving data until the final byte FF
            if (inByte == 0xFF) {
              // Final byte found.
              payloadBuffer[bufferIndex] = '\0'; // Finish the string
              
              // Perform command
              processCommand(cmd, String(payloadBuffer));
              
              // Return to initial state for new commands
              currentRxState = WAIT_FOR_START; 
              
            } else {
              // If we are not finished to read,continue
              if (bufferIndex < sizeof(payloadBuffer) - 1) {
                payloadBuffer[bufferIndex] = (char)inByte;
                bufferIndex++;
              }
              // If buffer is full, ignore other bytes until the end byte, to prevent overflow
            }
            break;
        }
      }
      send_CRC(rx_buffer, pos_buf);
    }
  }
}

void processCommand(byte commandType, String payload){
  Serial.println(payload);
  switch(commandType){
    case CMD_LOCK: {
      if(payload.toInt() == 1){
        closeBin();
        isOpen=false;
      }
      if(payload.toInt() == 0) openBin();
      break;
    }
    
    case CMD_LCD: {
      int commaIndex = payload.indexOf(',');
      int distance = 0;
      char dir = '?';
      if(commaIndex != -1){
        String distanceStr = payload.substring(0,commaIndex);
        distanceStr.trim();
        distance = distanceStr.toInt();

        String dirStr = payload.substring(commaIndex+1);
        dirStr.trim();

        if(distance == 0 && dirStr == "ok"){
          display.clearDisplay();
          display.display();
          break;
        }
        
        if(dirStr.length() > 0)dir = dirStr[0];
      }
      Serial.print("Distanza e dir: ");
      Serial.print(distance);
      Serial.print(" ");
      Serial.println(dir);
      showDirections(distance, dir);
      break;
    }
    
    default:
      break;
  }
}

void showDirections(int distance, char dir) {

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE); 

  // --- "XXX m" (es. 100 m) ---
  String distStr = String(distance) + " m"; 
  display.setTextSize(2);
  int16_t x1_text, y1_text;
  uint16_t w_text, h_text;
  display.getTextBounds(distStr, 0, 0, &x1_text, &y1_text, &w_text, &h_text);
  int16_t x_pos_1 = (SCREEN_WIDTH - w_text) / 2;
  int16_t y_pos_1 = 4;
  display.setCursor(x_pos_1, y_pos_1);
  display.print(distStr);

  // --- Arrow ---
  int16_t center_x = SCREEN_WIDTH / 2;
  int16_t center_y = 44;

  // Leggi orientamento corrente del dispositivo (BNO055)
  sensors_event_t event;
  bno.getEvent(&event);
  int16_t orientation = event.orientation.x; // 0° = Nord, 90° = Est, 180° = Sud, 270° = Ovest

  Serial.print("orientation "); Serial.println(orientation);

  // Mappa la direzione richiesta (U/R/D/L/O/N/E/S) agli angoli cardinali
  int16_t targetAngle;
  switch(dir) {
    case 'U': targetAngle = 0;   break; // Nord
    case 'N': targetAngle = 45;  break; // Nord-Est
    case 'R': targetAngle = 90;  break; // Est
    case 'E': targetAngle = 135; break; // Sud-Est
    case 'D': targetAngle = 180; break; // Sud
    case 'S': targetAngle = 225; break; // Sud-Ovest
    case 'L': targetAngle = 270; break; // Ovest
    case 'O': targetAngle = 315; break; // Nord-Ovest
    default:  targetAngle = 0;   break;
  }

  // Calcola l'angolo relativo tra target e orientamento attuale
  int16_t relativeAngle = targetAngle - orientation;
  
  // Normalizza tra 0-360
  while (relativeAngle < 0) relativeAngle += 360;
  while (relativeAngle >= 360) relativeAngle -= 360;

  // COMPENSAZIONE DISPLAY RUOTATO DI 90° (orario)
  relativeAngle = (relativeAngle + 270) % 360;

  // Disegna la freccia con l'angolo preciso
  drawRotatedArrow(center_x, center_y, relativeAngle);

  // 3. Invia il buffer al display
  display.display();
}

// Funzione per disegnare una freccia ruotata di un angolo specifico
void drawRotatedArrow(int16_t cx, int16_t cy, float angleDeg) {
  // Converti angolo in radianti
  float angleRad = angleDeg * PI / 180.0;
 
  // Parametri freccia
  int16_t arrowLength = 20;      // Lunghezza totale della freccia
  int16_t arrowHeadLength = 8;   // Lunghezza della punta
  int16_t arrowHeadWidth = 8;    // Larghezza della punta
  int16_t shaftWidth = 3;        // Larghezza dell'asta
  
  // Calcola i punti della freccia (orientata verso l'alto, poi ruotata)
  // Punta della freccia
  float tipX = 0;
  float tipY = -arrowLength;
  
  // Base della punta (dove inizia il triangolo)
  float baseY = -arrowLength + arrowHeadLength;
  
  // Punti laterali della punta
  float headLeftX = -arrowHeadWidth;
  float headLeftY = baseY;
  float headRightX = arrowHeadWidth;
  float headRightY = baseY;
  
  // Asta (rettangolo)
  float shaftTopY = baseY;
  float shaftBottomY = arrowLength * 0.3; // L'asta arriva a 30% della lunghezza totale
  float shaftLeftX = -shaftWidth / 2.0;
  float shaftRightX = shaftWidth / 2.0;
  
  // Ruota e trasla tutti i punti
  int16_t tipXr = cx + tipX * cos(angleRad) - tipY * sin(angleRad);
  int16_t tipYr = cy + tipX * sin(angleRad) + tipY * cos(angleRad);
  
  int16_t headLeftXr = cx + headLeftX * cos(angleRad) - headLeftY * sin(angleRad);
  int16_t headLeftYr = cy + headLeftX * sin(angleRad) + headLeftY * cos(angleRad);
  
  int16_t headRightXr = cx + headRightX * cos(angleRad) - headRightY * sin(angleRad);
  int16_t headRightYr = cy + headRightX * sin(angleRad) + headRightY * cos(angleRad);
  
  // Punti dell'asta
  int16_t shaftTL_Xr = cx + shaftLeftX * cos(angleRad) - shaftTopY * sin(angleRad);
  int16_t shaftTL_Yr = cy + shaftLeftX * sin(angleRad) + shaftTopY * cos(angleRad);
  
  int16_t shaftTR_Xr = cx + shaftRightX * cos(angleRad) - shaftTopY * sin(angleRad);
  int16_t shaftTR_Yr = cy + shaftRightX * sin(angleRad) + shaftTopY * cos(angleRad);
  
  int16_t shaftBL_Xr = cx + shaftLeftX * cos(angleRad) - shaftBottomY * sin(angleRad);
  int16_t shaftBL_Yr = cy + shaftLeftX * sin(angleRad) + shaftBottomY * cos(angleRad);
  
  int16_t shaftBR_Xr = cx + shaftRightX * cos(angleRad) - shaftBottomY * sin(angleRad);
  int16_t shaftBR_Yr = cy + shaftRightX * sin(angleRad) + shaftBottomY * cos(angleRad);
  
  // Disegna la punta (triangolo)
  display.fillTriangle(
    tipXr, tipYr,           // Punta
    headLeftXr, headLeftYr, // Base sinistra
    headRightXr, headRightYr, // Base destra
    SSD1306_WHITE
  );
  
  // Disegna l'asta (come due triangoli per formare un rettangolo)
  display.fillTriangle(
    shaftTL_Xr, shaftTL_Yr,
    shaftTR_Xr, shaftTR_Yr,
    shaftBL_Xr, shaftBL_Yr,
    SSD1306_WHITE
  );
  
  display.fillTriangle(
    shaftTR_Xr, shaftTR_Yr,
    shaftBR_Xr, shaftBR_Yr,
    shaftBL_Xr, shaftBL_Yr,
    SSD1306_WHITE
  );
  
  // Opzionale: aggiungi un cerchio al centro per estetica
  display.fillCircle(cx, cy, 2, SSD1306_WHITE);
}

// Function to check if NFC card corresponds to the default one, you can open the bin
void checkRFID() {
  // Search a new tag
  if (!rfid.PICC_IsNewCardPresent()) {
    return; // If no tag, exit
  }

  // Try to read it
  if (!rfid.PICC_ReadCardSerial()) {
    return; // Failed read, exit
  }

  // Compare the read UID with the default one, if they are equal, you can open
  if (compareUID(rfid.uid.uidByte, defaultCard, rfid.uid.size)) {    
    Serial.println("Card read");
    if(!isOpen){
       openBin();    
       isOpen=true; 
    }
  }

  // "Parcheggia" il tag per evitare di leggerlo di nuovo subito
  rfid.PICC_HaltA();
}

// Helper function to compare two UID
bool compareUID(byte scannedUID[], byte masterUID[], byte size) {
  for (byte i = 0; i < size; i++) {
    if (scannedUID[i] != masterUID[i]) {
      return false; // Appena trova un byte diverso, esce
    }
  }
  return true; // Se il loop finisce senza differenze, sono identici
}

// Function to open the bin
void openBin(){
  for(int i=0; i<90; i++){
      servo.write(i);
      delay(20);
  }
}

// Function to close the bin
void closeBin(){
  for(int i=90; i>0; i--){
      servo.write(i);
      delay(20);
  }
}

void setup() {
  servo.attach(SERVO_PIN);
  LoRa.setPins(LORA_CS_PIN, LORA_RESET_PIN, LORA_IRQ_PIN);
  Serial.begin(9600);
  delay(1000);

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
  openBin();

  send_position();

  display.clearDisplay();
  display.display();

  bno.setMode(OPERATION_MODE_NDOF);
  adafruit_bno055_offsets_t myOffsets;
  myOffsets.accel_offset_x = -23;
  myOffsets.accel_offset_y = -7;
  myOffsets.accel_offset_z = 29;
  myOffsets.gyro_offset_x = -2;
  myOffsets.gyro_offset_y = 1;
  myOffsets.gyro_offset_z = -1;
  myOffsets.mag_offset_x = 60;
  myOffsets.mag_offset_y = 64;
  myOffsets.mag_offset_z = 462;
  myOffsets.accel_radius = 1000;
  myOffsets.mag_radius = 665;
  bno.setSensorOffsets(myOffsets);

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
