/*
 * Project : ESP32 Smart Home Automation System
 * Developer : Dhairya Parikh
 * Details: This code is written for the ESP32 Home automation PCB created by Techiesms YouTube channel. 
 * All the code is written for the 2nd Article of Circuit Cellar Magazine.
 * 
 * Features of this code:
 * 
 * 1. Capability of creating a BLE Server and 2 new characteristics within a service.
 * 2. Controlling 2 Relays based on the values in the characteristics.
 * 
 * Please contact me at dhairyaparikh1998@gmail.com if you require assitance with this code.
 */

// Importing the necessary Libraries
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// ----------------- Constants, Variables and Objects Declarations ---------------------

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID        "99dcc956-e6e1-4fd8-800b-8d3ea14b308a"
#define SwITCH1_CHARACTERISTIC_UUID "5ed88eac-d876-4e57-85c0-1391af164cbe"
#define SwITCH2_CHARACTERISTIC_UUID "d6b3f3d6-1ddb-4bcd-a50a-d5550d8d542b"

#define SWITCH1 32
#define SWITCH2 35
#define LED1 26
#define LED2 25

// In case you want to use these in the future
//#define SWITCH3 34
//#define SWITCH4 39
//#define LED3 27
//#define Buzzer 21

// For manual feedback
//#define R1 15
//#define R2 2
//#define R3 4
//#define R4 22


// Oject Declarations
BLEServer *pServer;
BLEService *pService;
BLECharacteristic *pCharacteristic1;
BLECharacteristic *pCharacteristic2;

// Variable Declarations
String temp1 = "";
String temp2 = "";

// ------------------------------------------------------------------------------------


// Arduino Setup Function
void setup()
{
  // Starting a Serial Connection
  Serial.begin(115200);
  Serial.println("Starting BLE Server!");

  // ---------------- Setting up pin configuration for both switches and LED pins of the PCB------
  pinMode(SWITCH1, OUTPUT);
  pinMode(SWITCH2, OUTPUT);
  digitalWrite(SWITCH1, LOW);
  digitalWrite(SWITCH2, LOW);

  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  // ---------------------------------------------------------------------------------------------
  
  // ------------------ BLE Service and Characteristics Initialization ----------------------------
  
  BLEDevice::init("ESP32-BLE-Server");
  pServer = BLEDevice::createServer();
  pService = pServer->createService(SERVICE_UUID);

  // Characteristic 1 initialization for 1st switch
  pCharacteristic1 = pService->createCharacteristic(
                                         SwITCH1_CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  pCharacteristic1->setValue("");

  // Characteristic 2 initialization for 2nd switch
  pCharacteristic2 = pService->createCharacteristic(
                                         SwITCH2_CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  pCharacteristic2->setValue("");

  // Start the BLE Service and setup some parameters --------------------------------------------
  
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  
  Serial.println("Characteristic defined! Now you can read it in the Client!");

  // -------------------------------------------------------------------------------------------
}


// Arduino Loop Fucntion
void loop()
{
  // fetch the latest value in both characteristics
  std::string switch1_value = pCharacteristic1->getValue();
  std::string switch2_value = pCharacteristic2->getValue();

  // Change the state of the relays and leds according to the value 
  changeRelay1State(switch1_value.c_str());
  changeRelay2State(switch2_value.c_str());
}  



void changeRelay1State(String state) {

    // Writing this logic to avoid continuous prints in the serial monitor.
    if (temp1 != state)
    {
      // If the value in the charateristic is 1, turn on the Relay1 and LED1
      if(state[0] == '1')
      {
        Serial.println("Turning ON Switch 1");       
        digitalWrite(SWITCH1, HIGH);
        digitalWrite(LED1, HIGH);
      }
      // If the value in the charateristic is 0, turn off the Relay1 and LED1
      else if(state[0] == '0')
      {
        Serial.println("Turning OFF Switch 1");
        digitalWrite(SWITCH1, LOW);
        digitalWrite(LED1, LOW);
      }
      else {
         // Do Nothing
        }
      temp1 = state;
    } 
    else {}    
}

void changeRelay2State(String state) {
    
    // Writing this logic to avoid continuous prints in the serial monitor.
    if (temp2 != state)
    {
      // If the value in the charateristic is 1, turn on the Relay2 and LED2
      if(state[0] == '1')
      {
        Serial.println("Turning ON Switch 2");       
        digitalWrite(SWITCH2, HIGH);
        digitalWrite(LED2, HIGH);
      }
      // If the value in the charateristic is 0, turn off the Relay2 and LED2
      else if(state[0] == '0')
      {
        Serial.println("Turning OFF Switch 2");
        digitalWrite(SWITCH2, LOW);
        digitalWrite(LED2, LOW);
      }
      else {
         // Do Nothing
        }
      temp2 = state;
    } 
    else {}    
}
