
#include <BME280I2C.h> //Ver 2.30 Written by Tyler Glenn, 2016.https://github.com/finitespace/BME280
#include <EnvironmentCalculations.h>
#include <ESP8266WiFi.h> // Defines WiFi, the WiFi controller object
#include <WiFiClientSecure.h>
#include <Wire.h>

/*
 * Go to https://weatherstation.wunderground.com/
 * in Chrome go to : >developer tools>Security View Sertificate
 * Copy SHA-1 Fingerprint.
 * Now it is :
 * SHA-1 Fingerprint 50 22 08 9D DC 3C 2D FC 21 D4 22 30 07 8B 2E 68 63 47 20 02
 * Replace in *sslFingerprint.
 * 
   Web Site specific constants.
     host = the hostname of the server to connect to.
     port = the HTTPS port to connect to (443 is the default).
     sslFingerprint = the SHA1 fingerprint of the SSL Certificate
       of the web site we will be connecting to.
     httpProtocol = the protocol to use. Some sites prefer HTTP/1.1.
       This Sketch uses HTTP/1.0 to avoid getting a response that is
       Transfer-encoding: chunked
       which is hard to parse.
     url = the url, less https:// and the server name.
     httpAgent = a string to identify our client software.
       This Sketch uses the name of the Github Repository of this project.
       Replace this with whatever you like (no spaces).

     The Weather Underground Personal Weather Station upload protocol
     is specified at https://feedback.weather.com/customer/en/portal/articles/2924682-pws-upload-protocol?b_id=17298
*/
const char *host = "weatherstation.wunderground.com";
const int port = 443;
const char *sslFingerprint
  = "50 22 08 9D DC 3C 2D FC 21 D4 22 30 07 8B 2E 68 63 47 20 02";
  // Certificate expires August 25, 2021
const char *httpProtocol = "HTTP/1.0";
const char *url = "/weatherstation/updateweatherstation.php";
const char *httpAgent = "BME280WeatherStation";

/* Recommended Modes -
   Based on Bosch BME280I2C environmental sensor data sheet.
   Weather Monitoring :
   forced mode, 1 sample/minute
   pressure ×1, temperature ×1, humidity ×1, filter off
   Current Consumption =  0.16 μA
   RMS Noise = 3.3 Pa/30 cm, 0.07 %RH
   Data Output Rate 1/60 Hz
 */
BME280I2C::Settings settings(
   BME280::OSR_X1,
   BME280::OSR_X1,
   BME280::OSR_X1,
   BME280::Mode_Forced,
   BME280::StandbyTime_1000ms,
   BME280::Filter_Off,
   BME280::SpiEnable_False,
   0x76 // I2C address. I2C specific.
);

BME280I2C bme(settings);
// client = manager of the HTTPS connection to the web site.
WiFiClientSecure client;

const int sleepTimeS = 300; //18000 for Half hour, 300 for 5 minutes etc.

char *wifiSsid="SSID";
char *wifiPassword="Password";

char *stationId="StationID";
char *stationKey="StationKey";

float temp,hum,pres,dewpF; 
 
void setup() {
  
  Serial.begin(115200);
  delay(100);

  Wire.begin();
  while(!bme.begin())
  {
    Serial.println("Could not find BME280I2C sensor!");
    delay(1000);
  }

  switch(bme.chipModel())
  {
     case BME280::ChipModel_BME280:
       Serial.println("Found BME280 sensor! Success.");
       break;
     case BME280::ChipModel_BMP280:
       Serial.println("Found BMP280 sensor! No Humidity available.");
       break;
     default:
       Serial.println("Found UNKNOWN sensor! Error!");
  }

  WiFi.mode(WIFI_STA);    // Station (Client), not soft AP or dual mode.
  WiFi.setAutoConnect(false); // don't connect until we call .begin()
  WiFi.setAutoReconnect(true); // if the connection drops, reconnect.

  Serial.print(F("Connecting to "));
  Serial.println(wifiSsid);
  WiFi.begin(wifiSsid, wifiPassword);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

  }
  Serial.println();
  
  Serial.println("WiFi connected");
  Serial.println("IP address: "); Serial.println(WiFi.localIP());
  

// Change some settings before using.
   settings.tempOSR = BME280::OSR_X4;

   bme.setSettings(settings);
   BME280::TempUnit tempUnit(BME280::TempUnit_Fahrenheit);
   BME280::PresUnit presUnit(BME280::PresUnit_inHg);

   bme.read(pres, temp, hum, tempUnit, presUnit);

   EnvironmentCalculations::TempUnit     envTempUnit =  EnvironmentCalculations::TempUnit_Fahrenheit;
   
   dewpF = EnvironmentCalculations::DewPoint(temp, hum, envTempUnit);
 
  Serial.print("Temperature: ");
  Serial.print(temp);
  Serial.println(" F");
  
  Serial.print("Pressure:    ");
  Serial.print(pres);
  Serial.println(" inHg");

  Serial.print("Humidity:    ");
  Serial.print(hum);
  Serial.println(" %");
  
  Serial.print("Dwepoint:    ");
  Serial.print(dewpF);
  Serial.println(" F");
  
// Report our data to WU server.
      if (HttpsPost()) {
        Serial.println("Success");
      }
  delay(2500);
    
//Connect IO16(D0) to RST to work properly !
  Serial.print(F("Sleeping..."));
  ESP.deepSleep(sleepTimeS * 1000000);
}


