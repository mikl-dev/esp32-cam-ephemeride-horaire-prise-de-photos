/***********************************************************************************
   Horloge connectée à base d'ESP8266 ou d'ESP32
   Affichage de l'heure et de la date dans le moniteur série.
   L'heure est obtenue grâce à un serveur NTP
   
   https://electroniqueamateur.blogspot.com/2018/10/horloge-wi-fi-esp8266.html
*************************************************************************************/   

//********************* Declaration des includes *******************************
#if defined ARDUINO_ARCH_ESP8266  // s'il s'agit d'un ESP8266
#include <ESP8266WiFi.h>
#elif defined ARDUINO_ARCH_ESP32  // s'il s'agit d'un ESP32
#include "WiFi.h"
#endif

#include <Arduino.h>

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
#include <Ephemeris.h>
// Pour l'heure du reseau
#include <NTPClient.h>
#include <WiFiUdp.h>
//*******************************************************************************
//************* Declaration des variables *******************************/
const char* ssid = "";  
const char* password = ""; 

//FSInfo fsInfo;

const int decalage = 2;  // la valeur dépend de votre fuseau horaire. Essayez 2 pour la France. 
const int delaiDemande = 5 * 60; // nombre de secondes entre deux demandes consécutives au serveur NTP

unsigned long derniereDemande = millis(); // moment de la plus récente demande au serveur NTP
unsigned long derniereMaJ = millis(); // moment de la plus récente mise à jour de l'affichage de l'heure
time_t maintenant;
struct tm * timeinfo;
int day = 0;
int month = 0;
int year = 0;
int hours, minutes;
float seconds;
String heuresSunriseConcaten;
String heuresSunsetConcaten;
bool heureEte;
bool heureHiver;
bool rinter;
bool result; 
bool resultUTC;
const byte MARS = 3;
const byte OCTOBRE = 10;
String ConcHeureLeve = "";
String ConcHeureCouche = "";
String ConcNow = "";
String Sday = "";
String Smonth = "";
String Syear = "";
String Sdate = "";
String Sheure = "";
String Smin = "";
String Ssec = "";
String heureenforme = "";
String minenforme = "";
String Hete = "";
int address = 0;
int boardId = 0;



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
#define smtpServer            ""
#define smtpServerPort         465  //465 //587 //465
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
//**********************************************************************
//******************** Programmes apellés ******************************
void Interroge_SPIFFS()
{
  Serial.print("SPIFFS.totalBytes: "); Serial.println(SPIFFS.totalBytes());
  Serial.print("SPIFFS.usedBytes: "); Serial.println(SPIFFS.usedBytes());
}
void RecupereHeureDate()
{
//*** chercher l'heure sur le reseau  a chaque debut de boucle***
//ca.pool.ntp.org serveurs canadiens. En Europe, essayez europe.pool.ntp.org ou fr.pool.ntp.org
configTime(decalage * 3600, 0, "fr.pool.ntp.org");  

while (time(nullptr) <= 100000) 
{
  Serial.print(".");
  delay(1000);
}
Serial.println("");

time(&maintenant);
//Serial.print("time(&maintenant): "); Serial.println(time(&maintenant));

//*** Mise en forme de la date et heure
timeinfo = localtime(&maintenant);

//*** Mise en forme de l'heure
  if ((timeinfo->tm_hour ) < 10) 
  {
    Sheure = "0" + String(timeinfo->tm_hour);
  }
  else{
    Sheure = String(timeinfo->tm_hour);
  }

  if (timeinfo->tm_min < 10) 
  {
    Smin = "0" + String(timeinfo->tm_min);
  }
  else
  {
    Smin = String(timeinfo->tm_min);
  }

  if (timeinfo->tm_sec < 10) 
  {
    Ssec = "0" + timeinfo->tm_sec;
  }
  else
  {
    Ssec = String(timeinfo->tm_sec);
  }

Serial.print("heure: "); Serial.print(Sheure); Serial.print(":"); Serial.print(Smin); Serial.print(":"); Serial.println(Ssec);

//*** Mise en forme de la date
  if (timeinfo->tm_mday < 10) 
  {
    Sday = "0" + String(timeinfo->tm_mday);      // <---- a revoir   concatenation
    day = Sday.toInt();
  }
  else 
  {
    Sday = String(timeinfo->tm_mday);
    day = Sday.toInt();
  }
  if (timeinfo->tm_mon + 1 < 10) 
  {
    Smonth = "0" + String(timeinfo->tm_mon + 1);      // <---- a revoir   concatenation
    month = Smonth.toInt();
  }
  else 
  {
    Smonth = String(timeinfo->tm_mon + 1);
    month = Smonth.toInt();
  }

  //Serial.print("controle mois: "); Serial.print(Smonth);

  year = timeinfo->tm_year + 1900;
  Syear = String(year);

Serial.print("Date: "); Serial.print(Sday); Serial.print("/"); Serial.print(Smonth); Serial.print("/"); Serial.println(Syear);

if (month == MARS)
  {
    uint8_t dernierDimancheMars = 31 - ((5 + year + (year >> 2)) % 7); 
    rinter = (day == dernierDimancheMars && day != 0);        
    result = day > dernierDimancheMars || rinter;
  }												
if (month == OCTOBRE)						
  {
    uint8_t dernierDimancheOctobre = 31 - ((2 + year + (year >> 2)) % 7);
    rinter = (day == dernierDimancheOctobre && day == 0);
    result = day < dernierDimancheOctobre || rinter;
  } 
  resultUTC = MARS < month && month < OCTOBRE;   

  // definir si on est en H hivers ou ete
  if (resultUTC == 0)
  {
    heureHiver = 1;
    heureEte = !heureHiver;
  }
  else if (resultUTC == 1)
  {
    heureEte = 1;
    heureHiver = !heureEte;
  }

  //month = month + timeinfo->tm_mon +1;
  //year = timeinfo->tm_year + 1900;

}
void sendCallback(SendStatus msg) 
{
  // Print the current status
  Serial.println(msg.info());

  // Do something when complete
  if (msg.success()) {
    Serial.println("----------------");
  }
}
void printDate(int day, int month, int year)
{
  Serial.print(day);
  Serial.print("/");
  Serial.print(month);
  Serial.print("/");
  Serial.println(year);
}
void printRiseAndSet(char *city, FLOAT latitude, FLOAT longitude, int UTCOffset, int day, int month, int year, char *ref)
{
  //Serial.print(String(day)); Serial.print(" " + String(month)); ; Serial.println(" " + String(year));
  Ephemeris::setLocationOnEarth(latitude,longitude);
    
  Serial.print(city);
  Serial.print(" (UTC");
  if( UTCOffset >= 0 )
  {
    Serial.print("+");
  }
  Serial.print(UTCOffset);  
  Serial.print(")");
  Serial.println(":");
           
  SolarSystemObject sun = Ephemeris::solarSystemObjectAtDateAndTime(Sun,
                                                                    day,month,year,
                                                                    0,0,0);

    // Print sunrise and sunset if available according to location on Earth
  if( sun.riseAndSetState == RiseAndSetOk )
  {
    int hours,minutes;
    FLOAT seconds;

    // Convert floating hours to hours, minutes, seconds and display.
    Ephemeris::floatingHoursToHoursMinutesSeconds(Ephemeris::floatingHoursWithUTCOffset(sun.rise,UTCOffset), &hours, &minutes, &seconds);
    if (minutes < 10) 
    {
      String minutes = "0";
      minenforme = minutes + String(minutes);      // <---- a revoir   concatenation
    }
    else
    {
      minenforme = String(minutes);
    }
    
    Serial.print("  Sunrise: ");
    Serial.print(hours);
    Serial.print("h");
    Serial.print(minenforme);
    Serial.print("m");
    Serial.print(seconds,0);
    Serial.println("s");

    ConcHeureLeve = String(hours) + String(minutes);
    Serial.print("ConcHeureLeve: "); Serial.println(ConcHeureLeve);


    // Convert floating hours to hours, minutes, seconds and display.
    Ephemeris::floatingHoursToHoursMinutesSeconds(Ephemeris::floatingHoursWithUTCOffset(sun.set,UTCOffset), &hours, &minutes, &seconds);
    if (minutes < 10) 
    {
      String minutes = "0";
      minenforme = minutes + String(minutes);      // <---- a revoir   concatenation
    }
    else
    {
      minenforme = String(minutes);
    }
    
    
    Serial.print("  Sunset:  ");
    Serial.print(hours);
    Serial.print("h");
    Serial.print(minenforme);
    Serial.print("m");
    Serial.print(seconds,0);
    Serial.println("s");

    ConcHeureCouche = String(hours) + String(minenforme);
    Serial.print("ConcHeureCouche: "); Serial.println(ConcHeureCouche);
  }
  else if( sun.riseAndSetState == LocationOnEarthUnitialized )
  {
    Serial.println("You must set your location on Earth first.");
  }
  else if( sun.riseAndSetState == ObjectAlwaysInSky )
  {
    Serial.println("Sun always in sky for your location.");
  }
  else if( sun.riseAndSetState == ObjectNeverInSky )
  {
    Serial.println("Sun never in sky for your location.");
  }

  // *************** Photo si l'heure est comprise dans l'intervalle levée/couché

    if (timeinfo->tm_min < 10) 
    {
      String minutes = "0";
      minenforme = minutes + String(timeinfo->tm_min);      // <---- a revoir   concatenation
    }
    else
    {
      minenforme = String(timeinfo->tm_min);
    }
  
  ConcNow = String(timeinfo->tm_hour) + String(minenforme);
  Serial.print("ConcNow: "); Serial.println(ConcNow);
  
  //esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  //esp_deep_sleep_start();
  //*****************************************************************************
}		
void photo()
 {
     
   //****** debut de boucle***************************************  
    camera_fb_t * fb = NULL;
    
    // Take Picture with Camera
    fb = esp_camera_fb_get();  
    if(!fb) {
      Serial.println("Camera capture failed");
      Interroge_SPIFFS();
      EEPROM.write(address, boardId);
      ESP.restart();
      return;
    }

    String nomImage = "/" + Sday + Smonth + Syear + "  " + Sheure + Smin + ".jpg";
    Serial.print(nomImage);
    String path = nomImage;
    //String path = "/Sight.jpg";
    File file = SPIFFS.open(path, FILE_WRITE);
    // Path where new picture will be saved in SD Card
    


    if(!file){
      Serial.println("Failed to open file in writing mode");
    } 
    else {
      file.write(fb->buf, fb->len); // payload (image), payload length
      //Serial.println(path);
      //Serial.printf("Saved file to path: %s\n", path);
  //    EEPROM.write(0, pictureNumber);      // Not used 
  //    EEPROM.commit();
    }
    file.close();
    esp_camera_fb_return(fb); 

    if (SPIFFS.usedBytes() <= 50000)
    {
      board += 1;
      EEPROM.write(address, boardId);
      ESP.restart();
    }
    
    // Turns off the ESP32-CAM white on-board LED (flash) connected to GPIO 4
    pinMode(4, OUTPUT);
    digitalWrite(4, LOW);
    rtc_gpio_hold_en(GPIO_NUM_4);
  //*************************************************************************************************************************

    Serial.println("Preparing to send email");
    Serial.println();

    
    // Set the SMTP Server Email host, port, account and password
    smtpData.setLogin(smtpServer, smtpServerPort, emailSenderAccount, emailSenderPassword);

    // For library version 1.2.0 and later which STARTTLS protocol was supported,the STARTTLS will be 
    // enabled automatically when port 587 was used, or enable it manually using setSTARTTLS function.
    //smtpData.setSTARTTLS(true);

    // Set the sender name and Email
    
    //Serial.print("Date pour le mail: "); Serial.print(day); Serial.print("/"); Serial.print(Smonth); Serial.print("/"); Serial.println(year);
    String message = String(day) + String(Smonth) + String(year) + "    " + String(timeinfo->tm_hour) + String(Smin);
    
    smtpData.setSender(message, emailSenderAccount);
    //smtpData.setSender("Cat_00", emailSenderAccount);    --------------------------> original

    // Set Email priority or importance High, Normal, Low or 1 to 5 (1 is highest)
    smtpData.setPriority("Normal");

    // Set the subject
    smtpData.setSubject(emailSubject);

    EEPROM.write(address, boardId);

    if (heureEte == 1)
    {
      Hete = "heure d'été.";
    }
    else
    {
      Hete = "heure d'hiver.";
    }
    
    // Set the message with HTML format
    String contenueMail = "Bytes utilisés dans le SPIFFS: " + String(SPIFFS.usedBytes()) + "." + "<br>" \
                          "Puissance du signal WIFI: " + String(WiFi.RSSI()) + "<br>" \
                          "Heure de levé concatenée: " + ConcHeureLeve + "<br>" \
                          "Heure de maintenant concaténée: " + ConcNow + "<br>" \
                          "Heure du couché concatenée: " + ConcHeureCouche + "<br>" \
                          "Nous sommes en " + Hete + "<br>" \                     
                          "L'ESP a redémaré " + String(EEPROM.read(address)) + " fois." + "<br>";  //"<div><h1> Récapitulatif des variables </h1></div>" + "<br>" 
                          // ajouter la date du dernier restart
                          
    smtpData.setMessage(contenueMail, true); //"<div style=\"color:#2f4468;\"><h1>Voila la photo !!</h1><p>- Envoyer depuis l'ESP32-CAM</p></div>", true);
    // Set the email message in text format (raw)
    //smtpData.setMessage("Thing on position #00", false);

    // Add recipients, you can add more than one recipient
    smtpData.addRecipient(emailRecipient);
    //smtpData.addRecipient("YOUR_OTHER_RECIPIENT_EMAIL_ADDRESS@EXAMPLE.com");
    smtpData.setFileStorageType(MailClientStorageType::SPIFFS);


    //String nomFichier = String(day) + String(Smonth) + String(year) + "    " + String(hours) + String(minutes);
    //Serial.print("Date pour la photo: "); Serial.println(nomFichier);
    //smtpData.addAttachFile("/" + nomFichier + ".jpg");
    //smtpData.addAttachFile("/Sight.jpg");    //-----> original
    smtpData.addAttachFile(nomImage);
    //smtpData.setFileStorageType(MailClientStorageType::SD);
    
    smtpData.setSendCallback(sendCallback);

  

    //Start sending Email, can be set callback function to track the status
    if (!MailClient.sendMail(smtpData))
      Serial.println("Error sending Email, " + MailClient.smtpErrorReason());

    //Clear all data from Email object to free memory
    smtpData.empty();

    Interroge_SPIFFS();
    SPIFFS.remove(nomImage);
    Serial.print("Fichier effacé: "); Serial.println(SPIFFS.remove(nomImage));

    Interroge_SPIFFS();

    for (int i=0; i<1800; i++)    
    {
    //  Serial.println(i);
      delay(1000);
    } 

}
void setup() 
{
  Serial.begin(115200);
 
  //*** Initialisation du SPIFF ***
  Serial.println("WELCOME");
  if (!SPIFFS.begin(true)) 
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    boardId += 1;
    EEPROM.write(address, boardId);
    ESP.restart();
  }
  else 
  {
    Serial.println("SPIFFS mounted successfully");
  }
  SPIFFS.format();

  //*** Initialisation de l'EEPROM ***
  EEPROM.begin(400);

  //*** Config camera ***
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
  
  //*** Config de la PSRAM pour la photo ***
  if(psramFound())
  {
    config.frame_size = FRAMESIZE_UXGA; // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } 
  else 
  {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

//*** Initialisation de la camera ***
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) 
  {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

//*** Initialisation et connexion au WIFI ***
WiFi.mode(WIFI_STA);
Serial.println();
WiFi.begin(ssid, password);
Serial.print("Connexion au reseau WiFi");

while (WiFi.status() != WL_CONNECTED) 
{
  Serial.print(".");
  delay(500);
}
Serial.println();
//timeClient.begin();
}
void loop() 
{
//*** si le wifi est déconnecté alors redemarre l'ESP
if (WiFi.status() != WL_CONNECTED) 
{
  boardId += 1;
  EEPROM.write(address, boardId);
  ESP.restart();
}
//*** Recupere la date et l'heure du reseau et met en forme
RecupereHeureDate();

//*** Affiche la date pour le calcul de l'ephemeride
  Serial.print("SunRise/SunSet at: ");
  printDate(day,month,year);
  Serial.print(":\n");

//*** definir si on est en H hivers ou ete
  if (month == MARS)
  {
    uint8_t dernierDimancheMars = 31 - ((5 + year + (year >> 2)) % 7); 
    rinter = (day == dernierDimancheMars && day != 0);        
    result = day > dernierDimancheMars || rinter;
  }												
  if (month == OCTOBRE)						
  {
    uint8_t dernierDimancheOctobre = 31 - ((2 + year + (year >> 2)) % 7);
    rinter = (day == dernierDimancheOctobre && day == 0);
    result = day < dernierDimancheOctobre || rinter;
  } 
  resultUTC = MARS < month && month < OCTOBRE;
  
  if (resultUTC == 0)
  {
    heureHiver = 1;
    heureEte = !heureHiver;
  }
  else if (resultUTC == 1)
  {
    heureEte = 1;
    heureHiver = !heureEte;
  }
  Serial.print("heureEte: ");Serial.println(heureEte);
  Serial.print("heureHiver: ");Serial.println(heureHiver);

  // ajouter 1 heure en fonction
  byte hiver_ete = 1;
  if (heureEte == 1)
  {
    hiver_ete = 1;
  }
  else
  {
    hiver_ete = 0;
  }
//Serial.print("avant de rentrer dans riseandset: "); Serial.print(String(day)); Serial.print(" " + String(month)); ; Serial.println(" " + String(year));
//*** Appel du programme ephemeride ***
  //               CITY         LATITUDE    LONGITUDE     TZ   DATE             REFERENCE
  printRiseAndSet("Caudry",    50.1258333,    3.4051032, 1 + hiver_ete,  day, month, year, "sunearthtools (10/2/2017): SunRise: 08:07:00 | SunSet: 18:03:19");

//*** photo si l'heure est situé entre levé et couché
if (ConcNow.toInt() >= ConcHeureLeve.toInt() && ConcNow.toInt() <= ConcHeureCouche.toInt())      //  nest pas < ou > car ce sont des chaines de caracteres   ! 
  {
    Serial.println("Je suis dans la boucle et je prends une photo.");
    Serial.print("concnow dans la boucle: "); Serial.println(ConcNow);
    photo();
  }
}
