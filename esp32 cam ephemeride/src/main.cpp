/***********************************************************************************
   Horloge connectée à base d'ESP8266 ou d'ESP32
   Affichage de l'heure et de la date dans le moniteur série.
   L'heure est obtenue grâce à un serveur NTP
   
   https://electroniqueamateur.blogspot.com/2018/10/horloge-wi-fi-esp8266.html
   
*************************************************************************************/

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
#include "BluetoothSerial.h"

// Pour l'heure du reseau
#include <NTPClient.h>
#include <WiFiUdp.h>

//#include "wdt.h"

const char* ssid = "Bbox-E9ED9E75-pro-2.4G";  //"Bbox-E9ED9E75-pro-2.4G";
const char* password = "Vivimimi123456789"; //"Vivimimi123456789";

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

BluetoothSerial SerialBT;

#include <Ephemeris.h>

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
String Smonth = "";
String Smin = "";
String minenforme = "";

WiFiClient espClient;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);
int start = millis();
long int _now = 0;

// To send Email using Gmail use port 465 (SSL) and SMTP Server smtp.gmail.com
// YOU MUST ENABLE less secure app option https://myaccount.google.com/lesssecureapps?pli=1
#define emailSenderAccount    "michael.marchessoux@bbox.fr"    
#define emailSenderPassword   "vivi2909"
#define emailRecipient        "mikl-dev@bbox.fr"  
#define smtpServer            "smtp.bbox.fr"
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
     
   //****** debut de boucle***************************************  
    camera_fb_t * fb = NULL;
    
    // Take Picture with Camera
    fb = esp_camera_fb_get();  
    if(!fb) {
      Serial.println("Camera capture failed");
      return;
    }
    // Impossible de changer le nom du fichier car day, Smonth et year sont defini bien apres
    
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

    Serial.println("Preparing to send email");
    Serial.println();

    
    // Set the SMTP Server Email host, port, account and password
    smtpData.setLogin(smtpServer, smtpServerPort, emailSenderAccount, emailSenderPassword);

    // For library version 1.2.0 and later which STARTTLS protocol was supported,the STARTTLS will be 
    // enabled automatically when port 587 was used, or enable it manually using setSTARTTLS function.
    //smtpData.setSTARTTLS(true);

    // Set the sender name and Email
    if (timeinfo->tm_mon +1 < 10) 
    {
      Smonth = "0" + String(timeinfo->tm_mon +1);
      //Serial.print("mois: "); Serial.println(Smonth);
    }
    else
    {
      Smonth = String(timeinfo->tm_mon +1);
    }
    if (timeinfo->tm_min < 10) 
    {
      Smin ="0" + String(timeinfo->tm_min);
    }
    else
    {
      Smin = String(timeinfo->tm_min);
    }
    Serial.print("Date pour le mail: "); Serial.print(day); Serial.print("/"); Serial.print(Smonth); Serial.print("/"); Serial.println(year);
    String message = String(day) + String(Smonth) + String(year) + "    " + String(timeinfo->tm_hour) + String(Smin);
    
    smtpData.setSender(message, emailSenderAccount);
    //smtpData.setSender("Cat_00", emailSenderAccount);    --------------------------> original

    // Set Email priority or importance High, Normal, Low or 1 to 5 (1 is highest)
    smtpData.setPriority("Normal");

    // Set the subject
    smtpData.setSubject(emailSubject);

    // Set the message with HTML format
    smtpData.setMessage("<div style=\"color:#2f4468;\"><h1>Voila la photo !!</h1><p>- Envoyer depuis l'ESP32-CAM</p></div>", true);
    // Set the email message in text format (raw)
    //smtpData.setMessage("Thing on position #00", false);

    // Add recipients, you can add more than one recipient
    smtpData.addRecipient(emailRecipient);
    //smtpData.addRecipient("YOUR_OTHER_RECIPIENT_EMAIL_ADDRESS@EXAMPLE.com");
    smtpData.setFileStorageType(MailClientStorageType::SPIFFS);


    //String nomFichier = String(day) + String(Smonth) + String(year) + "    " + String(hours) + String(minutes);
    //Serial.print("Date pour la photo: "); Serial.println(nomFichier);
    //smtpData.addAttachFile("/" + nomFichier + ".jpg");
    smtpData.addAttachFile("/Sight.jpg");    //-----> original
  
    //smtpData.setFileStorageType(MailClientStorageType::SD);
    
    smtpData.setSendCallback(sendCallback);

  

    //Start sending Email, can be set callback function to track the status
    if (!MailClient.sendMail(smtpData))
      Serial.println("Error sending Email, " + MailClient.smtpErrorReason());

    //Clear all data from Email object to free memory
    smtpData.empty();

    for (int i=0; i<1800; i++)    
    {
      Serial.println(i);
      delay(1000);
    } 



    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    esp_deep_sleep_start();

    //software_Reboot()
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
  Serial.println(timeinfo->tm_sec);   // timeinfo->tm_sec: secondes (0 - 60)


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





  if (timeinfo->tm_mday < 10) {
    //Serial.print("0");
    day = 0;
    day = day + timeinfo->tm_mday;      // <---- a revoir   concatenation
  }
  else {
    day = timeinfo->tm_mday;
  }

  //Serial.print(timeinfo->tm_mday);  // timeinfo->tm_mday: jour du mois (1 - 31)
  //Serial.print("-");

  if ((timeinfo->tm_mon + 1) < 10) {
    //Serial.print("0");
    month = 0;
  }

  //Serial.print(timeinfo->tm_mon + 1);    // timeinfo->tm_mon: mois (0 - 11, 0 correspond à janvier)
  //Serial.print("-");
  //Serial.println(timeinfo->tm_year + 1900);  // timeinfo->tm_year: tm_year nombre d'années écoulées depuis 1900
  month = month + timeinfo->tm_mon +1;
  year = timeinfo->tm_year + 1900;
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
    Serial.print("ConcHeureCouche: "); Serial.println(minenforme);
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

    if (minutes < 10) 
    {
      String minutes = "0";
      minenforme = minutes + String(minutes);      // <---- a revoir   concatenation
    }
    else
    {
      minenforme = String(minutes);
    }
  
  ConcNow = String(timeinfo->tm_hour) + minenforme;
  Serial.print("ConcNow: "); Serial.println(ConcNow);

  //      2017                    536                    2017                 2159 
  while (ConcNow.toInt() >= ConcHeureLeve.toInt() && ConcNow.toInt() <= ConcHeureCouche.toInt())      //  nest pas < ou > car ce sont des chaines de caracteres   ! 
  {
    Serial.println("Je suis dans la boucle et je prends une photo.");
    photo();
  }
    
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
  //*****************************************************************************

}		
void setup() 
{
  Serial.begin(115200);
 
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

 
  //SerialBT.begin("ESP32");
  //delay(500);
  //SerialBT.print("Module Bluetooth Initialisé.");

  WiFi.mode(WIFI_STA);
  Serial.println();
  WiFi.begin(ssid, password);
  Serial.println("Connexion au reseau WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  SerialBT.print("ESP32 connecté au WIFI.");
  Serial.println();

  configTime(decalage * 3600, 0, "fr.pool.ntp.org");  //ca.pool.ntp.org serveurs canadiens
       // en Europe, essayez europe.pool.ntp.org ou fr.pool.ntp.org
  
  Serial.print("Attente date et heure");

  while (time(nullptr) <= 100000) {
    Serial.print(".");
    delay(1000);
  }

  time(&maintenant);
  Serial.println("");
  Serial.print("Maintenant: ");
  Serial.println(time(&maintenant));

  afficheHeureDate();
  
  // Affichage sur le BT 
  SerialBT.print("Date: ");
  SerialBT.print(day);
  SerialBT.print("/");
  SerialBT.print(month);
  SerialBT.print("/");
  SerialBT.println(year);

  SerialBT.print("Heure:   ");
  if ((timeinfo->tm_hour ) < 10) {
    SerialBT.print("0");
  }
  SerialBT.print(timeinfo->tm_hour );  // heure entre 0 et 23
  SerialBT.print(":");

  if (timeinfo->tm_min < 10) {
    SerialBT.print("0");
  }
  SerialBT.print(timeinfo->tm_min);   // timeinfo->tm_min: minutes (0 - 59)
  SerialBT.print(":");

  if (timeinfo->tm_sec < 10) {
    SerialBT.print("0");
  }
  SerialBT.println(timeinfo->tm_sec);   // timeinfo->tm_sec: secondes (0 - 60)


  //int day=27,month=5,year=2021;

  Serial.print("SunRise/SunSet at ");
  printDate(day,month,year);
  Serial.print(":\n");

  //En France métropolitaine :
  //1: ete    Passage de l'heure d'hiver à l'heure d'été le dernier dimanche de mars à 1h00 UTC (à 2h00 locales il est 3h00)
  //0: hiver  Passage de l'heure d'été à l'heure d'hiver le dernier dimanche d'octobre à 1h00 UTC (à 3h00 locales il est 2h00)


  // pour essai H hiver / ete
  //day = 01;
  //month = 2;
  //year = 2021;

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
  resultUTC = MARS < month && month < OCTOBRE;   //  -> ajouter le resultat a UTC ((utc+1) + return ou bien ajouter a lever et coucher 
  //	  leve conc < now conc  &&   now conc  <  couche conc     -> si 0-> pas de photos, Si 1> photo
  
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

  
   // changer TZ du tableau  +1 ou +2 selon ete ou hiver a confirmer avec greg

  //               CITY         LATITUDE    LONGITUDE     TZ   DATE             REFERENCE
  printRiseAndSet("Caudry",    50.1258333,    3.4051032, 1 + hiver_ete,  day, month, year, "sunearthtools (10/2/2017): SunRise: 08:07:00 | SunSet: 18:03:19");
}
void loop() 
{ 
}
