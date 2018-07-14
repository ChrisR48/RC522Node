/**
 * ----------------------------------------------------------------------------
 * This is a RC522Node; see https://github.com/ChrisR48/RC522Node
 * for further details
 *
 * NOTE: The node uses MFRC522 library from: https://github.com/miguelbalboa/rfid
 *
 * Typical pin layout used:
 * -----------------------------------------------------------------------------------------
 *             MFRC522      Arduino       Arduino   Arduino    Arduino          Arduino
 *             Reader/PCD   Uno/101       Mega      Nano v3    Leonardo/Micro   Pro Micro
 * Signal      Pin          Pin           Pin       Pin        Pin              Pin
 * -----------------------------------------------------------------------------------------
 * RST/Reset   RST          9             5         D9         RESET/ICSP-5     RST
 * SPI SS      SDA(SS)      10            53        D10        10               10
 * SPI MOSI    MOSI         11 / ICSP-4   51        D11        ICSP-4           16
 * SPI MISO    MISO         12 / ICSP-1   50        D12        ICSP-1           14
 * SPI SCK     SCK          13 / ICSP-3   52        D13        ICSP-3           15
 *
 */


#include <SPI.h>
#include <MFRC522.h>

#define LED_ON LOW
#define LED_OFF HIGH

#define BUTTON_ON HIGH
#define BUTTON_OFF LOW

const uint8_t OUTPUT_NODE = 2;
const uint8_t OUTPUT_TAG  = 3;
const uint8_t INPUT_BTN   = 4;

const uint8_t SS_PIN = 10;
const uint8_t RST_PIN = UINT8_MAX; //NOT USED

MFRC522 mfrc522(SS_PIN, RST_PIN);    // Create MFRC522 instance.

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

    while(!mfrc522.PCD_PerformSelfTest()){
      Serial.println(F("MFRC522 Selftest failed"));
      delay(1000);
    }
    Serial.println(F("MFRC522 Selftest passed"));
    mfrc522.PCD_DumpVersionToSerial();

    Serial.print(F("MFRC522 Soft_RST() "));
    //mfrc522.PCD_Reset();
    Serial.println(F("done"));
    
    digitalWrite(OUTPUT_NODE, LED_ON); //Node is ready
    
    Serial.println(F("Startup completed"));
}

void loop() {

    digitalWrite(OUTPUT_TAG, LED_OFF);

    if( !digitalRead(INPUT_BTN) == BUTTON_ON){
      Serial.println("Button pressed");
      digitalWrite(OUTPUT_TAG, LED_ON);
    }

    // Look for new cards
    if ( !mfrc522.PICC_IsNewCardPresent()){
        return;
    }
    
    Serial.println(F("MFRC522 Card Present"));

    // Select one of the cards
    if ( ! mfrc522.PICC_ReadCardSerial())
        return;

    Serial.println(F("MFRC522 Read Serial Successful"));
}