void loop() {
  // put your main code here, to run repeatedly:
            
}  
/*
    Uploads to Wunderground.
    Uses:
      stationId, stationKey
      station TempF,Himudity,Pressure,Dew pint in F
      and the server information (host, post, etc.)

    Returns true if successful, false otherwise.
*/
boolean HttpsPost() {
  char content[1024];  // Buffer for the content of the POST request.
  char *pContent;  // a pointer into content[]
  char ch;
  /*
     Build the content of the POST. That is, the parameters.
     UrlEncode the fields according to https://support.weather.com/s/article/PWS-Upload-Protocol?language=en_US
   */

  strcpy(content, "");

  strcat(content, "ID=");
  urlencodedcat(content, stationId);

  strcat(content, "&");
  strcat(content, "PASSWORD=");
  urlencodedcat(content, stationKey);
  
  strcat(content, "&");
  strcat(content, "action=updateraw"); // says "I'm posting the weather"

  strcat(content, "&");
  strcat(content, "dateutc=now"); // says when the data was collected
    
    strcat(content, "&");
    strcat(content, "tempf=");
    floatcat(content, temp);

    strcat(content, "&");
    strcat(content, "humidity=");
    floatcat(content, hum);
    
    strcat(content, "&");
    strcat(content, "baromin=");
    floatcat(content, pres);

    strcat(content, "&");
    strcat(content, "dewptf=");
    floatcat(content, dewpF);
    
  // Perform the Post

  client.setFingerprint(sslFingerprint);
  if (!client.connect(host, port)) {
    Serial.print("Failed to connect to ");
    Serial.print(host);
    Serial.print(" port ");
    Serial.println(port);
    Serial.print(" using SSL fingerprint ");
    Serial.print(sslFingerprint);
    Serial.println();
    return false;
  }
  Serial.print("Connected to ");
  Serial.println(host);

  client.print("POST ");
  client.print(url);
  client.print(" ");
  client.println(httpProtocol);

  client.print("Host: ");
  client.println(host);

  client.print("User-Agent: ");
  client.println(httpAgent);

  client.println("Connection: close");

  client.println("Content-Type: application/x-www-form-urlencoded");

  client.print("Content-Length: ");
  client.println(strlen(content));

  client.println();  // blank line indicates the end of the HTTP headers.

  // send the content: the POST parameters.
  client.print(content);

  Serial.println("Reading response:");
  while (client.connected()) {
    if (client.available()) {
      ch = client.read();
      Serial.print(ch);
    }
    delay(1); // to yield the processor for a moment.
  }

  Serial.println();
  Serial.println("connection closed.");

  client.stop();

  return true;
}

/*
   Append (concatenate) the given floating-point number
   as a string to the given buffer.

   buffer = a space to concatenate into. It must contain a null-terminated
    set of characters.
   f = the floating point number to convert into a set of characters.

*/
void floatcat(char *buffer, float f) {
  char *p;
  long l;

  p = buffer + strlen(buffer);

  // Convert the sign.
  if (f < 0.0) {
    *p++ = '-';
    f = -f;
  }

  // Convert the integer part.
  l = (long) f;
  f -= (float) l;
  itoa(l, p, 10);
  p += strlen(p);

  // Convert the first 2 digits of the fraction.
  *p++ = '.';
  f *= 100.0;
  itoa((long) f, p, 10);
}

/*
   Append (concatenate) the given string to the given buffer,
   UrlEncoding the string as it is appended.

   buffer = a space to concatenate into. It must contain a null-terminated
    set of characters, and be large enough to store the result.
   str = points to the null-terminated string to UrlEncode and append.

*/
void urlencodedcat(char *buffer, char *str) {
  char *pBuf; // points to the next empty space in buffer[]
  char *pIn;  // points to a character in str[]
  char ch;  // the current input character to UrlEncode.
  char hexDigit[] = "0123456789ABCDEF";

  pBuf = buffer + strlen(buffer);
  
  for (pIn = str; *pIn; ++pIn) {
    ch = *pIn;
    if (isAlphaNumeric(ch)) {
      // Alphabetic and numeric characters are passed as-is.
      *pBuf++ = ch;
      continue;
    }
    
    /*
       All other characters are encoded as a % followed by
       two hexidecimal characters. E.g., = is encoded as %3D.
       Reference: https://developer.mozilla.org/en-US/docs/Glossary/percent-encoding
     */

     *pBuf++ = '%';
     *pBuf++ = hexDigit[((int) ch >> 4) & 0xF];
     *pBuf++ = hexDigit[((int) ch) & 0xF];
  }

  *pBuf = (char) 0; // null terminate the output.
}

/*
 * Calculates the Dew point given temperature and relative humidity just for reference.
 */

double dewPoint(double tempf, double humid) //Calculate dew Point
{
  double A0= 373.15/(273.15 + tempf);
  double SUM = -7.90298 * (A0-1);
  SUM += 5.02808 * log10(A0);
  SUM += -1.3816e-7 * (pow(10, (11.344*(1-1/A0)))-1) ;
  SUM += 8.1328e-3 * (pow(10,(-3.49149*(A0-1)))-1) ;
  SUM += log10(1013.246);
  double VP = pow(10, SUM-3) * humid;
  double T = log(VP/0.61078);   
  return (241.88 * T) / (17.558-T);
}
