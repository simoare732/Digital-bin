#include <SR04.h>
#define TRIG_PIN 31
#define ECHO_PIN 30
SR04 sr04 = SR04(ECHO_PIN,TRIG_PIN);
long distance;
const int id = 1;

void setup() {
   Serial.begin(9600);
   delay(100);
   //digitalWrite(LED, LOW);
}

void loop() {
   distance=sr04.Distance();
   Serial.print("FILL,");
   Serial.print(id);
   Serial.print(",");
   Serial.println(distance);
   delay(1000);
}
