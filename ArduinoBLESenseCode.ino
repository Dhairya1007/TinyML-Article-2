// If your target is limited in memory remove this macro to save 10K RAM
#define EIDSP_QUANTIZE_FILTERBANK   0

/**
 * Define the number of slices per model window. E.g. a model window of 1000 ms
 * with slices per model window set to 4. Results in a slice size of 250 ms.
 * For more info: https://docs.edgeimpulse.com/docs/continuous-audio-sampling
 */
// Choose 1 slice per model window, hence giving use a 1000 ms or 1 second slice
#define EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW 1


/*
 ** NOTE: If you run into TFLite arena allocation issue.
 **
 ** This may be due to may dynamic memory fragmentation.
 ** Try defining "-DEI_CLASSIFIER_ALLOCATION_STATIC" in boards.local.txt (create
 ** if it doesn't exist) and copy this file to
 ** `<ARDUINO_CORE_INSTALL_PATH>/arduino/hardware/<mbed_core>/<core_version>/`.
 **
 ** See
 ** (https://support.arduino.cc/hc/en-us/articles/360012076960-Where-are-the-installed-cores-located-)
 ** to find where Arduino installs cores on your machine.
 **
 ** If the problem persists then there's not enough memory for this model and application.
 */

/* Includes ---------------------------------------------------------------- */
#include <PDM.h>
#include <Circuit-Cellar-Article-2_inferencing.h>

// *************************** Code Addition ******************************

// Adding the Arduino BLE Library
#include <ArduinoBLE.h>
// Libraries for OLD control
#include <Wire.h>
#include <SFE_MicroOLED.h>  //Click here to get the library: http://librarymanager/All#SparkFun_Micro_OLED

#define DC_JUMPER 1
#define PIN_RESET 9

// ************************************************************************

/** Audio buffers, pointers and selectors */
typedef struct {
    signed short *buffers[2];
    unsigned char buf_select;
    unsigned char buf_ready;
    unsigned int buf_count;
    unsigned int n_samples;
} inference_t;

// *************************** Code Addition ******************************

// BLE constant declarations (Use these same uuids in the ESP32 Code)
const char* ESP32uuid = "99dcc956-e6e1-4fd8-800b-8d3ea14b308a";
const char* Switch1CharacteristicUuid = "5ed88eac-d876-4e57-85c0-1391af164cbe";
const char* Switch2CharacteristicUuid = "d6b3f3d6-1ddb-4bcd-a50a-d5550d8d542b";
String switch1message = "";
String switch2message = "";

MicroOLED oled(PIN_RESET, DC_JUMPER); //Example I2C declaration

int SCREEN_WIDTH = oled.getLCDWidth();
int SCREEN_HEIGHT = oled.getLCDHeight();
int flag1 = 0;
int flag2 = 0;
int choice = 0;

// ************************************************************************

static inference_t inference;
static bool record_ready = false;
static signed short *sampleBuffer;
static bool debug_nn = false; // Set this to true to see e.g. features generated from the raw signal
static int print_results = -(EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW);

/**
 * @brief      Arduino setup function
 */
