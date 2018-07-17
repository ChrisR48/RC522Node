

/**
   ----------------------------------------------------------------------------
   This is a RC522Node; see https://github.com/ChrisR48/RC522Node
   for further details

   NOTE: The node uses MFRC522 library from: https://github.com/miguelbalboa/rfid

   Typical pin layout used:
   -----------------------------------------------------------------------------------------
               MFRC522      Arduino       Arduino   Arduino    Arduino          Arduino
               Reader/PCD   Uno/101       Mega      Nano v3    Leonardo/Micro   Pro Micro
   Signal      Pin          Pin           Pin       Pin        Pin              Pin
   -----------------------------------------------------------------------------------------
   RST/Reset   RST          9             5         D9         RESET/ICSP-5     RST
   SPI SS      SDA(SS)      10            53        D10        10               10
   SPI MOSI    MOSI         11 / ICSP-4   51        D11        ICSP-4           16
   SPI MISO    MISO         12 / ICSP-1   50        D12        ICSP-1           14
   SPI SCK     SCK          13 / ICSP-3   52        D13        ICSP-3           15

*/


#include <SPI.h>
#include <deprecated.h>
#include <MFRC522.h>
#include <MFRC522Extended.h>
#include <require_cpp11.h>
#include <EEPROM.h>

#define LED_ON LOW
#define LED_OFF HIGH

#define BUTTON_ON LOW
#define BUTTON_OFF HIGH

const uint8_t OUTPUT_NODE = 2;
const uint8_t OUTPUT_TAG  = 3;
const uint8_t INPUT_BTN   = 4;

const uint8_t SS_PIN = 10;
const uint8_t RST_PIN = 9;

MFRC522 mfrc522(SS_PIN, RST_PIN);    // Create MFRC522 instance.

MFRC522::MIFARE_Key key;

bool refDataSet = false;
byte refData;


void setup() {

  pinMode(OUTPUT_NODE, OUTPUT);
  digitalWrite(OUTPUT_NODE, LED_OFF); //Node is not ready

  pinMode(OUTPUT_TAG, OUTPUT);
  digitalWrite(OUTPUT_TAG, LED_OFF); //Node is not ready

  pinMode(INPUT_BTN, INPUT_PULLUP);

  Serial.begin(9600); // Initialize serial communications with the PC
  while (!Serial);    // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)

  Serial.println(F("System start."));

  SPI.begin();        // Init SPI bus

  Serial.print(F("MFRC522 Init() "));
  mfrc522.PCD_Init(); // Init MFRC522 card
  Serial.println(F("done"));

  while (!mfrc522.PCD_PerformSelfTest()) {
    Serial.println(F("MFRC522 Selftest failed"));
    delay(1000);
  }
  Serial.println(F("MFRC522 Selftest passed"));

  mfrc522.PCD_Init(); ///* Init again after Self Test otherwise no tags will be detected

  mfrc522.PCD_DumpVersionToSerial();

  Serial.println(F("MFRC522 Setting Antenna Gain to MAX"));
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);

  if (EEPROM.read(1) != 48) { //Is tag defined in EEPROM?
    Serial.println(F("No Tag is stored in EEPROM, press button to store tag"));
  } else {
    refData = EEPROM.read(2);
    refDataSet = true;
  }

  digitalWrite(OUTPUT_NODE, LED_ON); //Node is ready

  Serial.println(F("Startup completed"));

  // Prepare the key (used both as key A and as key B)
  // using FFFFFFFFFFFFh which is the default at chip delivery from the factory
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
}

bool writeNextTag = false;
byte writeData;

bool readRef = false;

bool tagMatches = false;
unsigned long tagMatchesTime = 0;

void writeConfig() {
  if (EEPROM.read(1) != 48) {
    EEPROM.write(1, 48);
  }

  if (EEPROM.read(2) != refData) {
    EEPROM.write(2, refData);
  }
  refDataSet = true;
}

