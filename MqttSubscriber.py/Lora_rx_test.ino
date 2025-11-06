/*
 * Sketch Esempio: Ricevitore LoRa (Receiver)
 * Piattaforma: Arduino Uno R3
 * Libreria: "LoRa" di Sandeep Mistry
 *
 * Questo sketch si mette in ascolto di pacchetti LoRa.
 * Quando ne riceve uno, stampa il contenuto e l'RSSI sul Monitor Seriale.
 * Assicurati che i pin e la frequenza corrispondano a quelli del Sender!
 */

#include <SPI.h>  // Necessario per la comunicazione SPI
#include <LoRa.h> // Libreria LoRa

// Definisci i pin di controllo del modulo LoRa
#define LORA_CS_PIN     10  // Chip Select (NSS) - Pin SS hardware dell'Uno
#define LORA_RESET_PIN  9   // Reset (RST)
#define LORA_IRQ_PIN    2   // Interrupt (DIO0) - Deve essere un pin di interrupt (2 o 3 su Uno)

void setup() {
  // Inizializza la comunicazione seriale per il debug
  Serial.begin(9600);
  while (!Serial); // Attende l'apertura della porta seriale
  
  Serial.println("Arduino Uno - LoRa Receiver");

  // Imposta i pin per la libreria LoRa
  LoRa.setPins(LORA_CS_PIN, LORA_RESET_PIN, LORA_IRQ_PIN);

  // Inizializza il modulo LoRa alla frequenza desiderata (es. 868MHz per l'Europa)
  // DEVE CORRISPONDERE ALLA FREQUENZA DEL TRASMETTITORE!
  if (!LoRa.begin(433E6)) { 
    Serial.println("Inizializzazione LoRa fallita!");
    while (1); // Blocco del programma
  }
  
  Serial.println("LoRa inizializzato correttamente!");
  Serial.println("Pronto a ricevere pacchetti...");
}

void loop() {
  // Tenta di "parsare" (analizzare) un pacchetto in arrivo
  int packetSize = LoRa.parsePacket();
  
  // Se packetSize è > 0, un pacchetto è stato ricevuto
  if (packetSize) {
    // Pacchetto ricevuto!
    Serial.print("Pacchetto ricevuto! Dimensione: ");
    Serial.println(packetSize);

    // Leggi i dati del pacchetto (come stringa)
    String receivedData = "";
    while (LoRa.available()) {
      receivedData += (char)LoRa.read();
    }

    // Stampa i dati ricevuti
    Serial.print("Dati: '");
    Serial.print(receivedData);
    Serial.println("'");

    // Stampa la potenza del segnale (RSSI)
    // È un numero negativo, più è vicino a 0, più il segnale è forte.
    Serial.print("RSSI: ");
    Serial.println(LoRa.packetRssi());
  }
  
  // Il loop continua a controllare...
}
