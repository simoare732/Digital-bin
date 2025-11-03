#include <SR04.h>
#include <Servo.h>
#define TRIG_PIN 31
#define ECHO_PIN 30
#define SERVO_PIN 9

SR04 sr04 = SR04(ECHO_PIN,TRIG_PIN);
Servo servo;

const int id = 1;

const unsigned long delay_fill = 2000;  // Delay between two measurements of fill
unsigned long last_fill = millis();   // Timestamp of last measurement

const unsigned long delay_overturn = 2000;  // Delay between two measurements of overturn
unsigned long last_overturn = millis();  // Timestamp of last measurement   

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

void send_fill(){
   long distance=sr04.Distance();
   Serial.print("FILL,");
   Serial.print(id);
   Serial.print(",");
   Serial.println(distance);
}

void checkSerialCommands() {
  while (Serial.available() > 0) {
    byte inByte = Serial.read();

    switch (currentRxState) {
      
      case WAIT_FOR_START:
        // 1. Aspettiamo solo il byte d'inizio
        if (inByte == 0xFE) {
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

void processCommand(byte commandType, String payload){
  switch(commandType){
    case CMD_LOCK:
      if(payload.toInt() == 1) servo.write(90);
      if(payload.toInt() == 0) servo.write(0);
      break;
    
    case CMD_LCD:
      //logica lcd
      break;
    
    default:
      break;
  }
}

void setup() {
   servo.attach(SERVO_PIN);
   Serial.begin(9600);
   delay(1000);
   //digitalWrite(LED, LOW);

   Serial.print("POSITION,");
   Serial.print(id);
   Serial.print(",");
   Serial.print(lat);
   Serial.print(",");
   Serial.println(lon);
}

void loop() {
   if(millis() - last_fill >= delay_fill){
      last_fill = millis();
      send_fill();
   }
   
   if(millis() - last_overturn >= delay_overturn){
      last_overturn = millis();
      //send_overturn();
   }

   checkSerialCommands();
}
