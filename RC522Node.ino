/**
   ----------------------------------------------------------------------------
   This is a RC522Node; see https://github.com/ChrisR48/RC522Node
   for further details

   NOTE:
   The node uses MFRC522 library from: https://github.com/miguelbalboa/rfid
   For Serial Command, this library is used: https://github.com/joshmarinacci/CmdArduino

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

  Installation nodes:

   go to https://github.com/miguelbalboa/rfid and install the latest version into Arduino IDE
   go to https://github.com/joshmarinacci/CmdArduino and install the latest version into Arduino IDE

  Command Line:

  *************** CMD *******************
  CMD >> help
  usage: command [parameter ...]
    commands:
     * help: prints this help text
     * clear: clears eeprom data
     * write: write data to tag
     * cancel: cancel write
     * read: read reference data from eeprom
     * store: store reference data in eeprom
     * status: print status information

*/


#include <SPI.h>
#include <deprecated.h>
#include <MFRC522.h>
#include <MFRC522Extended.h>
#include <require_cpp11.h>
#include <EEPROM.h>

#include <Cmd.h>

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

bool refDataSet = false;
byte refData;

bool writeNextTag = false;
byte writeData;

bool readRef = false;

bool tagMatches = false;
unsigned long tagMatchesTime = 0;

bool buttonPressed = false;

boolean tag_present = false;
unsigned long tag_present_time = 0;
boolean tag_read = false;

//**************************** CLI START ***************

const char* CMD_HELP    = "help";
const char* CMD_CLEAR   = "clear";
const char* CMD_WRITE   = "write";
const char* CMD_READ    = "read";
const char* CMD_STORE   = "store";
const char* CMD_CANCEL  = "cancel";
const char* CMD_STATUS  = "status";

void help(int arg_cnt, char **args) {
  Serial.println(F("usage: command [parameter ...]"));
  Serial.println(F("  commands:"));
  Serial.print(F("   * ")); Serial.print(CMD_HELP);   Serial.println(F(": prints this help text"));
  Serial.print(F("   * ")); Serial.print(CMD_CLEAR);  Serial.println(F(": clears eeprom data"));
  Serial.print(F("   * ")); Serial.print(CMD_WRITE);  Serial.println(F(": write data to tag"));
  Serial.print(F("   * ")); Serial.print(CMD_CANCEL); Serial.println(F(": cancel write"));
  Serial.print(F("   * ")); Serial.print(CMD_READ);   Serial.println(F(": read reference data from eeprom"));
  Serial.print(F("   * ")); Serial.print(CMD_STORE);  Serial.println(F(": store reference data in eeprom"));
  Serial.print(F("   * ")); Serial.print(CMD_STATUS); Serial.println(F(": print status information"));
}

void clearEEPROM(int arg_cnt, char **args) {
  Serial.print(F("Clearing eeprom"));
  EEPROM.write(1, 0);
  refDataSet = false;
}

void readEEPROM(int arg_cnt, char **args) {
  Serial.print(F("Reference Data: "));
  if (refDataSet) {
    Serial.println(refData, HEX);
  } else {
    Serial.println(F("Not Set"));
  }
}

void writeEEPROM(int arg_cnt, char **args) {

  if (arg_cnt != 2) {
    Serial.println("Invalid argument count for store");
    Serial.println("Usage: store DATA");
    return;
  }

  String param(args[1]);

  if (param.length() != 1) {
    Serial.println("Invalid parameter, must be one character");
    return;
  }

  refData = (byte) param.charAt(0);

  Serial.print(F("Writing to eeprom: "));
  Serial.println((char) refData);

  writeConfig();
}

void writeToTag(int arg_cnt, char **args) {
  if (arg_cnt != 2) {
    Serial.println("Invalid argument count for write");
    Serial.println("Usage: write DATA");
    return;
  }

  String param(args[1]);

  if (param.length() != 1) {
    Serial.println("Invalid parameter, must be one character");
    return;
  }

  writeData = (byte) param.charAt(0);

  Serial.print(F("Writing to next tag: "));
  Serial.println(writeData, HEX);
  writeNextTag = true;

}

void cancel(int arg_cnt, char **args) {
  Serial.print(F("Writing canceled"));
  writeNextTag = false;
}