void setup()
{
    // put your setup code here, to run once:
    Serial.begin(115200);
    delay(100);
    Wire.begin();

    // OLED Online
    oled.begin();
    oled.clear(ALL);
    oled.display();
    oled.setFontType(0);
    oled.setCursor(0,0);
    oled.print("Circuit\nCellar");
    oled.display();
    delay(2000);
    oled.clear(PAGE);
    Serial.println(" OLED Online ");
    
    // *************************** Code Addition ******************************
    
    // Starting the BLE Service
    if (!BLE.begin()) {
    Serial.println("* Starting Bluetooth® Low Energy module failed!");
    while (1);
    }
  
    BLE.setLocalName("Nano 33 BLE (Central)"); 
    BLE.advertise();

    // ************************************************************************
    
    Serial.println("Edge Impulse Inferencing Demo");

    // summary of inferencing settings (from model_metadata.h)
    ei_printf("Inferencing settings:\n");
    ei_printf("\tInterval: %.2f ms.\n", (float)EI_CLASSIFIER_INTERVAL_MS);
    ei_printf("\tFrame size: %d\n", EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
    ei_printf("\tSample length: %d ms.\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT / 16);
    ei_printf("\tNo. of classes: %d\n", sizeof(ei_classifier_inferencing_categories) /
                                            sizeof(ei_classifier_inferencing_categories[0]));

    run_classifier_init();
    if (microphone_inference_start(EI_CLASSIFIER_SLICE_SIZE) == false) {
        ei_printf("ERR: Failed to setup audio sampling\r\n");
        return;
    }
}

/**
 * @brief      Arduino main function. Runs the inferencing loop.
 */
void loop()
{

    // *************************** Code Addition ******************************
    
    // BLE code for this project
    // define an object for BLE Device
    BLEDevice peripheral;

    Serial.println("- Discovering peripheral device...");
    
    BLE.scanForUuid(ESP32uuid);
    peripheral = BLE.available();
    
    if (peripheral) {
      Serial.println("* Peripheral device found!");
      Serial.print("* Device MAC address: ");
      Serial.println(peripheral.address());
      Serial.print("* Device name: ");
      Serial.println(peripheral.localName());
      Serial.print("* Advertised service UUID: ");
      Serial.println(peripheral.advertisedServiceUuid());
      Serial.println(" ");
      BLE.stopScan();      
    }

    // Define the Characteristics of each switch to which we would be writing the values
    BLECharacteristic Switch1Characteristics = peripheral.characteristic(Switch1CharacteristicUuid);
    BLECharacteristic Switch2Characteristics = peripheral.characteristic(Switch2CharacteristicUuid);
  
    // ************************************************************************
    
    bool m = microphone_inference_record();
    if (!m) {
        ei_printf("ERR: Failed to record audio...\n");
        return;
    }

    signal_t signal;
    signal.total_length = EI_CLASSIFIER_SLICE_SIZE;
    signal.get_data = &microphone_audio_signal_get_data;
    ei_impulse_result_t result = {0};

    EI_IMPULSE_ERROR r = run_classifier_continuous(&signal, &result, debug_nn);
    if (r != EI_IMPULSE_OK) {
        ei_printf("ERR: Failed to run classifier (%d)\n", r);
        return;
    }

    if (++print_results >= (EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW)) {
        // print the predictions
        ei_printf("Predictions ");
        ei_printf("(DSP: %d ms., Classification: %d ms., Anomaly: %d ms.)",
            result.timing.dsp, result.timing.classification, result.timing.anomaly);
        ei_printf(": \n");
        for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
            ei_printf("    %s: %.5f\n", result.classification[ix].label,
                      result.classification[ix].value);

        // *************************** Code Addition ******************************

        // Main project logic
        if (result.classification[ix].label == "lights on" && result.classification[ix].value > 0.6)   // Keeping the confidence value at 60%
        {
          if(peripheral.connected()) {
            
            Serial.println("Turning on the Lights!");
            char* msg = "1";
            Switch1Characteristics.writeValue(msg);
            flag1 = 1;
          }
        }
        else if (result.classification[ix].label == "lights off" && result.classification[ix].value > 0.6)   // Keeping the confidence value at 60%
        {
          if(peripheral.connected()) {
            
            Serial.println("Turning off the Lignts!");
            char* msg = "0";
            Switch1Characteristics.writeValue(msg);
            flag1 = 0;
          }
        }
        else if (result.classification[ix].label == "fan on" && result.classification[ix].value > 0.6)   // Keeping the confidence value at 60%
        {
          if(peripheral.connected()) {
            
            Serial.println("Turning on the Fan!");
            char* msg = "1";
            Switch2Characteristics.writeValue(msg);
            flag2 = 1;
          }
        }
        else if (result.classification[ix].label == "fan off" && result.classification[ix].value > 0.6)   // Keeping the confidence value at 60%
        {
          if(peripheral.connected()) {
            //Convert String to Char array
            Serial.println("Turning off the Fan!");
            char* msg = "0";
            Switch2Characteristics.writeValue(msg);
            flag2 = 0;
          }
        }
        else 
        {
          // Do Nothing in case the label output is noise or unknown 
        }
        // Get the value of choice for OLED Display
        if((flag1 == 0) && (flag2 == 0))
        {
          choice = 1;
        }
        else if((flag1 == 1) && (flag2 == 0))
        {
          choice = 2;
        }
        else if((flag1 == 0) && (flag2 == 1))
        {
          choice = 3;
        }
        else if((flag1 == 1) && (flag2 == 1))
        {
          choice = 4;
        }
        printoled(choice);
        
   // *********************************************************************
    }
#if EI_CLASSIFIER_HAS_ANOMALY == 1
        ei_printf("    anomaly score: %.3f\n", result.anomaly);
#endif

        print_results = 0;
    }
}

//*************************** Code Addition ******************************
// Defining a function that will change the OLED with Switch states

void printoled(int val)
{
  switch(val)
  {  
      case 1:   
        delay(200);
        oled.setFontType(0);
        oled.setCursor(0,0);
        oled.clear(PAGE);
        oled.print("Switch 1: ");
        oled.print(" OFF");
        oled.print("\nSwitch 2: ");
        oled.print(" OFF");
        oled.display();
        break;

      case 2:   
        delay(200);
        oled.setFontType(0);
        oled.setCursor(0,0);
        oled.clear(PAGE);
        oled.print("Switch 1: ");
        oled.print(" ON");
        oled.print("\nSwitch 2: ");
        oled.print(" OFF");
        oled.display();
        break;

      case 3:   
        delay(200);
        oled.setFontType(0);
        oled.setCursor(0,0);
        oled.clear(PAGE);
        oled.print("Switch 1: ");
        oled.print(" OFF");
        oled.print("\nSwitch 2: ");
        oled.print(" ON");
        oled.display();
        break;

     case 4:    
        delay(200);
        oled.setFontType(0);
        oled.setCursor(0,0);
        oled.clear(PAGE);
        oled.print("Switch 1: ");
        oled.print(" ON");
        oled.print("\nSwitch 2: ");
        oled.print(" ON");
        oled.display();
        break;

     default: 
        delay(200);
        oled.setFontType(0);
        oled.setCursor(0,0);  
        oled.clear(PAGE);
        oled.print("Circuit\nCellar");
        oled.display();
    }
}

// *********************************************************************      
/**
 * @brief      PDM buffer full callback
 *             Get data and call audio thread callback
 */
static void pdm_data_ready_inference_callback(void)
{
    int bytesAvailable = PDM.available();

    // read into the sample buffer
    int bytesRead = PDM.read((char *)&sampleBuffer[0], bytesAvailable);

    if (record_ready == true) {
        for (int i = 0; i<bytesRead>> 1; i++) {
            inference.buffers[inference.buf_select][inference.buf_count++] = sampleBuffer[i];

            if (inference.buf_count >= inference.n_samples) {
                inference.buf_select ^= 1;
                inference.buf_count = 0;
                inference.buf_ready = 1;
            }
        }
    }
}

/**
 * @brief      Init inferencing struct and setup/start PDM
 *
 * @param[in]  n_samples  The n samples
 *
 * @return     { description_of_the_return_value }
 */
static bool microphone_inference_start(uint32_t n_samples)
{
    inference.buffers[0] = (signed short *)malloc(n_samples * sizeof(signed short));

    if (inference.buffers[0] == NULL) {
        return false;
    }

    inference.buffers[1] = (signed short *)malloc(n_samples * sizeof(signed short));

    if (inference.buffers[1] == NULL) {
        free(inference.buffers[0]);
        return false;
    }

    sampleBuffer = (signed short *)malloc((n_samples >> 1) * sizeof(signed short));

    if (sampleBuffer == NULL) {
        free(inference.buffers[0]);
        free(inference.buffers[1]);
        return false;
    }

    inference.buf_select = 0;
    inference.buf_count = 0;
    inference.n_samples = n_samples;
    inference.buf_ready = 0;

    // configure the data receive callback
    PDM.onReceive(&pdm_data_ready_inference_callback);

    PDM.setBufferSize((n_samples >> 1) * sizeof(int16_t));

    // initialize PDM with:
    // - one channel (mono mode)
    // - a 16 kHz sample rate
    if (!PDM.begin(1, EI_CLASSIFIER_FREQUENCY)) {
        ei_printf("Failed to start PDM!");
    }

    // set the gain, defaults to 20
    PDM.setGain(127);

    record_ready = true;

    return true;
}

/**
 * @brief      Wait on new data
 *
 * @return     True when finished
 */
static bool microphone_inference_record(void)
{
    bool ret = true;

    if (inference.buf_ready == 1) {
        ei_printf(
            "Error sample buffer overrun. Decrease the number of slices per model window "
            "(EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW)\n");
        ret = false;
    }

    while (inference.buf_ready == 0) {
        delay(1);
    }

    inference.buf_ready = 0;

    return ret;
}

/**
 * Get raw audio signal data
 */
static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr)
{
    numpy::int16_to_float(&inference.buffers[inference.buf_select ^ 1][offset], out_ptr, length);

    return 0;
}

/**
 * @brief      Stop PDM and release buffers
 */
static void microphone_inference_end(void)
{
    PDM.end();
    free(inference.buffers[0]);
    free(inference.buffers[1]);
    free(sampleBuffer);
}

#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_MICROPHONE
#error "Invalid model for current sensor."
#endif
