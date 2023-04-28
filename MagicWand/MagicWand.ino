#include <ArduinoBLE.h>

#include <Arduino_LSM9DS1.h>
 
#include <TensorFlowLite.h>
#include <tensorflow/lite/micro/kernels/all_ops_resolver.h>
#include <tensorflow/lite/micro/micro_error_reporter.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/schema/schema_generated.h>
#include <tensorflow/lite/version.h>

#include "model.h" //tensorflow data

#define MOTION_THRESHOLD 0.1
// #define CAPTURE_DELAY 200 // This is now in milliseconds
#define NUM_SAMPLES 10

// BLEByteCharacteristic* gestureCharacteristic = nullptr;
// BLEService* gestureService = nullptr;
#define LOCAL_NAME "AI Powered Magic Wand"

const char* SERVICE_UUID = "19b10000-e8f2-537e-4f6c-d104768a1214";

const char* GESTURE_UUID = "19b10001-e8f2-537e-4f6c-d104768a1214";
// const char* CONNECTED_UUID = "19b10002-e8f2-537e-4f6c-d104768a1214";

BLEFloatCharacteristic* gestureCharacteristic = nullptr;
// BLEBoolCharacteristic* isConnectedCharacteristic = nullptr;
BLEService* gestureService = nullptr;

// long previousMillis = 0;

// const float accelerationThreshold = 4; // threshold of significant in G's
// const float accelerationThreshold = 0.075; // threshold of significant in G's

// const int numSamples = 10;

// int samplesRead = numSamples;

// global variables used for TensorFlow Lite (Micro)
tflite::MicroErrorReporter tflErrorReporter;

// pull in all the TFLM ops, you can remove this line and
// only pull in the TFLM ops you need, if would like to reduce
// the compiled size of the sketch.
tflite::ops::micro::AllOpsResolver tflOpsResolver;

const tflite::Model* tflModel = nullptr;
tflite::MicroInterpreter* tflInterpreter = nullptr;
TfLiteTensor* tflInputTensor = nullptr;
TfLiteTensor* tflOutputTensor = nullptr;

// Create a static memory buffer for TFLM, the size may need to
// be adjusted based on the model you are using
constexpr int tensorArenaSize = 8 * 1024;
byte tensorArena[tensorArenaSize];

// array to map gesture index to a name
const char *GESTURES[] = {
    "Up", "Down", "Left", "Right"
};

#define NUM_GESTURES (sizeof(GESTURES) / sizeof(GESTURES[0]))

// int a=0; // count

// void handleIsConnectedWritten(BLEDevice central, BLECharacteristic characteristic)
// {
//   // Serial.print("isConnectedCharacteristic value changed: ");
//   // Serial.println(isConnectedCharacteristic.value());
//   // Serial.println(isConnectedCharacteristic->value());
// }

void setup() {

//  gestureService = new BLEService("19b10000-e8f2-537e-4f6c-d104768a1214");
//  gestureCharacteristic = new BLEByteCharacteristic("19b10001-e8f2-537e-4f6c-d104768a1214", BLERead | BLEWrite | BLENotify);

  gestureService = new BLEService(SERVICE_UUID);
  gestureCharacteristic = new BLEFloatCharacteristic(GESTURE_UUID, BLERead | BLENotify | BLEWrite);
  // isConnectedCharacteristic = new BLEBoolCharacteristic(CONNECTED_UUID, BLERead | BLENotify | BLEWrite);


 Serial.begin(9600);
  if (!IMU.begin()) {
    Serial.println("LSM9DS1 failed!");
    while (1);
  }

  if (!BLE.begin()) {
    Serial.println("starting BLE failed!");
    while (1);
  }

  // set advertised local name and service UUID:
  BLE.setAdvertisedService(*gestureService); // add the service UUID
  gestureService->addCharacteristic(*gestureCharacteristic);
  // gestureService->addCharacteristic(*isConnectedCharacteristic);
  BLE.addService(*gestureService);
  // start advertising
  // BLE.setLocalName("TEST");
  static String deviceName = LOCAL_NAME;
  BLE.setLocalName(deviceName.c_str());
  BLE.setDeviceName(deviceName.c_str());
  // set the initial value for the characeristic:
  //gestureCharacteristic->writeValue(0);
  // start advertising
  BLE.advertise();
  Serial.println("BLE service started");
 
  // print out the samples rates of the IMUs
  Serial.print("Accelerometer sample rate = ");
  Serial.print(IMU.accelerationSampleRate());
  Serial.println(" Hz");
  Serial.print("Gyroscope sample rate = ");
  Serial.print(IMU.gyroscopeSampleRate());
  Serial.println(" Hz");
  Serial.println();

  // get the TFL representation of the model byte array
  tflModel = tflite::GetModel(model);
  if (tflModel->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println("Model schema mismatch!");
    while (1);
  }
  // Create an interpreter to run the model
  tflInterpreter = new tflite::MicroInterpreter(tflModel, tflOpsResolver, tensorArena, tensorArenaSize, &tflErrorReporter);
 
  // Allocate memory for the model's input and output tensors
  tflInterpreter->AllocateTensors();
 
  // Get pointers for the model's input and output tensors
  tflInputTensor = tflInterpreter->input(0);
  tflOutputTensor = tflInterpreter->output(0);

  Serial.println("Init done");
}

