#include "Config.h"

/**************** Global variable declare *****************/
WiFiManager wifiManager;
WiFiClient net;
MQTTClient client;
volatile int8_t currentState = NORMAL;
IRrecv irrecv(RECEIVE_PIN);
IRsend irsend(SEND_PIN);
decode_results results;

String ir = "";
String irString = "";

String codeString[] = {"", "", "", ""};
int indexString = 0;
String hourString[] = {"", "", "", ""};
String timeTemp = "";

Ticker ticker;

int codeType = -1; // The type of code
unsigned long codeValue; // The code value if not raw
int codeLen; // The length of the code
int toggle = 0; // The RC5/6 toggle state
unsigned long timeToWait = 0;

unsigned long previous = 0;

bool wifi_status = false;

void tick() {
  digitalWrite(WIFI_STATUS_PIN, wifi_status = !wifi_status);
}

void configModeCallback (WiFiManager *myWiFiManager) {
  #ifdef DEBUG
    Serial.println("Entered config mode");
    Serial.println(WiFi.softAPIP());
    //if you used auto generated SSID, print it
    Serial.println(myWiFiManager->getConfigPortalSSID());
    //entered config mode, make led toggle faster
  #endif
  ticker.attach_ms(50, tick);
}

void setup() {
  // set output to status pin
  pinMode(STATUS_PIN, OUTPUT);
  pinMode(WIFI_STATUS_PIN, OUTPUT);
  #ifdef DEBUG
    Serial.begin(115200);
  #endif

  ticker.attach_ms(600, tick);

  wifiManager.setAPCallback(configModeCallback);

  if (!wifiManager.autoConnect("Config Wifi")) {
    #ifdef DEBUG
      Serial.println("failed to connect and hit timeout");
    #endif
    //reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(1000);
  }
  #ifdef DEBUG
    Serial.println("Wifi connected...");
  #endif
  ticker.detach();
  //keep LED on
  digitalWrite(WIFI_STATUS_PIN, 0);
  irrecv.enableIRIn();
  client.begin("iot.eclipse.org", net);

  connect();
}

void loop() {
  client.loop();
  delay(10); // <- fixes some issues with WiFi stability

  if(!client.connected()) {
    connect();
  }
  
  switch(currentState) {
    case NORMAL:
      if (millis() - previous > 5000) {
        previous = millis();
        timeTemp = getTime();
        #ifdef DEBUG
          Serial.println(timeTemp);
        #endif
        for (int8_t i = 0; i < indexString; i++) {
          if (hourString[i].equals(timeTemp)) {
            #ifdef DEBUG
              Serial.println("index action is: " + String(i));
              Serial.println("String action is: " + codeString[i]);
            #endif
          }
        }
      }
      
      break;
    case RECEIVE:
      if (irrecv.decode(&results)) {
        storeCode(&results);
        irrecv.resume(); // Receive the next value
        sendCodeToBroker();

        // flash a led
        digitalWrite(STATUS_PIN, 0);
        delay(50);
        digitalWrite(STATUS_PIN, 1);
        delay(50);
        digitalWrite(STATUS_PIN, 0);
 
        currentState = NORMAL;
      }
      break;
    case SEND:
      #ifdef DEBUG
        Serial.println("Ready to send");
      #endif
      // send
      if (timeToWait == 0) {
        currentState = NORMAL;
      } else {
        currentState = WAIT;
        previous = millis();
      }
      break;
    case WAIT:
      if (millis() - previous > timeToWait) {
        currentState = PROCESS_IR_STRING;
      }
      break;
    case PROCESS_IR_CODE:
      decodeIR(ir);
      currentState = SEND;
      break;
    case PROCESS_IR_STRING:
      decodeIRString(irString);
      currentState = PROCESS_IR_CODE;
      break;
    default:
      break;
  }
}

/***********************FUNCTION DEFINE***********************/

void connect() {
  #ifdef DEBUG
    Serial.print("\nMQTT connecting...");
  #endif
  while (!client.connect("arduino1", "try", "try")) {
    #ifdef DEBUG
      Serial.print(".");
    #endif
    delay(1000);
  }
  #ifdef DEBUG
    Serial.println("\nMQTT connected!");
  #endif
  client.subscribe("example");
  client.subscribe("example/s");
  client.subscribe("example/a");
  client.subscribe("example/h");
  // client.unsubscribe("/example");
}
/****** Funtion get time from the internet *************/
String getTime() {
  WiFiClient client;
  while (!client.connect("google.com", 80)) {
    Serial.println("connection failed, retrying...");
  }

  client.print("HEAD / HTTP/1.1\r\n\r\n");
 
  while(!client.available()) {
     yield();
  }

  while(client.available()){
    if (client.read() == '\n') {    
      if (client.read() == 'D') {    
        if (client.read() == 'a') {    
          if (client.read() == 't') {    
            if (client.read() == 'e') {    
              if (client.read() == ':') {    
                client.read();
                String theDate = client.readStringUntil('\r');
                client.stop();
                return theDate.substring(17, 22);
              }
            }
          }
        }
      }
    }
  }
}

