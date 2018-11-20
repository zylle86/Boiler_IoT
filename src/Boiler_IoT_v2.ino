#include "application.h"
#include "HttpClient.h"

// Log all events to serial output
#ifndef LOGGING
#define LOGGING
#endif

// Run locally without publishing
#ifndef LOCAL
#define LOCAL
#endif

// This function enables the use of retained variables which maintain their data in DEEP_SLEEP
STARTUP(System.enableFeature(FEATURE_RETAINED_MEMORY));

/*
* Declaring retained variables
*/

// Critical error flag
retained int critical_Err = 0;
retained int day = 0;

/**
* Declaring user defined variables
*/

// Set Server IP - Format is (x,x,x,x)
IPAddress Server_IP(0,0,0,0);

// Set server port
int port = 0;

// Set server path
String path = "/put_path_here.txt";

// Set Login information - authorization type and credentials

// For "Basic" access user and password is base 64 encoded in the format "user:password"
// Use https://www.base64encode.org/ to encode credentials
String type = "Basic";
String credentials = "dGVzdDp0ZXN0";

String authentication = type + " " + credentials;

// Set sleep time, in seconds, in between error checking and publishing.
int sleepTime = 30;

//Set time to live for publish function *** Currently not in use ***
int ttl = 60;

// Publish event name
String eventRoutine = "Boiler Error!";
String eventCritical = "Critical Boiler Error!";

// Set max number of publish attempts to be made before putting to sleep
int attempts = 10;

// Set the wait time, in miliseconds, before attempting to re-connect to cloud.
int waitTime = 10000;

/*Headers currently need to be set at init, useful for API keys etc.
* HttpClient library does not support HTTP 1.1. However Servers not supporting
* HTTP 1.0, require "Host" header field in request headers.
*/
HttpClient http;

http_request_t request;
http_response_t response;
http_header_t headers[] = {
  // **** Host IP field needs to be typed manually!!! ****
  { "Host", "0.0.0.0"},
  { "Authorization" , authentication},
  { "Accept" , "*/*"},
  { NULL, NULL } // NOTE: Always terminate headers will NULL
};

// Set pin for DEEP_SLEEP override
int sleep_Override = D7;

void setup() {

  pinMode(sleep_Override, INPUT_PULLUP);

  #ifdef LOGGING
  Serial.begin(9600);
  #endif

// Check if critical error timer has expired
  int crit_err_timer;
  crit_err_timer = Time.weekday();
  if (crit_err_timer != day) {
    // If timer is expired reset critical error flag
    critical_Err = 0;
  }
}

void loop() {

// Login to server and get response.

  // Set request IP, port, path and body.
  request.ip = Server_IP;
  request.port = port;
  request.path = path;

  #ifdef LOGGING
  Serial.println();
  Serial.println("Application>\tStart of Loop.");
  // Get request
  http.get(request, response, headers);
  Serial.print("Application>\tResponse status: ");
  Serial.println(response.status);
  Serial.print("Application>\tHTTP Response Body: ");
  Serial.println(response.body);
  #endif

// If a correct response is recieved, check response values for errors, publish error if any are found
  //See https://en.wikipedia.org/wiki/List_of_HTTP_status_codes for response status codes
  if (response.status > 0 && response.status < 400) {

    // Check response for error and assign error code to errorVAL
    int errVAL = errChk(response.body);

    // If error is detected parse error response for content and publish
    if(errVAL >= 0 ) {
      String errCode;
      errCode = errParse(errVAL);

      #ifdef LOGGING
      Serial.print("Error Response>\t Error detected! Status code is: ");
      Serial.println(errVAL);
      Serial.print("Error Response>\t Error type: ");
      Serial.println(errCode);
      #endif

      // Publish errors if LOCAL is not defined
      #ifndef LOCAL
      errPublish(errCode, errVAL);
      #endif
    }

  } else {
// If incorrect response from server is recieved try again after wait period
    #ifdef LOGGING
    Serial.println("Application>\t Error - Cannot connect to Server!");
    Serial.println("Application\t Ensure server parameters are correct & try again.");
    Serial.println("Application>\t System will now go to sleep!");
    #endif
  }

  /* Put Photon to sleep for a designated period of seconds
  * Deep sleep shuts down MCU & network functions.
  * On wake-up system is re-initialized, with all registers and SRAM cleared.
  *
  * To ensure comms with photon is possible for reflashing, D7 can be shorted to
  * ground to prevent DEEP_SLEEP mode.
  */
  int readVal = digitalRead(sleep_Override);
  #ifdef LOGGING
  Serial.print("Application>\t Override Pin status: ");
  Serial.println(readVal);
  #endif

  // If override pin is not shorted, put system to sleep
  if (readVal == HIGH) {
    #ifdef LOGGING
    Serial.print("Application>\t System shutting down.");
    #endif
    delay(5000);
    System.sleep(SLEEP_MODE_DEEP,sleepTime);
  }

  // If override pins are shorted do not put to sleep but run loop until jumper is removed
  else {
    #ifdef LOGGING
    Serial.println("Application>\t Shutdown bypass detected, loop will run again.");
    #endif
    delay(30000);
  }


}
//End of Loop.