//String Level_String;
float aX, aY, aZ, gX, gY, gZ;

boolean active = false;
/*
void loop() {

  // listen for BLE peripherals to connect:
  BLEDevice central = BLE.central();
  // if a central is connected to peripheral:
  if (central)
  {
    // Serial.println("Bluetooth connected");
    while (central.connected())
    {
      // bluetooth device is connected
      
      // wait for significant motion
      if(active == false && samplesRead == numSamples) {
        if (IMU.accelerationAvailable()  && IMU.gyroscopeAvailable()) {
          // read the acceleration data
          IMU.readAcceleration(aX, aY, aZ);
          IMU.readGyroscope(gX, gY, gZ);
    
          // sum up the absolutes
          float aSum = fabs(aX) + fabs(aY) + fabs(aZ);
    
          // check if it's above the threshold
          if (aSum >= accelerationThreshold) {
            // reset the sample read count
            samplesRead = 0;
            active = true;
          }
        }
      }
      
      // check if the all the required samples have been read since
      // the last time the significant motion was detected
      if (active == true && samplesRead < numSamples) {
        // check if new acceleration AND gyroscope data is available
        if (IMU.accelerationAvailable() && IMU.gyroscopeAvailable()) {
          // read the acceleration and gyroscope data
          IMU.readAcceleration(aX, aY, aZ);
          IMU.readGyroscope(gX, gY, gZ);
    
          // normalize the IMU data between 0 to 1 and store in the model's
          // input tensor
          tflInputTensor->data.f[samplesRead * 6 + 0] = (aX + 4.0) / 8.0;
          tflInputTensor->data.f[samplesRead * 6 + 1] = (aY + 4.0) / 8.0;
          tflInputTensor->data.f[samplesRead * 6 + 2] = (aZ + 4.0) / 8.0;
          tflInputTensor->data.f[samplesRead * 6 + 3] = (gX + 2000.0) / 4000.0;
          tflInputTensor->data.f[samplesRead * 6 + 4] = (gY + 2000.0) / 4000.0;
          tflInputTensor->data.f[samplesRead * 6 + 5] = (gZ + 2000.0) / 4000.0;
    
          samplesRead++;
    
          if (samplesRead == numSamples) {
            active = false;
            // Run inferencing
            TfLiteStatus invokeStatus = tflInterpreter->Invoke();
            if (invokeStatus != kTfLiteOk) {
              Serial.println("Invoke failed!");
              while (1);
              return;
            }

            int maxIndex = 0;
            float maxValue = 0;


            for (int i = 0; i < NUM_GESTURES; i++) {
              float _value = tflOutputTensor->data.f[i];
              if(_value > maxValue){
                maxValue = _value;
                maxIndex = i;
              }
              Serial.print(GESTURES[i]);
              Serial.print(": ");
              Serial.println(tflOutputTensor->data.f[i], 4);
            }
            Serial.print("Winner: ");
            Serial.println(GESTURES[maxIndex]);
            Serial.println();

            //   // Counts if accuracy is 0.7 or higher
            //  if(tflOutputTensor->data.f[0] >= 0.2){
            //    // do something
            //    Serial.println("So far so good");
            //    if(central.connected()){
            //      // write to bluetooth
            //      gestureCharacteristic->writeValue(1);
            //    }
            //  }
          
          }
        }
      }     
    }
    Serial.println("Bluetooth disconnected");
  }
}
*/



bool isCapturing = false;
int numSamplesRead = 0;
// int NUM_SAMPLES = 10;

