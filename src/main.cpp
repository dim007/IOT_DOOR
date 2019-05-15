/*
    Author: Diego Martinez
    Date: 5.1.19
    https://github.com/dim007/IOT_DOOR
  

  Driving motor 28BYJ-48 (driver ULN2003) in Full-Step Mode.
  From Spec Sheet each step is 5.625 * 2 = 11.25 Degs.
  Num Steps is 360 / 11.52 = 32 Steps.
  Gear Ratio is 64:1, true steps = 32 * 64 = 2038.

  Using MFRC522 class created by Miguel Balboa for RC522 module.
  https://github.com/miguelbalboa/rfid

  Using Bluetooth 4.0 Module HC-08
  https://www.amazon.com/DSD-TECH-SH-HC-08-Transceiver-Compatible/dp/B01N4P7T0H

 * -----------------------------------------------------------------------------------------
 * Module     Module Pin   Arduino Pin
 * -----------------------------------------------------------------------------------------
 * BLUETOOTH   TX           0(RX)
 * BLUETOOTH   RX           1(TX)
 * BUTTON      +            2
 * LED         +            3
 * MOTOR       IN1          4
 * MOTOR       IN2          5
 * MOTOR       IN3          6
 * MOTOR       IN4          7
 * BUZZER      +            8
 * MFRC522     RST          9
 * MFRC522     SDA(SS)      10
 * MFRC522     MOSI         11 / ICSP-4
 * MFRC522     MISO         12 / ICSP-1
 * MFRC522     SCK          13 / ICSP-3
 */

#include <SPI.h>
#include <MFRC522.h>
#include <Stepper.h>
#include "buzzerMusic.h"

#define BUTTON_PIN 2
#define LED_PIN 3
#define IN1 4
#define IN2 5
#define IN3 6
#define IN4 7
#define BUZZ_PIN 8
#define RST_PIN 9
#define SS_PIN 10
#define stepsPerRev 2038
#define stepsNeeded 1631
#define NUM_KEYS 2
#define KEY_SIZE 4
#define BLUE_PASS_SIZE 7

void lockDoor();
void unlockDoor();
bool scanCard();
bool checkBluetooth();
void printHex(byte *buffer, byte bufferSize);
void printDec(byte *buffer, byte bufferSize);

Stepper stepper(stepsPerRev, IN1,IN3,IN2,IN4); // NOTE: See order, should be 1,3,2,4 to allow clockwise/counter-clockwise movement.
MFRC522 rfid(SS_PIN, RST_PIN); 
char buffer[16];
byte myKeys[NUM_KEYS][KEY_SIZE] = {
  {0xFF, 0xFF, 0xFF, 0xFF},
  {0xFF, 0xFF, 0xFF, 0xFF}
};
byte bluetoothPass[BLUE_PASS_SIZE] = {'F','F','F','F','F','F','F'};
bool doorLocked = false; //Door is unlocked on startup

void setup() { 
  Serial.begin(9600); // Init Bluetooth, NOTE: Will also debug via bluetooth
  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522 
  stepper.setSpeed(15); // max for 28byj-48

  pinMode(BUTTON_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZ_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); //Door is unlocked on startup
  playZeldaMelody(BUZZ_PIN);
}

void loop() {
  // If AnalogButton pressed, OR valid RFID card present OR if Bluetooth command valid, lock/unlock DOOR.
  if((digitalRead(BUTTON_PIN) == HIGH) || scanCard() || checkBluetooth()) {
    if(doorLocked)
      unlockDoor();
    else
      lockDoor();
  }
}

void lockDoor() {
  playAckMelody(BUZZ_PIN);
  delay(10);
  stepper.step(stepsNeeded); 
  playLockMelody(BUZZ_PIN);
  digitalWrite(LED_PIN, LOW);
  doorLocked = true;
  digitalWrite(IN1,LOW); // Release Motor
  digitalWrite(IN2,LOW);
  digitalWrite(IN3,LOW);
  digitalWrite(IN4,LOW);
}
void unlockDoor() {
  playAckMelody(BUZZ_PIN);
  delay(10);
  stepper.step(-stepsNeeded); 
  playZeldaMelody(BUZZ_PIN);
  digitalWrite(LED_PIN, HIGH);
  doorLocked = false;
  digitalWrite(IN1,LOW); // Release Motor
  digitalWrite(IN2,LOW);
  digitalWrite(IN3,LOW);
  digitalWrite(IN4,LOW);
}
/*
 Function checks if received bluetooth data matches our password.
 Or returns lock status.
 */
bool checkBluetooth() {
  if(Serial.available() > 0) {
    int charsRead = Serial.readBytesUntil('\n', buffer, sizeof(buffer)-1);
    buffer[charsRead] = '\0';
    for(byte i = 0; i < BLUE_PASS_SIZE; i++) {
      if(buffer[i] != bluetoothPass[i])
        break;
      if(i == BLUE_PASS_SIZE-1)
        return true;
    }
    // IF password fails, return status of lock
    if(doorLocked)
      Serial.println("Locked.");
    else
      Serial.println("Unlocked");
  }
  return false;
}
/**
 * Wrapper function that abstracts MFRC522 Code.
 */
bool scanCard() {
    // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if ( ! rfid.PICC_IsNewCardPresent() || ! rfid.PICC_ReadCardSerial())
    return false;

  Serial.print(F("PICC type: "));
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  Serial.println(rfid.PICC_GetTypeName(piccType));

  // Check is the PICC of Classic MIFARE type
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&  
    piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
    piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    Serial.println(F("Your tag is not of type MIFARE Classic."));
    rfid.PICC_HaltA();  // Halt PICC
    rfid.PCD_StopCrypto1();   // Stop encryption on PCD
    return false;
  }
  Serial.println(F("The NUID tag is:"));
  Serial.print(F("In hex: "));
  printHex(rfid.uid.uidByte, rfid.uid.size);
  Serial.println();
  char keyMatch = 0;
  for(byte j = 0; j < NUM_KEYS; j++) {
    for(byte i = 0; i < KEY_SIZE; i++) {
      if (myKeys[j][i] != rfid.uid.uidByte[i])
        break;
      else
        keyMatch++;    
    }
    if(keyMatch == (char) KEY_SIZE) {
      rfid.PICC_HaltA();  // Halt PICC
      rfid.PCD_StopCrypto1();   // Stop encryption on PCD
      return true;
    }
  }

  rfid.PICC_HaltA();  // Halt PICC
  rfid.PCD_StopCrypto1();   // Stop encryption on PCD
  return false;
}

/**
 * Helper routine to dump a byte array as hex values to Serial. 
 */
void printHex(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

/**
 * Helper routine to dump a byte array as dec values to Serial.
 */
void printDec(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], DEC);
  }
}