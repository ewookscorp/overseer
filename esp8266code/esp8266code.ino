/* DHTServer - ESP8266 Webserver with a DHT sensor as an input

   Based on ESP8266Webserver, DHTexample, and BlinkWithoutDelay

   Intial version for this Homeautomation projext <sami@tabloiti.com>
*/

extern "C" {
#include "user_interface.h"
}
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <DHT.h>
#include <aJSON.h>

#define DHTTYPE DHT11
#define DHTPIN  2

const char* ssid     = "default";
const char* password = "default";
const char* place = "sensor-3";
String ip_addr = "0.0.0.0";

ESP8266WebServer server(80);
 
// Initialize DHT sensor  
DHT dht(DHTPIN, DHTTYPE, 11); // 11 works fine for ESP8266
 
float humidity, temp_f;  // Values read from sensor
float humidity_old, temp_f_old;
String webString="";     // String to display

unsigned long previousMillis = 0;        // will store last temp was read
unsigned long previousUdpMillis = 0;        // will store last temp was read
unsigned long previousReset = 0;
const long interval = 2000;              // interval at which to read sensor
const long udp_interval = 5000;
const long reset_interval = 10000; // 5minustes = 300000


//================================================================
// Config page HTML page: TODO: this page not actual yet WORK!!
// need to do settings saving
//================================================================
void showconfigpage(ESP8266WebServer server) {

  String content="";
  content += "<form method=get>";
  content += "<label>Device name</label><br>";
  content += "<input  type='text' name='name' maxlength='30' size='15' value='";
  content += place;
  content += "'><br>";
  content += "<label>SSID</label><br>";
  content += "<input  type='text' name='ssid' maxlength='30' size='15' value='"; 
  content += ssid; 
  content += "'><br>";
  content += "<label>Password</label><br>";
  content += "<input  type='password' name='password' maxlength='30' size='15'><br><br>";
  content += "<input  type=\"button\" value=\"Cancel\" onclick=\"window.location='/';return false;\">";
  content += "<input  type='submit' value='Save & Connect' >";
  content += "</form>";

  server.send(200," text/html", content);
}

//================================================================
// Get Temperature and humidity form DHT server
//================================================================
void gettemperature() {
  // Wait at least 2 seconds seconds between measurements.
  unsigned long currentMillis = millis();


  if(currentMillis - previousMillis >= interval) {
    // save the last time you read the sensor 
    previousMillis = currentMillis;   

    // Reading temperature for humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (it's a very slow sensor)
    humidity = dht.readHumidity();          // Read humidity (percent)
    temp_f = dht.readTemperature(true);     // Read temperature as Fahrenheit

    if(temp_f >= 1000.0)
       temp_f = temp_f_old;
    else
       temp_f_old = temp_f;
       
    if(humidity >= 1000.0)
      humidity = humidity_old;
    else
      humidity_old = humidity;     
      
    // Check if any reads failed and exit early (to try again).
    if (isnan(humidity) || isnan(temp_f)) {
      Serial.println("Failed to read from DHT sensor!");
      temp_f = 0;
      humidity = 0;

      if(temp_f_old >= 1)
        temp_f = temp_f_old;

      if(humidity_old >=1)
         humidity = humidity_old;

      return;
    }  
  }
}

//================================================================
// Main Page handel
//================================================================
void handle_root() {
  String content = "Hello from Ewook Weather sensor, read from /temp, /humidity, /json, /config<br><br>";
  content += "Connected to:";
  content += ssid ;
  content += "<br>IP address: ";
  content += ip_addr; // WiFi.localIP();
  content += "<br>Name: ";
  content += place; 
  
  server.send(200, "text/html", content);
  delay(100);
}

//================================================================
// Convert IP address to String
//================================================================
String ipToString(IPAddress ip){
  String s="";
  for (int i=0; i<4; i++)
    s += i  ? "." + String(ip[i]) : String(ip[i]);
  return s;
} 

//================================================================
// Main intialize setup
//================================================================
void setup(void)
{
  // You can open the Arduino IDE Serial Monitor window to see what the code is doing
  Serial.begin(115200);  // Serial connection from ESP-01 via 3.3v console cable
  dht.begin();           // initialize temperature sensor

  humidity_old = 0;
  temp_f_old = 0;

  // Connect to WiFi network
  WiFi.begin(ssid, password);
  Serial.print("\n\r \n\rWorking to connect");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("DHT Weather Reading Server");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  ip_addr = ipToString(WiFi.localIP());

  // set udp port for listen
  udp.begin(udplocalPort);
  
  Serial.print("Local port: ");
  Serial.println(udp.localPort());

  server.on("/reset", [](){
    server.send(200, "text/plain", "Reset Called");
      //system_restart();
      //Work a round to disable wdt since wtc wont restart
      //ESP after crash
      ESP.wdtDisable();
      ESP.restart();
    }); 
  server.on("/", handle_root);
  
  server.on("/tempf", [](){  // if you add this subdirectory to your webserver call, you get text below :)
    gettemperature();       // read sensor
    webString="Temperature: "+String((int)temp_f)+" F";   
    server.send(200, "text/plain", webString);           
  });

    server.on("/temp", [](){  // if you add this subdirectory to your webserver call, you get text below :)
    gettemperature();       // read sensor
    float celsius = ((int)temp_f - 32)/1.8;
    webString="Temperature: "+String((float)celsius)+" C";   // Arduino has a hard time with float to string
    server.send(200, "text/plain", webString);            // send to someones browser when asked
  });

  server.on("/humidity", [](){  // if you add this subdirectory to your webserver call, you get text below :)
    gettemperature();           // read sensor
    webString="Humidity: "+String((int)humidity)+"%";
    server.send(200, "text/plain", webString);               // send to someones browser when asked
  });

  server.on("/json", [](){  
    gettemperature();           // read sensor
    float celsius = ((int)temp_f - 32)/1.8;
    //webString="{"humidity:"+String((int)humidity)+"%"+",temp:%.2f"+String((float)celsius)+" C}";

    aJsonObject *root,*fmt;
    root=aJson.createObject();  
    float temp_round = round(celsius);

    aJson.addItemToObject(root, "temperature", aJson.createItem( (float)temp_round) );
    aJson.addItemToObject(root, "temperature2", aJson.createItem( (int)temp_f ) );
    aJson.addItemToObject(root, "humidity", aJson.createItem((int)humidity ) );
    aJson.addItemToObject(root, "place", aJson.createItem( place ) );
   
    char *data= aJson.print(root);
    String webString(data);
    server.send(200, "text/plain",webString);               // send to someones browser when asked
  });

  /* config wlan and device name via webinterface*/
  server.on("/config", [](){
    showconfigpage(server);
  });
    
  server.begin();
  Serial.println("HTTP server started");
}

//================================================================
// Main loop
//================================================================ 
void loop(void)
{
  server.handleClient();
  /*Work a round to disable wdt since wtc wont restart
    ESP after crash. NOTE GPIO-0 need to be disconnected*/
   ESP.wdtDisable();
} 

 