// float aX, aY, aZ, gX, gY, gZ;
/*
void loop() {
  
  // listen for BLE peripherals to connect:
  BLEDevice central = BLE.central();
  // if a central is connected to peripheral:
  if (central)
  {
    Serial.println("Bluetooth connected");
    while (central.connected())
    {
      
      // bluetooth device is connected
      
      // wait for significant motion

      while (!isCapturing) {
      if (IMU.accelerationAvailable() && IMU.gyroscopeAvailable()) {
        // Serial.println("both availablle");
      
        IMU.readAcceleration(aX, aY, aZ);
        IMU.readGyroscope(gX, gY, gZ);

        // Sum absolute values
        float average = fabs(aX / 4.0) + fabs(aY / 4.0) + fabs(aZ / 4.0) + fabs(gX / 2000.0) + fabs(gY / 2000.0) + fabs(gZ / 2000.0);
        average /= 6.;

        // Above the threshold?
        if (average >= 0.075) {
          isCapturing = true;
          numSamplesRead = 0;
          break;
        }
      }
      // Serial.println("Ending !isCapturing");
    }

    while (isCapturing) {
      // Serial.println("Starting isCapturing");

      // Check if both acceleration and gyroscope data is available
      if (IMU.accelerationAvailable() && IMU.gyroscopeAvailable()) {
        // Serial.println("both available again");

        // read the acceleration and gyroscope data
        IMU.readAcceleration(aX, aY, aZ);
        IMU.readGyroscope(gX, gY, gZ);

        // Normalize the IMU data between -1 to 1 and store in the model's
        // input tensor. Accelerometer data ranges between -4 and 4,
        // gyroscope data ranges between -2000 and 2000
        tflInputTensor->data.f[numSamplesRead * 6 + 0] = aX / 4.0;
        tflInputTensor->data.f[numSamplesRead * 6 + 1] = aY / 4.0;
        tflInputTensor->data.f[numSamplesRead * 6 + 2] = aZ / 4.0;
        tflInputTensor->data.f[numSamplesRead * 6 + 3] = gX / 2000.0;
        tflInputTensor->data.f[numSamplesRead * 6 + 4] = gY / 2000.0;
        tflInputTensor->data.f[numSamplesRead * 6 + 5] = gZ / 2000.0;

        numSamplesRead++;

        // Do we have the samples we need?
        if (numSamplesRead == NUM_SAMPLES) {
          
          // Stop capturing
          isCapturing = false;
          // Serial.println("stopping capture to predict");
          
          
          // Run inference
          Serial.println("Starting Invoke");
          TfLiteStatus invokeStatus = tflInterpreter->Invoke();
          Serial.println("Invoke Started");
          if (invokeStatus != kTfLiteOk) {
            Serial.println("Error: Invoke failed!");
            while (1);
            return;
          }
          Serial.println("Invoke successful");

          // Loop through the output tensor values from the model
          int maxIndex = 0;
          float maxValue = 0;
          
          for (int i = 0; i < NUM_GESTURES; i++) {
            float _value = tflOutputTensor->data.f[i];
            if(_value > maxValue){
              maxValue = _value;
              maxIndex = i;
            }
            Serial.print(GESTURES[i]);
            Serial.print(": ");
            Serial.println(tflOutputTensor->data.f[i], 6);
          }
          
          Serial.print("Winner: ");
          Serial.print(GESTURES[maxIndex]);
          
          if(GESTURES[maxIndex] == "Up") {
              // Serial.print("Winner is UP ");
              gestureCharacteristic->writeValue(0);
              break;
          }
          else if(GESTURES[maxIndex] == "Down") {
              // Serial.print("Winner is DOWN ");
              gestureCharacteristic->writeValue(1);
              break;
          }
          else if(GESTURES[maxIndex] == "Left") {
              // Serial.print("Winner is LEFT ");
              gestureCharacteristic->writeValue(2);
              break;
          }
          else if(GESTURES[maxIndex] == "Right") {
              // Serial.print("Winner is RIGHT ");
              gestureCharacteristic->writeValue(3);
              break;
          }
          Serial.println();
          
          
          //gestureCharacteristic.writeValue(GESTURES[maxIndex]);
          
          // Serial.println();

          // Add delay to not double trigger
          //delay(CAPTURE_DELAY);
          //delay(5000);
          // TODO: Add manual timeout via counter
        }
      }
      // Serial.println("Ending isCapturing");
    }
    }
    Serial.println("Bluetooth disconnected");
  }
}
*/



