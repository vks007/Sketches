#include <ESP8266WiFi.h>
#include <espnow.h>
#include "secrets.h"

// REPLACE WITH RECEIVER MAC Address , This should be the address of the softAP (and NOT WiFi MAC addr obtained by WiFi.macAddress()) if the Receiver uses both, WiFi & ESPNow
// You can get the address via the command WiFi.softAPmacAddress() , usually it is one decimal no after WiFi MAC address
uint8_t broadcastAddress[] = {0xA4, 0xCF, 0x12, 0x96, 0x91, 0xB1};//A4:CF:12:96:91:B1 - This is the SoftAP MAC addr of the ESP32
//uint8_t broadcastAddress[] = {0xA4, 0xCF, 0x12, 0x96, 0x91, 0xB0};//A4:CF:12:96:91:B0 - This is the WiFi MAC addr of ESP32
//uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; //Use this if you want to broadcast to all slaves, you could also loop through and send to multiple slaves
//Note though that in broadcast mode the delivery of messages are auto marked as delivered.

#define BOARD_ID 1

typedef struct esp_now_peer_info {
  u8 peer_addr[6];
  uint8_t channel;
  uint8_t encrypt;
}esp_now_peer_info_t;


// Structure example to send data
// Must match the receiver structure
typedef struct struct_message {
    int id;
    float temp;
    float hum;
    int readingId;
} struct_message;


//Create a struct_message called myData
struct_message myData;

unsigned long previousMillis = 0;   // Stores last time temperature was published
const long interval = 2000;        // Interval at which to publish sensor readings

unsigned int readingId = 0;

// Insert your SSID
constexpr char WIFI_SSID[] = primary_ssid;

/*
 * Gets the WiFi channel of the SSID of your router, It has to be on the same channel as this one as the receiver will listen to ESPNow messages on the same 
 * While theritically it should be possible for WiFi and ESPNow to work on different channels but it seems due to some bug (or behavior) this isnt possible with espressif
 */
int32_t getWiFiChannel(const char *ssid) {
  if (int32_t n = WiFi.scanNetworks()) {
      for (uint8_t i=0; i<n; i++) {
          if (!strcmp(ssid, WiFi.SSID(i).c_str())) {
              return WiFi.channel(i);
          }
      }
  }
  return 0;
}

// callback when data is sent
esp_now_send_cb_t OnDataSent([](uint8_t *mac_addr, uint8_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == 0 ? "Delivery Success" : "Delivery Fail");
  if(status != 0)
  {
    WiFi.printDiag(Serial);
    if(wifi_get_channel() == 0)
      ESP.restart();
  }
});
 
void setup() {
  //Init Serial Monitor
  Serial.begin(115200);

  // Set device as a Wi-Fi Station and set channel
  WiFi.mode(WIFI_STA);

  int32_t channel = getWiFiChannel(WIFI_SSID);

// Sometimes after loading the new firmware the channel doesnt change to the required one, I have to reset the board to do the same.
// It rather reports the channel as 0 , I will have to look into this and solve, I think I saw some discussion around this where ESP remembers the last channel used 
// I think I have to fo a WiFi.disconnect to circumvent this, will try this out.

  WiFi.disconnect(); // trying to see if the issue of sometimes it not setting the right channel gets solved by this.
  // To change the channel you have to first call wifi_promiscuous_enable(true) , change channel and then call wifi_promiscuous_enable(false)
  WiFi.printDiag(Serial); // Uncomment to verify channel number before
  wifi_promiscuous_enable(true);
  wifi_set_channel(channel);
  wifi_promiscuous_enable(false);
  WiFi.printDiag(Serial); // Uncomment to verify channel change after

  //Init ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
  
//  //Register peer
//  esp_now_peer_info_t peerInfo;
//  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
//  peerInfo.encrypt = false;
  
  //Add peer , There is no method in ESP8266 to add a peer by passing esp_now_peer_info_t object unlike ESP32
  if (esp_now_add_peer((u8*)broadcastAddress, ESP_NOW_ROLE_SLAVE, channel, NULL, 0) != 0){
    Serial.println("Failed to add peer");
    return;
  }
}
 
void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    // Save the last time a new reading was published
    previousMillis = currentMillis;
    //Set values to send
    myData.id = BOARD_ID;
    myData.temp = (float)random(0,100)/1.2;
    myData.hum = (float)random(0,100)/1.2;
    myData.readingId++;
     
    //Send message via ESP-NOW , the result here just indicates that a message was sent , not that it was receieved.
    // For that you have to see the status in OnDataSent()
    int result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
    if (result == 0) {
      Serial.println("Sent with success");
    }
    else {
      Serial.println("Error sending the data");
    }
  }
}
