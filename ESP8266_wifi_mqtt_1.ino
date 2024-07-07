#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <RCSwitch.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
//#include <Update.h>
#include <EEPROM.h>

#define USE_SERIAL Serial
const char *sketch_VER = "0012";
const char *sketch_VID = "0001";//type of device 0001=ESP8622EX Wemos D1 R2&mini RF433+MQTT+Update-Https
const char *sketch_VID_info = "VID0001 - ESP8622EX Wemos D1 R2&mini. RF433+MQTT+Update-Https";
uint32_t chipID=ESP.getChipId();
String  DEVID;
String  mqtt_topic_devid;
//WiFiMulti wifiMulti;
// MQTT Broker settings
const char *mqtt_broker = "broker.emqx.io";//"f16e3b17.ala.asia-southeast1.emqxsl.com";  // EMQX broker endpoint
const char *mqtt_topic_root_con = ("972526435150/root/connected");//+"/"+String(sketch_VID)).c_str();  // MQTT topic
//const char *mqtt_topic_devid = ("/"+String(sketch_VID)+"/"+String(DEVID)).c_str();//"/1";
//const char *mqtt_topic_ver = "/sketch/ver";  // MQTT topic ver
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
#define INTPIN  D2//0//D2



// Define the name for the downloaded firmware file
//#define FILE_NAME "firmware.bin"
String fwfilename="";
String filepath="";
String hostname="";

bool runupdate=false;
#define ENTER D5
#define UP  D6
#define DOWN  D7

#define BUTTONS_MAX 4
#define EEPROM_OFFSET 10
int rf433_but=0;

bool eepromWriteUint32(const int address,uint32_t val){
  uint8_t i=0;
  uint8_t wrbyte=0;
  Serial.print("EEPROM 1st address : 0x");Serial.println(address,HEX);
  Serial.print("uint32 value to write: 0x");Serial.println(val,HEX);
  for(i=0;i<4;i++){
    wrbyte=(uint8_t)((val>>(i*8))&0xff);
    Serial.print(wrbyte,HEX);
    EEPROM.write((address*4)+i, wrbyte);
  }
  Serial.println();
  Serial.println("uint32 value writen.");
  if (EEPROM.commit()) {
    Serial.println("EEPROM successfully committed");
    return true;
  } else {
    Serial.println("ERROR! EEPROM commit failed");
  }
  return false;
}
uint32_t eepromReadUint32(const int address){
  uint8_t i=0;
  uint8_t rdbyte=0;
  uint32_t val=0;
  Serial.print("EEPROM 1st address : 0x");Serial.println(address,HEX);
  for(i=0;i<4;i++){
    rdbyte=EEPROM.read((address*4)+3-i);
    Serial.print(rdbyte,HEX);
    val<<=8;
    val|=rdbyte;    
  }
  Serial.println();
  Serial.print("uint32 value read: 0x");Serial.println(val,HEX);
  return val;
}

void changeButton(uint8_t button, bool state){
  if(state){
    digitalWrite(button,LOW);
    pinMode(button, OUTPUT);
  }
  else{
    pinMode(button, INPUT);
  }
}
void pressButton(uint8_t button){
  changeButton(button,true);
  delay(500);
  changeButton(button, false);
}