void storeCode(decode_results *results) {
  codeType = results->decode_type;
  int count = results->rawlen;
  if (codeType == UNKNOWN) {
    #ifdef DEBUG
      Serial.println("This protocol isn't supported");
    #endif
  }
  else {
    #ifdef DEBUG
    if (codeType == NEC) {
      Serial.print("Received NEC: ");
      if (results->value == REPEAT) {
        // Don't record a NEC repeat value as that's useless.
        Serial.println("repeat; ignoring.");
        return;
      }
    } 
    else if (codeType == SONY) {
      Serial.print("Received SONY: ");
    } 
    else if (codeType == PANASONIC) {
      Serial.print("Received PANASONIC: ");
    }
    else if (codeType == JVC) {
      Serial.print("Received JVC: ");
    }
    else if (codeType == RC5) {
      Serial.print("Received RC5: ");
    } 
    else if (codeType == RC6) {
      Serial.print("Received RC6: ");
    } 
    else {
      Serial.print("Unexpected codeType ");
      Serial.print(codeType, DEC);
      Serial.println("");
    }
    Serial.println(results->value, HEX);
    #endif
    codeValue = results->value;
    codeLen = results->bits;
  }
}

void sendCode() {
  if (codeType == NEC) {
      irsend.sendNEC(codeValue, codeLen);
      #ifdef DEBUG
        Serial.print("Sent NEC ");
        Serial.println(codeValue, HEX);
      #endif
  } else if (codeType == SONY) {
    irsend.sendSony(codeValue, codeLen);
    #ifdef DEBUG
      Serial.print("Sent Sony ");
      Serial.println(codeValue, HEX);
    #endif
  } 
  else if (codeType == PANASONIC) {
    irsend.sendPanasonic(codeValue, codeLen);
    #ifdef DEBUG
      Serial.print("Sent Panasonic");
      Serial.println(codeValue, HEX);
    #endif
  }
  else if (codeType == JVC) {
    irsend.sendPanasonic(codeValue, codeLen);
    #ifdef DEBUG
      Serial.print("Sent JVC");
      Serial.println(codeValue, HEX);
    #endif
  }
  else if (codeType == RC5 || codeType == RC6) {
    // Put the toggle bit into the code to send
    codeValue = codeValue & ~(1 << (codeLen - 1));
    codeValue = codeValue | (toggle << (codeLen - 1));
    if (codeType == RC5) {
      #ifdef DEBUG
        Serial.print("Sent RC5 ");
        Serial.println(codeValue, HEX);
      #endif
      irsend.sendRC5(codeValue, codeLen);
    } 
    else {
      irsend.sendRC6(codeValue, codeLen);
      #ifdef DEBUG
        Serial.print("Sent RC6 ");
        Serial.println(codeValue, HEX);
      #endif
    }
  } 
  else if (codeType == UNKNOWN /* i.e. raw */) {
    // Assume 38 KHz
    //irsend.sendRaw(rawCodes, codeLen, 38);
    //Serial.println("Sent raw");
  }
}

void sendCodeToBroker() {
  String a = String(codeType) + '/' + String(codeValue) + '/' + String(codeLen) + '/' + String(toggle);
  #ifdef DEBUG
    Serial.println(a);
  #endif
  client.publish("example1", a);
}

void decodeIR(String irString) {
  String a;
  String arr[5];
  arr[4] = "0";
  int8_t k = 0, j = -1;
  int n = irString.length();
  for (int i = 0; i < n; i++) {
    if (irString[i] == '/') {
      arr[k] = irString.substring(j + 1, i);
      j = i;
      k++;
    }
  }
  arr[4] = irString.substring(j + 1);

  codeType = int(arr[0].toInt());
  codeValue = (unsigned long)(arr[1].toInt());
  codeLen = int(arr[2].toInt());
  toggle = int(arr[3].toInt());
  timeToWait = (unsigned long)(arr[4].toInt());
  #ifdef DEBUG
    Serial.println(arr[0]);
    Serial.println(arr[1]);
    Serial.println(arr[2]);
    Serial.println(arr[3]);
    Serial.println(arr[4]);
  #endif
}

void decodeIRString(String irstring) {
  int8_t n = irstring.length();
  for (int8_t i = 0; i < n; i++) {
    if (irstring[i] == '-') {
      ir = irString.substring(0, i);
      irString = irstring.substring(i + 1);
      #ifdef DEBUG
        Serial.println(ir);
        Serial.println(irString);
      #endif
      break;
    }
  }
}

/************* Handel the massage receive **********/
void messageReceived(String topic, String payload, char * bytes, unsigned int length) {
  if (payload.equals("r")) {
    #ifdef DEBUG
      Serial.println("Ready to recieve");
    #endif
    digitalWrite(STATUS_PIN, 1);
    currentState = RECEIVE;
  } else if (topic.equals("example/s")){
    #ifdef DEBUG
      Serial.println("Ready to send");
    #endif
    ir = payload;
    currentState = PROCESS_IR_CODE;
  } else if (topic.equals("example/a")){
    codeString[indexString] = payload;
    indexString++;
    currentState = NORMAL;
    #ifdef DEBUG
      Serial.println("Recieve an action");
      Serial.println("Action number:" + String(indexString));
    #endif
  } else if (topic.equals("example/h")) {
    hourString[indexString] = payload;
    #ifdef DEBUG
      Serial.println("Recieve an hour");
      Serial.println("Action number:" + String(indexString));
    #endif
    currentState = NORMAL;
  } else {
    currentState = NORMAL;
  }
}
