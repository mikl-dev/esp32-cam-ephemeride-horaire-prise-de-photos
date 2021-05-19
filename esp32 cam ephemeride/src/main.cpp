/***********************************************************************************
   Horloge connectée à base d'ESP8266 ou d'ESP32
   Affichage de l'heure et de la date dans le moniteur série.
   L'heure est obtenue grâce à un serveur NTP
   
   https://electroniqueamateur.blogspot.com/2018/10/horloge-wi-fi-esp8266.html
   
*************************************************************************************/

//ATTENTION RETIRER +1 A LA LIGNE 56, 73 ET 82  POUR LE PASSAGE EN HIVER

#if defined ARDUINO_ARCH_ESP8266  // s'il s'agit d'un ESP8266
#include <ESP8266WiFi.h>
#elif defined ARDUINO_ARCH_ESP32  // s'il s'agit d'un ESP32
#include "WiFi.h"
#endif

// Partie pour calcul d'ephemeride
#include <time.h>
#include <Wire.h> // bibliotheque Wire 
#include <Ephemeris.h> // bibliotheque ephemeris

// partie pour photo et envoi par mail
#include "esp_camera.h"
#include "Arduino.h"
#include "FS.h"                // SD Card ESP32
#include "SD_MMC.h"            // SD Card ESP32
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"
#include <EEPROM.h>            // read and write from flash memory
#include "SD.h"
#include "ESP32_MailClient.h"
#include "SPIFFS.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "driver/rtc_io.h"

// Pour l'heure du reseau
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "fs.h"

const char* ssid = "";
const char* password = "";

const int decalage = 2;  // la valeur dépend de votre fuseau horaire. Essayez 2 pour la France. 
const int delaiDemande = 5 * 60; // nombre de secondes entre deux demandes consécutives au serveur NTP

unsigned long derniereDemande = millis(); // moment de la plus récente demande au serveur NTP
unsigned long derniereMaJ = millis(); // moment de la plus récente mise à jour de l'affichage de l'heure
time_t maintenant;
struct tm * timeinfo;

// initialisation des variables pour ephemeride
int day = 0;
int month = 0;
int year = 0;
int hours, minutes;
float seconds;
String heuresSunriseConcaten;
String heuresSunsetConcaten;

void printDate(int day, int month, int year)
{
  Serial.print(day);
  Serial.print("/");
  Serial.print(month);
  Serial.print("/");
  Serial.print(year);
}

int printRiseAndSet(char *city, float latitude, float longitude, int UTCOffset, int day, int month, int year, char *ref)
{
  Ephemeris::setLocationOnEarth(latitude, longitude);

  Serial.print(city);
  Serial.print(" (UTC");

  if ( UTCOffset >= 0 )
  {
    Serial.print("+");
  }
  Serial.print(UTCOffset +1);  // a modifier : enlever +1 si hiver
  Serial.print(")");
  Serial.println(":");

  SolarSystemObject sun = Ephemeris::solarSystemObjectAtDateAndTime(Sun,
                          day, month, year,
                          0, 0, 0);

  // Print sunrise and sunset if available according to location on Earth
  if ( sun.riseAndSetState == RiseAndSetOk )
  {
   /* int hours, minutes;
    float seconds;
   */
    // Convert floating hours to hours, minutes, seconds and display.

    Ephemeris::floatingHoursToHoursMinutesSeconds(Ephemeris::floatingHoursWithUTCOffset(sun.rise, UTCOffset), &hours, &minutes, &seconds);
    heuresSunriseConcaten = String((hours +1)) + String(minutes);

    Serial.print("heuresSunriseConcaten: "); Serial.println(heuresSunriseConcaten);

    Serial.print("  Sunrise: ");
    Serial.print(hours +1);   // enlever +1 si hiver
    Serial.print("h");
    Serial.print(minutes);
    Serial.print("m");
    Serial.print(seconds, 0);
    Serial.println("s");

    // Convert floating hours to hours, minutes, seconds and display.
    Ephemeris::floatingHoursToHoursMinutesSeconds(Ephemeris::floatingHoursWithUTCOffset(sun.set, UTCOffset), &hours, &minutes, &seconds);
    int hours, minutes;
    float seconds;
    heuresSunsetConcaten = String((hours +1)) + String(minutes);
    
    Serial.print("heuresSunsetConcaten: "); Serial.println(heuresSunsetConcaten);

    Serial.print("  Sunset:  ");  // enlever +1 si hiver
    Serial.print(hours +1);
    Serial.print("h");
    Serial.print(minutes);
    Serial.print("m");
    Serial.print(seconds, 0);
    Serial.println("s");
  }
  else if ( sun.riseAndSetState == LocationOnEarthUnitialized )
  {
    Serial.println("You must set your location on Earth first.");
  }
  else if ( sun.riseAndSetState == ObjectAlwaysInSky )
  {
    Serial.println("Sun always in sky for your location.");
  }
  else if ( sun.riseAndSetState == ObjectNeverInSky )
  {
    Serial.println("Sun never in sky for your location.");
  }

  Serial.print("  Ref: ");
  Serial.println(ref);
  Serial.println();

  //return heuresSunriseConcaten, heuresSunsetConcaten  ;
}