void status(int arg_cnt, char **args) {
  Serial.println(F("STATUS INFO:"));
  Serial.print(F("  Reference Data "));
  if (refDataSet) {
    Serial.println(F("set"));
  } else {
    Serial.println(F("not set"));
  }
  Serial.print(F("  Write Tag "));
  if (writeNextTag) {
    Serial.println(F("active"));
  } else {
    Serial.println(F("NOT active"));
  }

  Serial.print(F("  Button "));
  if (buttonPressed) {
    Serial.println(F("pressed"));
  } else {
    Serial.println(F("released"));
  }

  Serial.print(F("  Tag "));
  if (tag_present) {
    if (tag_read) {
      if (tagMatches) {
        Serial.println(F("matching"));
      } else {
        Serial.println(F("NOT matching"));
      }
    } else {
      Serial.println(F("NOT read"));
    }
  } else {
    Serial.println(F("NOT present"));

  }
}
//**************************** CLI END ****************

//**************************** EEPROM START ***********

void writeConfig() {
  if (EEPROM.read(1) != 48) {
    EEPROM.write(1, 48);
  }

  if (EEPROM.read(2) != refData) {
    EEPROM.write(2, refData);
  }
  refDataSet = true;
}

//**************************** EEPROM DONE ***********

//**************************** SETUP START ***********

void setup() {

  pinMode(OUTPUT_NODE, OUTPUT);
  digitalWrite(OUTPUT_NODE, LED_OFF); //Node is not ready

  pinMode(OUTPUT_TAG, OUTPUT);
  digitalWrite(OUTPUT_TAG, LED_OFF); //Node is not ready

  pinMode(INPUT_BTN, INPUT_PULLUP);

  Serial.begin(9600); // Initialize serial communications with the PC
  while (!Serial);    // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)

  cmdInit(&Serial);
  cmdAdd(CMD_HELP, help);
  cmdAdd(CMD_CLEAR, clearEEPROM);
  cmdAdd(CMD_WRITE, writeToTag);
  cmdAdd(CMD_READ, readEEPROM);
  cmdAdd(CMD_STORE, writeEEPROM);
  cmdAdd(CMD_CANCEL, cancel);
  cmdAdd(CMD_STATUS, status);

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

}


//**************************** SETUP DONE ************

//**************************** BUTTON START **********

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

//**************************** BUTTON END ************

//**************************** TAG START *************

bool readTag(byte &data) {
  byte buffer[18]; //We need to read 4 pages at once with crc
  byte byteCount = sizeof(buffer);
  MFRC522::StatusCode result = mfrc522.MIFARE_Read(6,  buffer, &byteCount); //Address 6 is first page for user data

  if (result == MFRC522::STATUS_OK ) {
    data = buffer[0];
    return true;
  }

  return false;
}

bool writeTag(byte data) {
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

bool tagPresent() {

  if (tag_present & (millis() - tag_present_time > 500)) {
    Serial.println(F("Tag removed"));
    tag_present = false;
    tag_read = false;
    tagMatches = false;
  }

  // Look for new cards
  if ( !mfrc522.PICC_IsNewCardPresent()) {
    return false;
  }

  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    return false;
  }

  if (!tag_present) {
    Serial.println(F("New Tag present"));
  }
  tag_present = true;
  tag_present_time = millis();

  MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);

  if (piccType != MFRC522::PICC_Type::PICC_TYPE_MIFARE_UL) {
    Serial.println(F("Tag failure. Only working with MIFARE Ultralight!"));
    return false;
  }

  return true;
}


void controlTag() {
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
        } else {

          if (!tag_read) {
            Serial.print(F("Tag data: "));
            Serial.println((char) data);
          }
          tag_read = true;

          if (refDataSet) {
            boolean match = (data == refData);

            if (tagMatches != match) {
              if (match) {
                Serial.println(F("Found matching tag"));
              } else {
                Serial.println(F("Matching tag removed"));
              }

            }
            tagMatches = match;
            if (match) {
              tagMatchesTime = millis();
            }
          }
        }
      }
    }
  }
}

//**************************** TAG DONE **************

//**************************** LED START *************

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

//**************************** LED START *************

void loop() {

  cmdPoll();

  checkButton();

  controlLED();

  controlTag();

}