void setup() {
    // WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
    // it is a good practice to make sure your code sets wifi mode how you want it.
    pinMode(UP, INPUT);
    pinMode(DOWN, INPUT);
    pinMode(ENTER, INPUT);

    EEPROM.begin(512);//define eeprom use 512 bytes
    
    // put your setup code here, to run once:
    Serial.begin(115200);
    delay(1000);
    DEVID=String(ESP.getChipId());
    mqtt_topic_devid="/"+String(sketch_VID)+"/"+DEVID;
    Serial.println("");
    Serial.println("Firmware Version: "+String(sketch_VER));
    Serial.println("Sketch VID/type: "+String(sketch_VID));
    Serial.println("DEVID :"+DEVID);

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
            Serial.print("Subscribe to ");Serial.println(addstr);
            mqtt_client.subscribe(addstr.c_str());
            // Publish message upon successful connection
            mqtt_client.publish(mqtt_topic_root_con, ("Hi EMQX I'm "+ 
                  String(sketch_VID_info) + " ID: "+ DEVID + " FW: "+ String(sketch_VER)).c_str());
        } else {
            Serial.print("Failed to connect to MQTT broker, rc=");
            Serial.print(mqtt_client.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}
void mqttCallback(char *topic, byte *payload, unsigned int length) {
    String topicStr = topic; 
    payload[length] = 0;
    String recv_payload = String(( char *) payload);
    Serial.println( "mqtt_callback - message arrived :\r\ntopic [" + topicStr + 
                    "] \r\npayload [" + recv_payload + "]" );
    Serial.println();
    Serial.println("-----------------------");
    if(topicStr==String(mqtt_topic_root_con)){
      
      if(recv_payload=="?"){//if((char)payload[0]=='?'){        
        mqtt_client.publish((topicStr).c_str(), mqtt_topic_devid.c_str());
      }
    }
    else if(topicStr==String(mqtt_topic_root_con)+String(mqtt_topic_devid)+"/firmware"){
      if(recv_payload.endsWith(".bin")){
        if(recv_payload.startsWith(sketch_VID)){
          String ver=recv_payload.substring(String(sketch_VID).length(),recv_payload.indexOf(".bin"));
          if(ver>sketch_VER){
            mqtt_client.publish((topicStr).c_str(), ("Updating to "+ver).c_str());
            fwfilename=recv_payload;
            runupdate=true;
          }
          else{
            mqtt_client.publish((topicStr).c_str(), ("Firmware is up-to date"));
          }
        }
      }
      else if(recv_payload=="ver"){
        mqtt_client.publish((topicStr).c_str(), String(sketch_VER).c_str());
      }
      
    }
    //topicStr=String(mqtt_topic_root_con)+String(mqtt_topic_devid)+"/button"+String(rf433_but);
    else if(topicStr==(String(mqtt_topic_root_con)+String(mqtt_topic_devid)+"/button/1")){
      if(recv_payload=="?"){        
        mqtt_client.publish((topicStr).c_str(), "scan,code,state");
        rf433_but=1;
      }
      else if(recv_payload=="scan"){        
        mqtt_client.publish((topicStr).c_str(), "send RF433 for button 1");
        rf433_but=1;
      }
      else if(recv_payload=="code"){        
        uint32_t u32val=eepromReadUint32(EEPROM_OFFSET+0);
        mqtt_client.publish((topicStr).c_str(), String(u32val,HEX).c_str());
        rf433_but=1;
      }
      else if(recv_payload=="state"){        
        mqtt_client.publish((topicStr).c_str(), "TBD");
        rf433_but=0;
      }
      else if(recv_payload=="1"){        
        pressButton(UP);
        mqtt_client.publish((topicStr).c_str(), "UP OK");
        rf433_but=0;
      }
    }
    else if(topicStr==(String(mqtt_topic_root_con)+String(mqtt_topic_devid)+"/button/2")){
      if(recv_payload=="scan"){        
        mqtt_client.publish((topicStr).c_str(), "send RF433 for button 2");
        rf433_but=2;
      }
      else if(recv_payload=="1"){        
        pressButton(DOWN);
        mqtt_client.publish((topicStr).c_str(), "DOWN OK");
        rf433_but=0;
      }
    }
    else if(topicStr==(String(mqtt_topic_root_con)+String(mqtt_topic_devid)+"/button/3")){
      if(recv_payload=="scan"){        
        mqtt_client.publish((topicStr).c_str(), "send RF433 for button 3");
        rf433_but=3;
      }
      else if(recv_payload=="1"){        
        pressButton(ENTER);
        mqtt_client.publish((topicStr).c_str(), "ENTER OK");
        rf433_but=0;
      }
    }
    else if(topicStr==(String(mqtt_topic_root_con)+String(mqtt_topic_devid)+"/button/4")){
      if(recv_payload=="scan"){        
        mqtt_client.publish((topicStr).c_str(), "send RF433 for button 4");
        rf433_but=4;
      }
    }
}

void printHex(uint8_t num) {
  char hexCar[2];

  sprintf(hexCar, "%02X", num);
  Serial.print(hexCar);
}

void httpsUpdate(const String &server, const String &filepath)
{
  mqtt_client.disconnect();
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
    client.print("Host: ");client.println(server);
    client.println("Connection: close");
    client.println();
    int filesize=0;
    while (client.connected()) {
      String line = client.readStringUntil('\n');
      Serial.println(line);
      int sizeStart=line.indexOf("Content-Length: ");
      if(sizeStart>=0){
        Serial.print("Found length sizeStart:");Serial.println(sizeStart);
        filesize=line.substring(sizeStart + String("Content-Length: ").length()).toInt();
        Serial.print("Length int: ");Serial.println(filesize);
      }
      if (line == "\r") {
        Serial.println("headers received");              
        break;
      }
    }
    if(filesize>0){
      Update.begin(filesize);
      Serial.println("begin size: " + String(Update.size()));
      Serial.println("progress: " + String(Update.progress()));
      Serial.println("remaining: " + String(Update.remaining()));
    }
    // if there are incoming bytes available
    // from the server, read them and print them:
    long readcount=0;
    int retry=0;
    int buf_size=1024;
    uint8_t buf[buf_size];
    int buf_idx=0;
    int len=0;
    if(client.connected()){
      while (client.available() || retry<100) {
        if (client.available()){
          buf_idx=client.readBytes(buf,buf_size);
          len=Update.write(buf,buf_idx);//len=Update.write(buf,buf_size);
          if(len<buf_size){
            if((len==buf_idx)&&((filesize-readcount)<buf_size)){
              Serial.print("last write only: ");Serial.println(len);
              readcount+=buf_idx;
              break;
            }
            Serial.print("write only: ");Serial.println(len);
            len=Update.write(&buf[len],(buf_size-len));
          }
          readcount+=buf_size;//readcount++;
          if((readcount % (buf_size<<5))==0){//if((readcount % (filesize>>4))==0){
            Serial.println("writen: "+String((readcount*100)/filesize)+" %");
            //Serial.print(".");
          }
          if(readcount>filesize){
            Serial.println("error: read > length");
            break;
          }
          retry=0;                   
        }
        else{
          delay(3);
          retry++;
        }
      }// Complete the OTA update process
      if(Update.end()) {
        Serial.println("Successful update");
        Serial.println("Reset in 3 seconds....");
        delay(3000);
        ESP.restart(); // Restart ESP32 to apply the update
      }
      else {
        Serial.println("Error Occurred:" + String(Update.getError()));
        Serial.println("size: " + String(Update.size()));
        Serial.println("progress: " + String(Update.progress()));
        Serial.println("remaining: " + String(Update.remaining()));
        if(Update.getError()!=0){
          //connectToMQTTBroker();
          return;
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
  //connectToMQTTBroker();
}

void loop() {
    // put your main code here, to run repeatedly:  
    if (mySwitch.available()) {
      uint32_t rf433id=mySwitch.getReceivedValue();
      uint32_t u32val=0;
      int i;
      Serial.print("RF433 receved value:");Serial.println(rf433id,HEX);
      output(mySwitch.getReceivedValue(), mySwitch.getReceivedBitlength(), mySwitch.getReceivedDelay(), mySwitch.getReceivedRawdata(),mySwitch.getReceivedProtocol());
      mySwitch.resetAvailable();
      String topicStr;
      if(rf433_but==0){//scan if mutch button
        for(i=0;i<BUTTONS_MAX;i++){
          u32val=eepromReadUint32(EEPROM_OFFSET+i);
          if(u32val==rf433id){
            topicStr=String(mqtt_topic_root_con)+String(mqtt_topic_devid)+"/button/"+String(i+1);
            if(i==0){        
              pressButton(DOWN);
              delay(1000);
              pressButton(ENTER);
              mqtt_client.publish((topicStr).c_str(), "Go sleep OK");
              rf433_but=0;
            }
            else if(i==1){        
              pressButton(ENTER);
              delay(1000);
              pressButton(UP);
              mqtt_client.publish((topicStr).c_str(), "Wakeup OK");
              rf433_but=0;
            }
            else{
              mqtt_client.publish(topicStr.c_str(), "Activated.");
            }
            break;
          }
        }
      }
      else if(rf433_but<BUTTONS_MAX){//program/write button to eeprom
        topicStr=String(mqtt_topic_root_con)+String(mqtt_topic_devid)+"/button/"+String(rf433_but);
        if(eepromWriteUint32(EEPROM_OFFSET+rf433_but-1,rf433id)){          
          mqtt_client.publish(topicStr.c_str(), "Write Success.");
        }
        else{
          mqtt_client.publish(topicStr.c_str(), "Write Failed.");  
        }
        while(mySwitch.available()){
          mySwitch.resetAvailable();
          delay(1);
        }
      }
      rf433_but=0;
    } 
    
    if (!mqtt_client.connected()) {
        connectToMQTTBroker();
    }
    mqtt_client.loop();
    
    if(runupdate){
      runupdate=false;
      hostname="972526435150.ucoz.org";
      filepath="/files/"+String(sketch_VID)+"/"+fwfilename;
      Serial.println("host: "+hostname+" filepath: "+filepath);
      httpsUpdate(hostname,filepath);
    }

    if(Serial.available()>0){
      //const String server="https://972526435150.ucoz.org/files/readtest.fw";
      //mqtt_client.disconnect();
      char ch=(char)Serial.read();
      if(ch=='1'){
        httpsUpdate("972526435150.ucoz.org","/files/D1mini_0.0.3.bin");
      }
      else if(ch=='2'){
        httpsUpdate("972526435150.ucoz.org","/files/0.0.2.bin");
      }
      else if(ch=='s'){
        Serial.println("chip real size: "+String(ESP.getFlashChipRealSize()));
        Serial.println("chip size: "+String(ESP.getFlashChipSize()));
        Serial.println("chip id: "+String(ESP.getFlashChipId()));
      }
      else if(ch=='r'){
        ESP.reset();
      }
      else if(ch=='u'){
        String update_host=Serial.readStringUntil('/');update_host.trim();
        String update_file=Serial.readStringUntil('\r');update_file.trim();
        Serial.println("host: "+update_host+" filepath: "+update_file);
        httpsUpdate(update_host,update_file);
      }
      else if(ch=='e'){
        String strU32;
        int addr;
        uint32_t val32;
        bool res;
        ch=(char)Serial.read();
        if(ch=='r'){
          strU32=Serial.readStringUntil('\r');strU32.trim();
          addr=strU32.toInt();
          Serial.print("EEPROM read address: ");Serial.println(addr,HEX);
          val32=eepromReadUint32(addr);
          Serial.print("EEPROM read value: ");Serial.println(val32,HEX);
        }
        else if(ch=='w'){
          strU32=Serial.readStringUntil(' ');strU32.trim();
          addr=strU32.toInt();
          Serial.print("EEPROM write address: ");Serial.println(addr,HEX);
          strU32=Serial.readStringUntil('\r');strU32.trim();
          val32=strU32.toInt();
          Serial.print("EEPROM wtite value: ");Serial.println(val32,HEX);
          res=eepromWriteUint32(addr,val32);
        }
      }
      Serial.readString();
    }

}