void loop() {
  
  // listen for BLE peripherals to connect:
  BLEDevice central = BLE.central();
  // if a central is connected to peripheral:
  if (central)
  {
    Serial.println("Bluetooth connected");
    while (central.connected())
    {
      
      // bluetooth device is connected
      
      // wait for significant motion

      // while (!isCapturing) {
      if(isCapturing == false) {
        if (IMU.accelerationAvailable() && IMU.gyroscopeAvailable()) {
          // Serial.println("both availablle");
        
          IMU.readAcceleration(aX, aY, aZ);
          IMU.readGyroscope(gX, gY, gZ);

          // Sum absolute values
          float average = fabs(aX / 4.0) + fabs(aY / 4.0) + fabs(aZ / 4.0) + fabs(gX / 2000.0) + fabs(gY / 2000.0) + fabs(gZ / 2000.0);
          average /= 6.;

          // Above the threshold?
          //if (average >= 0.075) {
          // if (average >= 0.1) {
          if (average >= MOTION_THRESHOLD) {
            isCapturing = true;
            numSamplesRead = 0;
            break;
          }
        }
        // Serial.println("Ending !isCapturing");
      }





    // while (isCapturing) {
    if(isCapturing == true && numSamplesRead < NUM_SAMPLES) {
      // Serial.println("Starting isCapturing");

      // Check if both acceleration and gyroscope data is available
      if (IMU.accelerationAvailable() && IMU.gyroscopeAvailable()) {
        // Serial.println("both available again");

        // read the acceleration and gyroscope data
        IMU.readAcceleration(aX, aY, aZ);
        IMU.readGyroscope(gX, gY, gZ);

        // Normalize the IMU data between -1 to 1 and store in the model's
        // input tensor. Accelerometer data ranges between -4 and 4,
        // gyroscope data ranges between -2000 and 2000
        tflInputTensor->data.f[numSamplesRead * 6 + 0] = aX / 4.0;
        tflInputTensor->data.f[numSamplesRead * 6 + 1] = aY / 4.0;
        tflInputTensor->data.f[numSamplesRead * 6 + 2] = aZ / 4.0;
        tflInputTensor->data.f[numSamplesRead * 6 + 3] = gX / 2000.0;
        tflInputTensor->data.f[numSamplesRead * 6 + 4] = gY / 2000.0;
        tflInputTensor->data.f[numSamplesRead * 6 + 5] = gZ / 2000.0;

        numSamplesRead++;

        // Do we have the samples we need?
        if (numSamplesRead == NUM_SAMPLES) {
          
          // Stop capturing
          isCapturing = false;
          // Serial.println("stopping capture to predict");
          
          
          // Run inference
          // Serial.println("Starting Invoke");
          TfLiteStatus invokeStatus = tflInterpreter->Invoke();
          // Serial.println("Invoke Started");
          if (invokeStatus != kTfLiteOk) {
            Serial.println("Error: Invoke failed!");
            while (1);
            return;
          }
          // Serial.println("Invoke successful");

          // Loop through the output tensor values from the model
          int maxIndex = 0;
          float maxValue = 0;
          
          for (int i = 0; i < NUM_GESTURES; i++) {
            float _value = tflOutputTensor->data.f[i];
            if(_value > maxValue){
              maxValue = _value;
              maxIndex = i;
            }
            Serial.print(GESTURES[i]);
            Serial.print(": ");
            Serial.println(tflOutputTensor->data.f[i], 6);
          }
          
          Serial.print("Winner: ");
          Serial.print(GESTURES[maxIndex]);
          
          if(GESTURES[maxIndex] == "Up") {
              // Serial.print("Winner is UP ");
              gestureCharacteristic->writeValue(0);
              break;
          }
          else if(GESTURES[maxIndex] == "Down") {
              // Serial.print("Winner is DOWN ");
              gestureCharacteristic->writeValue(1);
              break;
          }
          else if(GESTURES[maxIndex] == "Left") {
              // Serial.print("Winner is LEFT ");
              gestureCharacteristic->writeValue(2);
              break;
          }
          else if(GESTURES[maxIndex] == "Right") {
              // Serial.print("Winner is RIGHT ");
              gestureCharacteristic->writeValue(3);
              break;
          }
          Serial.println();
          
          
          //gestureCharacteristic.writeValue(GESTURES[maxIndex]);
          
          // Serial.println();

          // Add delay to not double trigger
          //delay(CAPTURE_DELAY);
          //delay(5000);
          // TODO: Add manual timeout via counter
        }
      }
      // Serial.println("Ending isCapturing");
    }
    }
    Serial.println("Bluetooth disconnected");
  }
}