void checkCommand() {
  if (Serial.available() > 3) {
    Serial.println(F("Start of command found"));
    if (Serial.read() == '$') {
      char c = Serial.read();
      if ( c == 'r') {
        Serial.print(F("Reference Data: "));
        if (refDataSet) {
          Serial.println(refData, HEX);
        } else {
          Serial.println(F("Not Set"));
        }
      } else if (c == 'w') {
        if (!readRef) {
          Serial.print(F("Writing to next tag: "));
          byte data = Serial.read();
          writeData = data; 
          Serial.println(writeData, HEX);
          writeNextTag = true;
        } else {
          Serial.println(F("Skipped writing as read reference is still active"));
        }
      } else if (c == 'c') {
        if (writeNextTag) {
          Serial.print(F("Writing canceled"));
          writeNextTag = false;
        }
      }
    }
  }
}

bool buttonPressed = false;

void checkButton() {

  bool cButton = (digitalRead(INPUT_BTN) == BUTTON_ON);

  if (!buttonPressed && cButton ) {
    buttonPressed = true;
    if (!writeNextTag) {
      Serial.println(F("Button pressed. Start reading reference from tag"));
      readRef = true;

    } else {
      Serial.println(F("Skipped as write tag is still active"));
    }


  } else if (buttonPressed && !cButton) {
    buttonPressed = false;
    Serial.println(F("Button released"));
    if (readRef) {
      Serial.println(F("Readind reference was not possible, aborting. Tag must be present!"));
      readRef = false;
    }
  }

}

bool tagPresent() {

  // Look for new cards
  if ( !mfrc522.PICC_IsNewCardPresent()) {
    return false;
  }

  //Serial.println(F("MFRC522 Card Present"));

  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial())
    return false;

  //Serial.println(F("MFRC522 Read Serial Successful"));

  MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);

  if (piccType != MFRC522::PICC_Type::PICC_TYPE_MIFARE_UL) {
    Serial.println(F("Tag failure. Only working with MIFARE Ultralight!"));
    return false;
  }

  return true;
}

void controlLED() {

  if (tagMatches) {
    digitalWrite(OUTPUT_TAG, LED_ON);
  } else {
    digitalWrite(OUTPUT_TAG, LED_OFF);
  }

  if (!refDataSet || writeNextTag || readRef) {
    digitalWrite(OUTPUT_NODE, LED_OFF);
  } else {
    digitalWrite(OUTPUT_NODE, LED_ON);
  }
}

void loop() {

  checkCommand();

  checkButton();

  controlLED();

  if (tagPresent()) {
    if (writeNextTag) {
      if (writeTag(writeData)) {
        Serial.println(F("Tag write successful"));
        writeNextTag = false;
      }
    } else {
      byte data;
      if (readTag(data)) {
        if (readRef) {
          Serial.println(F("Tag read successful"));
          Serial.println(data, HEX);
          refData = data;
          writeConfig();
          readRef = false;
        } else if (refDataSet) {
          tagMatches = (data == refData);
          tagMatchesTime = millis();
        }

      }
    }
  }

  if (tagMatches && (millis() - tagMatchesTime > 1000)) {
    tagMatches = false;
  }
}

bool readTag(byte &data) {
  Serial.println(F("readTag()"));
  byte buffer[18]; //We need to read 4 pages at once with crc
  byte byteCount = sizeof(buffer);
  MFRC522::StatusCode result = mfrc522.MIFARE_Read(6,  buffer, &byteCount); //Address 6 is first page for user data

  if (result == MFRC522::STATUS_OK ) {
    //dump_byte_array(buffer, byteCount);
    Serial.print(F("data:"));
    data = buffer[0];
    Serial.println(data, HEX);

    return true;
  }

  return false;
}

bool writeTag(byte data) {

  Serial.println(F("writeTag()"));

  byte buffer[4];
  byte byteCount = sizeof(buffer);

  buffer[0] = data;

  MFRC522::StatusCode result = mfrc522.MIFARE_Ultralight_Write(6,  buffer, &byteCount); //Address 6 is first page for user data

  if (result == MFRC522::STATUS_OK ) {
    Serial.println(F("writeTag() - success"));
    return true;
  } else if (result == MFRC522::STATUS_ERROR ) { //status error occures but still write was successful, ignore for now
    Serial.println(F("writeTag() - STATUS_ERROR, ignored"));
    return true;
  } else {
    Serial.print(F("Status: "));
    Serial.println(mfrc522.GetStatusCodeName(result));
    return false;
  }

}


/**
   Helper routine to dump a byte array as hex values to Serial.
*/
void dump_byte_array(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

