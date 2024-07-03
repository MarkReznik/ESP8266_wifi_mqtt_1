#include <dummy.h>

#include <ESP8266httpUpdate.h>


#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <RCSwitch.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
//#include <Update.h>

#define USE_SERIAL Serial
const char *sketch_ver = "0.0.2";
//WiFiMulti wifiMulti;
// MQTT Broker settings
const char *mqtt_broker = "broker.emqx.io";//"f16e3b17.ala.asia-southeast1.emqxsl.com";  // EMQX broker endpoint
const char *mqtt_topic_root_con = "972526435150/root/connected";  // MQTT topic
const char *mqtt_topic_devid = "/1";
const char *mqtt_topic_ver = "/sketch/ver";  // MQTT topic ver
const char *mqtt_username = "";//"mark";  // MQTT username for authentication
const char *mqtt_password = "";//"123456";  // MQTT password for authentication
const int mqtt_port = 8883;//8883;  //8883;//1883;  // MQTT port (TCP)

//WiFiClient espClient;
WiFiClientSecure espClient;     // <-- Change #1: Secure connection to MQTT Server

// Create an instance of WiFiClientSecure
//WiFiClientSecure wifiClient;
// Create an instance of PubSubClient
PubSubClient mqtt_client(espClient);



void connectToMQTTBroker();

void mqttCallback(char *topic, byte *payload, unsigned int length);

RCSwitch mySwitch = RCSwitch();
#define INTPIN  0//D2
void setup() {
    // WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
    // it is a good practice to make sure your code sets wifi mode how you want it.

    // put your setup code here, to run once:
    Serial.begin(115200);
    
    mySwitch.enableReceive(INTPIN);  // Receiver on interrupt 0 => that is pin #2
    
    //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wm;

    // reset settings - wipe stored credentials for testing
    // these are stored by the esp library
    // wm.resetSettings();

    // Automatically connect using saved credentials,
    // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
    // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
    // then goes into a blocking loop awaiting configuration and will return success result

    bool res;
    // res = wm.autoConnect(); // auto generated AP name from chipid
     res = wm.autoConnect("192-168-4-1"); // anonymous ap
    //wm.erase();
    //res = wm.autoConnect("ip192-168-4-1_p1-8","12345678"); // password protected ap
    //wm.erase()
    if(!res) {
        Serial.println("Failed to connect");
        // ESP.restart();
    } 
    else {
        //if you get here you have connected to the WiFi    
        Serial.println("connected...yeey :)");
        //espClient.setFingerprint(fingerprint);   // <-- Change #3: Set the SHA1 fingerprint
        espClient.setInsecure();// Alternative: 
        
        mqtt_client.setServer(mqtt_broker, mqtt_port);
        mqtt_client.setCallback(mqttCallback);
        
        // Load root CA certificate into WiFiClientSecure object
        //wifiClient.setTrustAnchors(new BearSSL::X509List(rootCACertificate));

        connectToMQTTBroker();
    }

}

void connectToMQTTBroker() {
    while (!mqtt_client.connected()) {
        String client_id = "esp8266-client-" + String(WiFi.macAddress());
        Serial.printf("\r\nConnecting to MQTT Broker as %s.....\n", client_id.c_str());
        if (mqtt_client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
            Serial.println("Connected to MQTT broker");
            String addstr = String(mqtt_topic_root_con) + "/#";
            //const char *tmpstr=addstr.c_str(addstr);
            Serial.print("Subscribe to ");Serial.println(addstr);
            mqtt_client.subscribe(addstr.c_str());
            // Publish message upon successful connection
            mqtt_client.publish(mqtt_topic_root_con, "Hi EMQX I'm ESP8266 devid=1 ^^");
        } else {
            Serial.print("Failed to connect to MQTT broker, rc=");
            Serial.print(mqtt_client.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}
void mqttCallback(char *topic, byte *payload, unsigned int length) {
    Serial.print("Message received on topic: ");
    Serial.println(topic);
    Serial.print("Message:");
    for (unsigned int i = 0; i < length; i++) {
        Serial.print((char) payload[i]);
    }
    Serial.println();
    Serial.println("-----------------------");
    if(strcmp(topic,mqtt_topic_root_con)==0){
      String txstr = String(topic) + "/";
      if((char)payload[0]=='?'){        
        mqtt_client.publish(txstr.c_str(), mqtt_topic_devid);
      }
      else if((char)payload[0]=='v'){
        mqtt_client.publish(txstr.c_str(), sketch_ver);
      }
    }
    
}

void printHex(uint8_t num) {
  char hexCar[2];

  sprintf(hexCar, "%02X", num);
  Serial.print(hexCar);
}



void httpsget(const String &server, const String &filepath)
{
// wait for WiFi connection
    //if((wifiMulti.run() == WL_CONNECTED)) 
    {
        //std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
        //client->setFingerprint(fingerprint_sni_cloudflaressl_com);
        BearSSL::WiFiClientSecure client;
        client.setInsecure();
        Serial.println("\nStarting connection to server...");
        Serial.println(server);
        //const char*  server = "www.howsmyssl.com";  // Server URL
        if (!client.connect(server, 443))
          Serial.println("Connection failed!");
        else {
          Serial.println("Connected to server!");
          // Make a HTTP request:
          Serial.println(server);
          Serial.println(filepath);
          client.print("GET ");client.print(filepath);client.println(" HTTP/1.0");
          //client.println("GET https://www.howsmyssl.com/a/check HTTP/1.0");
          //client.println("Host: www.howsmyssl.com");
          client.print("Host: ");client.println(server);
          client.println("Connection: close");
          client.println();
          
          while (client.connected()) {
            String line = client.readStringUntil('\n');
            if (line == "\r") {
              Serial.println("headers received");
              break;
            }
          }
          
          // if there are incoming bytes available
          // from the server, read them and print them:
          long readcount=0;
          int retry=0;
          //Update.begin(size_t size)
          if(client.connected()){
            while (client.available() || retry<100) {
              if (client.available()){
                char c = client.read();
                //Serial.write(c);
                if((++readcount % 100000)==0){
                  Serial.println(readcount);
                }
                retry=0;
                //Update.write()    
              }
              else{
                delay(3);
                retry++;
              }
            }
            Serial.print("size: ");Serial.println(readcount);
            Serial.print("retry: ");Serial.println(retry);
          }
          else{
            Serial.println("client disconected");
          }
          client.stop();
        }
    }
}

void loop() {
    // put your main code here, to run repeatedly:  
    if (mySwitch.available()) {
      //output(mySwitch.getReceivedValue(), mySwitch.getReceivedBitlength(), mySwitch.getReceivedDelay(), mySwitch.getReceivedRawdata(),mySwitch.getReceivedProtocol());
      mySwitch.resetAvailable();
    } 
    
    if (!mqtt_client.connected()) {
        connectToMQTTBroker();
    }
    mqtt_client.loop();
    
    if(Serial.available()>0){
      //const String server="https://972526435150.ucoz.org/files/readtest.fw";
      mqtt_client.disconnect();
      char ch=(char)Serial.read();
      if(ch=='1'){
        httpsget("972526435150.ucoz.org","/files/0.0.1.bin");
      }
      else if(ch=='2'){
        httpsget("972526435150.ucoz.org","/files/0.0.2.bin");
      }
      
      Serial.readString();
      connectToMQTTBroker();
    }

}