void afficheHeureDate() {

  timeinfo = localtime(&maintenant);

  Serial.print("Heure:   ");
  if ((timeinfo->tm_hour ) < 10) {
    Serial.print("0");
  }
  Serial.print(timeinfo->tm_hour );  // heure entre 0 et 23
  Serial.print(":");

  if (timeinfo->tm_min < 10) {
    Serial.print("0");
  }
  Serial.print(timeinfo->tm_min);   // timeinfo->tm_min: minutes (0 - 59)
  Serial.print(":");

  if (timeinfo->tm_sec < 10) {
    Serial.print("0");
  }
  Serial.print(timeinfo->tm_sec);   // timeinfo->tm_sec: secondes (0 - 60)


  Serial.print("        Date:    ");

  if (timeinfo->tm_mday < 10) {
    Serial.print("0");
    day = 0;
  }
  Serial.print(timeinfo->tm_mday);  // timeinfo->tm_mday: jour du mois (1 - 31)
  Serial.print("-");
  day = day + timeinfo->tm_mday;

  if ((timeinfo->tm_mon + 1) < 10) { //+1
    Serial.print("0");
    month = 0;
  }

  Serial.print(timeinfo->tm_mon + 1);    // timeinfo->tm_mon: mois (0 - 11, 0 correspond à janvier)
  Serial.print("-");
  Serial.println(timeinfo->tm_year + 1900);  // timeinfo->tm_year: tm_year nombre d'années écoulées depuis 1900
  month = month + timeinfo->tm_mon +1;
  year = timeinfo->tm_year + 1900;

}
WiFiClient espClient;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);
int start = millis();
long int _now = 0;

// To send Email using Gmail use port 465 (SSL) and SMTP Server smtp.gmail.com
// YOU MUST ENABLE less secure app option https://myaccount.google.com/lesssecureapps?pli=1
#define emailSenderAccount    ""    
#define emailSenderPassword   ""
#define emailRecipient        ""  
#define smtpServer            "smtp.bbox.fr"
#define smtpServerPort        465  //465 //587 //465
#define emailSubject          "Photo de L'ESP32-CAM"
SMTPData smtpData;

// Pin definition for CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
#define TIME_TO_SLEEP  10            //time ESP32 will go to sleep (in seconds)
#define uS_TO_S_FACTOR 1000000ULL   //conversion factor for micro seconds to seconds */
int pictureNumber = 0;

// Callback function to get the Email sending status
void sendCallback(SendStatus msg) 
{
  // Print the current status
  Serial.println(msg.info());

  // Do something when complete
  if (msg.success()) {
    Serial.println("----------------");
  }
}


