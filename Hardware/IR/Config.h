#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <ESP8266WiFi.h>          
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <IRremoteESP8266.h>    
#include <MQTTClient.h>   
#include <Ticker.h>

#define DEBUG

// pin define
#define RECEIVE_PIN       5
#define SEND_PIN          16
#define STATUS_PIN        14
#define WIFI_STATUS_PIN   2

// state define
#define NORMAL				    100
#define RECEIVE				    101
#define SEND 				      102
#define WAIT				      103
#define PROCESS_IR_CODE		104
#define PROCESS_IR_STRING	105

// function declare

void connect();
void sendCodeToBroker();
void decodeIR(String );
void decodeIRString(String );
String getTime();
void storeCode(decode_results* );
void sendCode();

#endif
