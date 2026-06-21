#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>

//pins
#define RST_PIN 5
#define SS_PIN 53
#define SERVO_PIN 9
#define TRIG_PIN 7
#define ECHO_PIN 6

//pins hidden
// Miso -> 50
// Mosi -> 51
// SCK -> 52

//vars
#define SERVO_OPEN 0
#define SERVO_CLOSED 90
#define MAX_DISTANCE 3    // Abstand in cm für Person

//structur für arr
struct AuthorizedRFID {
  byte uid[12];
  char name[30];
};

//authed chips
AuthorizedRFID authorizedChips[] = {
  {"01 02 03 04", "WOKWI: Blue Cart"},
  {"11 22 33 44", "WOKWI: Green Cart"},
  {"c9 46 31 03", "meine Karte"},
  {"43 86 c1 01", "mein Chip"}
};

#define NUM_CHIPS (sizeof(authorizedChips) / sizeof(authorizedChips[0]))

//global vars
MFRC522 mfrc522(SS_PIN, RST_PIN);
Servo doorServo;

int accessLimit = 0;           // Limits vom Benutzer
int accessCount = 0;           // Anzahl der durchgelassenen Personen
bool servoOpen = false;        // Status des Servos
unsigned long lastRFIDTime = 0; // Zeit des letzten RFID Scans
const unsigned long RFID_DEBOUNCE = 2000; // 2 Sekunden Entprellungszeit

void setup() {
  Serial.begin(9600);
  while (!Serial);
  
  delay(1000);
  
  Serial.println("\n RFID CONTROL oder so");
  Serial.println("gib mal das Zugrifflimit ein (Anzahl Personen):\n");
  
  // Warte auf Eingabe des Limits
  while (accessLimit <= 0) {
    if (Serial.available() > 0) {
      accessLimit = Serial.parseInt();
      if (accessLimit > 0) {
        Serial.print("Limit gespeichert: ");
        Serial.println(accessLimit);
        Serial.println("Warte auf RFID Chips...\n");
      } else {
        Serial.println("Ungültige Eingabe! Bitte positive Zahl eingeben:");
      }
    }
  }
  
  // RFID Sensor initialisieren
  SPI.begin();
  mfrc522.PCD_Init();
  
  // Servo initialisieren
  doorServo.attach(SERVO_PIN);
  doorServo.write(SERVO_CLOSED); // Servo geschlossen
  
  // Ultraschall Sensor Pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  Serial.println("System bereit!");
  printAuthorizedChips();
}

void loop() {
  // limit überprüf
  if (accessCount >= accessLimit) {
    if (servoOpen) {
      closeServo();
    }
    Serial.println("\n LIMIT ERREICHT");
    delay(1000);
    return;
  }
  
  // Prüfe auf RFID Chip
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    unsigned long currentTime = millis();
    
    // debounce
    if (currentTime - lastRFIDTime > RFID_DEBOUNCE) {
      handleRFIDScan();
      lastRFIDTime = currentTime;
    }
    
    mfrc522.PICC_HaltA();
  }
  
  //personen durchgang prüfung
  if (servoOpen) {
    checkUltrasonicSensor();
  }
  
  delay(100);
}

// rfid scan
void handleRFIDScan() {
  Serial.print("\nRFID Chip gescannt: ");
  printRFIDHex();
  
  // Prüfung chip auth.
  int chipIndex = findRFIDChip();
  
  if (chipIndex != -1) {
    Serial.print("Autorisiert: ");
    Serial.println(authorizedChips[chipIndex].name);
    
    accessCount++;
    Serial.print("Zugang: ");
    Serial.print(accessCount);
    Serial.print("/");
    Serial.println(accessLimit);
    
    openServo();
  } else {
    Serial.println(" NICHT AUTORISIERT - Zugang verweigert!");
  }
}

//  suchen rfid chip
int findRFIDChip() {
  for (int i = 0; i < NUM_CHIPS; i++) {
    if (compareUID(authorizedChips[i].uid)) {
      return i;
    }
  }
  return -1;
}

bool compareUID(char* uidString) {
  char scannedUID[12] = "";  

  // Scannierte UID in String-Format konvertieren
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    char hex[3];
    sprintf(hex, "%02X", mfrc522.uid.uidByte[i]);
    strcat(scannedUID, hex);
    if (i < mfrc522.uid.size - 1) {
      strcat(scannedUID, " ");
    }
  }
  
  return strcmp(scannedUID, uidString) == 0;
}

//rfid hex asugabe
void printRFIDHex() {
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) {
      Serial.print("0");
    }
    Serial.print(mfrc522.uid.uidByte[i], HEX);
    if (i < mfrc522.uid.size - 1) {
      Serial.print(" ");
    }
  }
  Serial.println();
}

void openServo() {
  if (!servoOpen) {
    doorServo.write(SERVO_OPEN);
    servoOpen = true;
    Serial.println("Servo OFFEN");
  }
}

void closeServo() {
  if (servoOpen) {
    doorServo.write(SERVO_CLOSED);
    servoOpen = false;
    Serial.println("Servo GESCHLOSSEN");
  }
}

//sonic messugn
void checkUltrasonicSensor() {
  // Sende Ultraschall Impuls
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  // Lese Echozeit
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  
  // Berechne Entfernung (Schallgeschwindigkeit ca = 343 m/s)
  long distance = duration * 0.0343 / 2;
  
  if (distance < MAX_DISTANCE) {
    delay(1000);
    closeServo();
  }
}

//  AUTORISIERTE CHIPS DRUCKEN 
void printAuthorizedChips() {
  Serial.println("\n Autorisierte RFID Chips");
  for (int i = 0; i < NUM_CHIPS; i++) {
    Serial.print(i + 1);
    Serial.print(". ");
    Serial.println(authorizedChips[i].name);
  }
}