// Function declaration for error checking - returns error code number or -1 if no errors
int errChk(String str){

  String err = "0";
  int index;
  int step = 16;
  int errNum = 4;

  // Error checking is done in reverse order so most critical errors are checked first

  //Find index of highest priority error
  index = str.indexOf("<err4>");
  index += 6;

  //Check if index is correct
  #ifdef LOGGING
  Serial.print("Error Checking>\t Index is: ");
  Serial.println(index);
  Serial.print("Error Checking>\t Character at index is: ");
  Serial.println(str.charAt(index));
  #endif

  /* Check for error: 1 = error, 0 = no error
  *  if no error found, iterate to next error and run loop again for all errors
  */
  do {err = str.charAt(index);

    #ifdef LOGGING
    Serial.print("Error Checking>\t Checking for error nuber: ");
    Serial.println(errNum);
    Serial.print("Error Checking>\t Status is: ");
    Serial.print(str.charAt(index));
    Serial.print(". Found at index: ");
    Serial.println(index);
    #endif

    //If error is found, break while loop and return with error number
    if (err == "1") {

      #ifdef LOGGING
      Serial.print("Error Checking>\t Error detected!! Error number is: ");
      Serial.println(errNum);
      Serial.print("Error Checking>\t Status is: ");
      Serial.print(str.charAt(index));
      Serial.print(". Found at index: ");
      Serial.println(index);
      #endif

      return errNum;
    }

    errNum--;
    index -= step;
  } while(errNum >= 0 );

  return -1;
}


// Function declaration for error parsing - returns parsed error string.

String errParse(int errorCode) {

  String result;

  // Declare error string codes
  String error0 = "Ignition Failure";
  String error1 = "Out of pellets";
  String error2 = "Overtemperature detected!";
  String error3 = "Overcurrent detected!";
  String error4 = "Backburn to buffertank detected!";

  //Switch statement to pair error codes with strings
  switch (errorCode) {
    case 0:
    result = error0;
    break;

    case 1:
    result = error1;
    break;

    case 2:
    result = error2;
    break;

    case 3:
    result = error3;
    break;

    case 4:
    result = error4;
    break;
  }

  return result;

}


// Function declaration for publishing errors to cloud.

void errPublish(String str, int value) {

  bool success;
  String eventName;


  /*Check if error is critical to determine publishing criteria:
  * If a critical error occured, check error flag is not set.
  * If not set, publish critical error - set error flag, and timer
  * If set, publish rutine error to prevent recurring critical error actions.
  */
  if (value > 1 && critical_Err == 0) {
    eventName = eventCritical;
    critical_Err = 1;
    day = Time.weekday();
  } else {
    eventName = eventRoutine;
  }

  // Check if Photon is connected to cloud to prevent blocking behavior

  if (Particle.connected()) {
    // If Photon is connected publish event, check for succesful publish
    // and ACK of cloud reciept
retry:
      do {success = Particle.publish(eventName, str, ttl, PRIVATE, WITH_ACK);

      #ifdef LOGGING
      if (success) {
        Serial.print("Application>\t Publish event occured: ");
        Serial.println(eventName);
        Serial.print("Application>\t Publish content is: ");
        Serial.println(str);
        break;
      } else
      // If publish is unsuccessfull try again, for max 10 times
      {
        Serial.print("Application>\t Publish event failed. ");
        Serial.println("A new attempt will be made.");
        Serial.print("Application>\t Attempts remaining: ");
        Serial.println(attempts);
      }
      #endif

      attempts--;
    } while(success != true | (attempts == 0));
  } else {
    //If Photon is not connected to cloud set wait period and try connecting
    // If unsuccessfull at connecting after wait time, go to sleep
    #ifdef LOGGING
    Serial.print("Application>\t Photon not connected to cloud! ");
    Serial.println("Standby for re-connect attempt");
    #endif

    Particle.connect();
    delay(waitTime);
    if (Particle.connected() == true) {
      goto retry;
    } else {

      #ifdef LOGGING
      Serial.print("Application>\t Photon unable to connect to cloud. ");
      Serial.println("System going to sleep");
      #endif
    }
  }
  return;
}