void photo()
 {
  for (byte i=0; i<60; i++)
  {
    //Serial.println(i);
    delay(1000);
  }  
  //****** debut de boucle***************************************  
    camera_fb_t * fb = NULL;
    
    // Take Picture with Camera
    fb = esp_camera_fb_get();  
    if(!fb) {
      Serial.println("Camera capture failed");
      return;
    }
  String path = "/Sight.jpg";
    File file = SPIFFS.open(path, FILE_WRITE);
    // Path where new picture will be saved in SD Card
    


    if(!file){
      Serial.println("Failed to open file in writing mode");
    } 
    else {
      file.write(fb->buf, fb->len); // payload (image), payload length
      Serial.printf("Saved file to path: %s\n", path);
  //    EEPROM.write(0, pictureNumber);      // Not used 
  //    EEPROM.commit();
    }
    file.close();
    esp_camera_fb_return(fb); 
    
    // Turns off the ESP32-CAM white on-board LED (flash) connected to GPIO 4
    pinMode(4, OUTPUT);
    digitalWrite(4, LOW);
    rtc_gpio_hold_en(GPIO_NUM_4);
  //*************************************************************************************************************************

    Serial.print("Connecting");

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(200);
    }

    Serial.println();
    Serial.println("WiFi connected.");
    Serial.println();
    Serial.println("Preparing to send email");
    Serial.println();

    
    // Set the SMTP Server Email host, port, account and password
    smtpData.setLogin(smtpServer, smtpServerPort, emailSenderAccount, emailSenderPassword);

    // For library version 1.2.0 and later which STARTTLS protocol was supported,the STARTTLS will be 
    // enabled automatically when port 587 was used, or enable it manually using setSTARTTLS function.
    //smtpData.setSTARTTLS(true);

    // Set the sender name and Email
    smtpData.setSender("Cat_00", emailSenderAccount);

    // Set Email priority or importance High, Normal, Low or 1 to 5 (1 is highest)
    smtpData.setPriority("High");

    // Set the subject
    smtpData.setSubject(emailSubject);

    // Set the message with HTML format
    smtpData.setMessage("<div style=\"color:#2f4468;\"><h1>Find a thing !</h1><p>- Sent from ESP32 board</p></div>", true);
    // Set the email message in text format (raw)
    //smtpData.setMessage("Thing on position #00", false);

    // Add recipients, you can add more than one recipient
    smtpData.addRecipient(emailRecipient);
    //smtpData.addRecipient("YOUR_OTHER_RECIPIENT_EMAIL_ADDRESS@EXAMPLE.com");
  smtpData.setFileStorageType(MailClientStorageType::SPIFFS);
    smtpData.addAttachFile("/Sight.jpg");
  
    //smtpData.setFileStorageType(MailClientStorageType::SD);
    
    smtpData.setSendCallback(sendCallback);

  

    //Start sending Email, can be set callback function to track the status
    if (!MailClient.sendMail(smtpData))
      Serial.println("Error sending Email, " + MailClient.smtpErrorReason());

    //Clear all data from Email object to free memory
    smtpData.empty();
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
}
void setup() {
  Serial.begin(115200);
  
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
 
  WiFi.mode(WIFI_STA);

  Serial.println();

  WiFi.begin(ssid, password);
  Serial.print("Connexion au reseau WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println();

  
  configTime(decalage * 3600, 0, "fr.pool.ntp.org");  //serveurs canadiens
       // en Europe, essayez europe.pool.ntp.org ou fr.pool.ntp.org
  
  Serial.print("Attente date et heure");

  while (time(nullptr) <= 100000) {
    Serial.print(".");
    delay(1000);
  }

  time(&maintenant);

  Serial.println("");
  
  afficheHeureDate();

  Serial.print("SunRise/SunSet at ");
    printDate(day, month, year);
    Serial.println("");
    Serial.print("Date: "); Serial.println(day);
    Serial.println(":\n");

    //               CITY         LATITUDE    LONGITUDE     TZ   DATE             REFERENCE
    printRiseAndSet("Caudry",    50.1258333,    3.4051032,  +1,  day, month, year, "sunearthtools (10/2/2017): SunRise: 08:07:00 | SunSet: 18:03:19");


  // Démarrage du client NTP - Start NTP client
  timeClient.begin();
  timeClient.update();
  Serial.println(timeClient.getFormattedTime());
  delay(1 * 1000);  // Attendre 1s 

  File f = SPIFFS.open("/lastEvent.txt", "r"); 
  int lastEvent = f.readStringUntil('n').toInt();
  Serial.println(lastEvent);
  f.close();

  f = SPIFFS.open("/lastEvent.txt", "w");
  f.println(_now); 
  f.close();

 delay(500);
 Serial.println("WELCOME");
if (!SPIFFS.begin(true)) {
  Serial.println("An Error has occurred while mounting SPIFFS");
  ESP.restart();
}
else {
  Serial.println("SPIFFS mounted successfully");
}
SPIFFS.format();

 
   EEPROM.begin(400);
   
 
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG; 

//gpio_pullup_en(GPIO_NUM_12);  // ajout
// Pin(14, Pin.IN, Pin.PULL_UP)

  
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA; // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  // Init Camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }


  Serial.print("timeClient.getHours(): "); Serial.println(((timeClient.getHours())+1) & timeClient.getMinutes());
  Serial.print("heuresSunriseConcaten: "); Serial.println(heuresSunriseConcaten);
  Serial.print("timeClient.getHours(): "); Serial.println(((timeClient.getHours())+1) & timeClient.getMinutes());
  Serial.print("heuresSunsetConcaten: "); Serial.println(heuresSunsetConcaten);  

  //if((timeClient.getHours())+1 >= int(heuresSunriseConcaten))
  //{ 
    photo();
  //}
  //Serial.println("Il n'est pas encore l'heure ne prendre des photos");
}
void loop() {

  // est-ce le moment de demander l'heure NTP?
  if ((millis() - derniereDemande) >=  delaiDemande * 1000 ) {
    time(&maintenant);
    derniereDemande = millis();

    Serial.println("Interrogation du serveur NTP");
  }

  // est-ce que millis() a débordé?
  if (millis() < derniereDemande ) {
    time(&maintenant);
    derniereDemande = millis();
  }

  // est-ce le moment de raffraichir la date indiquée?
  if ((millis() - derniereMaJ) >=   1000 ) {
    maintenant = maintenant + 1;
    //afficheHeureDate();
    derniereMaJ = millis();
  }

  
}
