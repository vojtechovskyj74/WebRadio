
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>
#include <WiFiMulti.h> 
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Fonts/FreeSans9pt7b.h>
#include "time.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Preferences.h> 
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include "Audio.h"
#include <WebServer.h>
#include <WebSocketsServer.h> 
#include "logos.h"
#include <ESPmDNS.h>

WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81); 

WiFiMulti wifiMulti;
Audio audio;
Preferences preferences;

// =================== 1. KONFIGURACE PINŮ PRO ESP32-S3 N16R8 ===================
#define I2S_BCLK      6  
#define I2S_LRC       7  
#define I2S_DOUT      9

#define TFT_SCL       18  
#define TFT_SDA       15  
#define TFT_CS         5  
#define TFT_RST        4  
#define TFT_DC        16  
#define TFT_BL        17        

// --- PŘIDÁNO PRO DRUHÝ DISPLEJ (VIZUALIZÉR) ---
//      TFT2_SCL      18  
//      TFT2_SDA      15  
#define TFT2_CS       10
#define TFT2_DC       11
#define TFT2_RST      12
#define TFT2_BL       14 

#define BUTTON_NEXT    1  
#define BUTTON_PREV    2  
#define BATTERY_PIN    8  // (dělič 10k)
#define CHARGE_PIN     3  // (dělič 10k)
#define LDR_PIN       13  // Fotoodpor (dělič 2k2)

#define PIN_TLACITKO_BT  38  // Bt on/off  proti GND
#define PIN_MAX_NAPAJENI 39  // Ovládá rele (Prepina napeti vcc pro MAX/BT)

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
Adafruit_ST7735 tft2 = Adafruit_ST7735(TFT2_CS, TFT2_DC, TFT2_RST);

GFXcanvas16 scrollCanvas(160, 12);
GFXcanvas16 titleCanvas(160, 18);

// =================== 2. PROMĚNNÉ A NASTAVENÍ ČASU ===================
int titleScrollPos = 5;
unsigned long lastTitleScrollTime = 0;
bool needsTitleScroll = false;
const char* svatekUrl = "https://svatkyapi.cz/api/day";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;
String svatek = "N/A";
String scrollText = "";
int scrollPos = 160;
unsigned long lastScrollTime = 0;
String currentTitle = "Cekam...";
String lastDrawnTitle = "";
String currentStreamUrl = ""; 
String aktualniPisnicka = "Nacitam informace...";
int currentStation = 0;
int curVolume = 80;                
bool isSelecting = false;
unsigned long lastActionTime = 0; 
unsigned long nextPressStart = 0;
bool nextLongFired = false;
unsigned long prevPressStart = 0;
bool prevLongFired = false;
void drawScreen();
void drawTitle();
void drawVolume();
unsigned long streamConnectTime = 0; 
int lastDrawnBattery = -1; 
bool lastChargingState = false;
bool isMuted = false;
int preMuteVolume = 10;
int savedVolumeBeforeBT = -1; // PŘIDÁNO: Proměnná pro uložení hlasitosti
bool isSleepTimerActive = false;
unsigned long sleepTimerStart = 0;
unsigned long sleepTimerDuration = 0;
int currentSleepValue = 0; 
bool isRadioPlaying = true; 
int eqLow = 0;   
int eqMid = 0;   
int eqHigh = 0;
int connectedWebClients = 0; 
bool rezimSluchatka = false;     // Udržuje stav (false = hrají repra)
unsigned long posledniStisk = 0; // Proti zákmytům tlačítka (debounce)
unsigned long btPressStart = 0;
bool btLongFired = false;
bool probihaNavratZBT = false;
unsigned long casStartuNavratu = 0;
bool batWarning20Shown = false;


const String fwVersion = "10.8";  // VERZE S AUTO-PWM PODSVÍCENÍM A FIXY! BT rele Rezim ============================== VERZE =================

// =================== 3. SEZNAM RÁDIOSTANIC ===================
const char* stationNames[] = {
    "Evropa 2",           //  0
    "Kiss",               //  1       
    "Bonton",             //  2
    "Signal",             //  3
    "Fajn",               //  4
    "City",               //  5
    "CS Radio",           //  6  
    "E2 CS Hity",         //  7
    "Cesky Blanik",       //  8
    "Blanik",             //  9  
    "Cesky Impuls",       // 10  
    "Impuls",             // 11       
    "Country radio",      // 12
    "Beat",               // 13
    "Relax",              // 14
    "Radio 1",            // 15
    "Helax",              // 16   
    "Frekvence 1",        // 17
    "Exclusively PSB",    // 18 
    "M.Jackson",          // 19
    "Madona",             // 20
    "Phill Colins",       // 21 
    "Metalica",           // 22
    "Tommorowland",       // 23
    "CRo Radiozurnal",    // 24
    "CRo Dvojka",         // 25
    "CRo Vltava",         // 26
    "Radio Diana",        // 27
    "Radio Humor",        // 28 
    "Radio Povidka",      // 29       
    "Radio Pohadka"       // 30         
};

const char* stationHosts[] = {
    "playerservices.streamtheworld.com",        //  0 E2
    "icecast1.play.cz",                         //  1 Kiss
    "ice.actve.net",                            //  2 Bonton
    "icecast1.play.cz",                         //  3 Signal 
    "ice.abradio.cz",                           //  4 Fajn 
    "ice.abradio.cz",                           //  5 City
    "gw2.mysoft.cz:8000",                       //  6 CS Radio
    "ice.actve.net",                            //  7 E2 CS
    "ice.abradio.cz",                           //  8 C Blanik
    "ice.abradio.cz",                           //  9 Blanik
    "icecast6.play.cz",                         // 10 C Impuls
    "icecast1.play.cz",                         // 11 Impuls
    "icecast4.play.cz",                         // 12 Country
    "icecast2.play.cz",                         // 13 Beat
    "icecast7.play.cz",                         // 14 Relax
    "icecast5.play.cz",                         // 15 Radio 1
    "playerservices.streamtheworld.com",        // 16 Helax
    "ice.actve.net",                            // 17 F1
    "nl4.mystreaming.net",                      // 18 PSB
    "streaming.exclusive.radio",                // 19 M.Jackson
    "nl4.mystreaming.net",                      // 20 Madona
    "streaming.exclusive.radio",                // 21 Colins
    "streaming.exclusive.radio",                // 22 Metalica
    "play.ilovemusic.de",                       // 23 Tommorowland
    "icecast7.play.cz",                         // 24 CRo RJ
    "icecast6.play.cz",                         // 25 CRo dvojka
    "icecast6.play.cz",                         // 26 CRo Vltava
    "westradio.cz",                             // 27 Diana
    "playerservices.streamtheworld.com",        // 28 Humor
    "ice2.abradio.cz",                          // 29 Povidka
    "ice3.abradio.cz"                           // 30 Pohadka
    
    
};
 
const char* stationPaths[] = {
    
    
    "/api/livestream-redirect/EVROPA2AAC.aac",    // E2            0
    "/kiss128.mp3",                               // Kiss          1  
    "/fm-bonton-128",                             // Bonton        2
    "/signal128.mp3",                             // Signal        3
    "/fajn128.mp3",                               // Fajn          4
    "/cityfm128.mp3",                             // City          5
    "/csradio128",                                // CS Radio      6
    "/web-e2-csweb",                              // E2 CS         7
    "/blanikcz128.mp3",                           // C Blanik      8
    "/blanikfm128.mp3",                           // Blanik        9 
    "/cesky-impuls.mp3",                          // C Impuls     10 
    "/impuls128.mp3",                             // Impuls       11
    "/country128.mp3",                            // Country      12 
    "/radiobeat128.mp3",                          // Beat         13
    "/relax128.mp3",                              // Relax        14
    "/radio1-128.mp3",                            // Radio 1      15
    "/api/livestream-redirect/RADIO_HELAXAAC.aac",// Helax        16
    "/fm-frekvence1-128",                         // F1           17
    "/er/petshopboys/icecast.audio",              // PSB          18
    "/er/michaeljackson/icecast.audio",           // Jackson      19
    "/er/madonna/icecast.audio",                  // Madona       20
    "/er/philcollins/icecast.audio",              // Colins       21
    "/er/metallica/icecast.audio",                // Metalica     22 
    "/ilm-itomorrowland_one_world_radio_germany", // Tommorowland 23 
    "/cro1-128.mp3",                              // CRo RJ       24
    "/cro2-128.mp3",                              // CRo 2        25
    "/cro3-128.mp3",                              // CRo Vltava   26
    "/radio/8010/radio.mp3",                      // Diana Com.   27 
    "/api/livestream-redirect/RADIO_HUMOR.aac",   // Humor        28
    "/povidka128.aac",                            // Povidka      29
    "/pohadka128.mp3"                             // Pohadka      30
    
    
};

const char* stationLogosWeb[] = {
   
   
   "https://upload.wikimedia.org/wikipedia/commons/5/50/Evropa_2_Logo.png",                             // 0
   "https://upload.wikimedia.org/wikipedia/commons/0/0e/Logo_r%C3%A1dia_Kiss.png",                      // 1
   "https://upload.wikimedia.org/wikipedia/commons/f/fb/R%C3%A1dio_Bonton.png",                         // 2
   "https://upload.wikimedia.org/wikipedia/commons/b/b2/Logo_Sign%C3%A1l_r%C3%A1dia.png",               // 3
   "https://www.radioprofesional.cz/wp-content/uploads/2023/11/nove-logo-fajn-radia-1200x650.jpg",      // 4
   "https://www.radiotv.cz/wp-content/uploads/2013/02/city20let-675x449.png",                           // 5
   "https://www.csradio.cz/media/logo_6a09ac5110191.png",                                               // 6
   "https://static.mytuner.mobi/media/tvos_radios/jCU2MJwJFW.png",                                      // 7
   "https://static.mytuner.mobi/media/tvos_radios/064/blanik-cz.7750398b.png",                          // 8
   "https://upload.wikimedia.org/wikipedia/commons/8/80/R%C3%A1dio_Blan%C3%ADk.png",                    // 9
   "https://www.impuls.cz/images/100_100_4_d1c92fe8de895d8dd4d66333445f78cd.webp",                      // 10
   "https://upload.wikimedia.org/wikipedia/commons/3/3f/R%C3%A1dio_Impuls.png",                         // 11 
   "https://upload.wikimedia.org/wikipedia/commons/b/be/Country_Radio.png",                             // 12
   "https://upload.wikimedia.org/wikipedia/commons/c/c3/Logo_r%C3%A1dio_BEAT.png",                      // 13  
   "https://www.radiomix.cz/wp-content/uploads/2022/06/radio-relax-760x300.png",                        // 14
   "https://www.galerierudolfinum.cz/wp-content/uploads/Radio-1-log-e1546718065289.jpeg",               // 15
   "https://www.radiotv.cz/wp-content/uploads/2020/11/logo_helax_online1-624x307.png",                  // 16
   "https://upload.wikimedia.org/wikipedia/commons/e/e6/Frekvence_1.png",                               // 17
   "https://github.com/vojtechovskyj74/Loga/blob/main/PSB-rm-obrys.png?raw=true",                       // 18
   "https://encrypted-tbn0.gstatic.com/images?q=tbn:ANd9GcRyhMGnRQ0RFXYeE5CU_JXk5T0B0g9jn0dOLw&s",      // 19
   "https://www.nicepng.com/png/detail/132-1321973_madonna-rebel-heart-tour-logo.png",                  // 20
   "https://www.ireport.cz/files/66_1001371_138569.png",                                                // 21
   "https://upload.wikimedia.org/wikipedia/commons/b/b7/Metallica_logo.png",                            // 22 
   "https://ilovemusic.de/fileadmin/user_upload/tomorrowland-oneworldradio-2026-2_2x1_970x465.jpg",     // 23
   "https://upload.wikimedia.org/wikipedia/commons/1/1f/CRo_Radiozurnal_logo.png",                      // 24
   "https://upload.wikimedia.org/wikipedia/commons/f/f5/CRo_Dvojka_logo.png",                           // 25  
   "https://upload.wikimedia.org/wikipedia/commons/f/fe/%C4%8Cesk%C3%BD_rozhlas_Vltava_-_logo_1996.png",// 26
   "https://www.radiomix.cz/wp-content/uploads/2024/05/comedy-club-rdio-diana-780x300.png",             // 27
   "https://radia.cz/media/images/0001/01/9159df74ff5da694889e9418c1e2439632913328.svg",                // 28
   "https://radia.cz/media/images/0001/01/f5c0ffebd2a41fb1c5fd4000c61315f12d322f4f.svg",                // 29
   "https://radia.cz/media/images/0001/01/53f4de16a34a7a39376560876ebfb0fc7cf7edd4.svg",                // 30 
   
  
   //"https://st2.depositphotos.com/4398873/6568/v/950/depositphotos_65685647-stock-illustration-internet-radio-logo-template.jpg"

};
const int stationCount = sizeof(stationNames) / sizeof(stationNames[0]);


// =================== 4. DRAW CLOCK SLEEP ===================
void drawClock() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return;

    char timeStringBuff[10];
    
    // Formát času bez počáteční nuly u hodin (např. 9:12)
    // (%d = hodiny bez nuly, %02d = minuty vždy na dvě místa)
    sprintf(timeStringBuff, "%d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

    tft.fillScreen(ST77XX_BLACK);
    tft.setFont(&FreeSansBold24pt7b); 
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(50, 50, 250)); 
    
    // Zjištění přesné šířky textu pro dokonalé vycentrování
    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(timeStringBuff, 0, 0, &x1, &y1, &w, &h);
    
    // Displej má šířku 160px. Výpočet pozice X, aby byl text přesně uprostřed:
    int cursorX = (160 - w) / 2;
    
    tft.setCursor(cursorX, 75); 
    tft.print(timeStringBuff);
    tft.setFont();
}
//================ 5. GET BATTERY PERCENT ====================
int getBatteryPercent() {
    long sum = 0;
    for(int i = 0; i < 20; i++) {
        sum += analogReadMilliVolts(BATTERY_PIN);
        delay(2);
    }
    int pin_mv = sum / 20;
    float kalibrace = 2.15;                
    int battery_mv = pin_mv * kalibrace;
    int percent = map(battery_mv, 3100, 4150, 0, 100);
    percent = constrain(percent, 0, 100);
    
    static float smoothedBattery = -1;
    if (smoothedBattery == -1) {
        smoothedBattery = percent;
    } else {
        smoothedBattery = (smoothedBattery * 0.95) + (percent * 0.05);
    }
     return (int)smoothedBattery;
}

//============================= 6. ODSTRAN DIAKRITIKU ==================
String removeDiacritics(String text) {
    String out = "";
    for (int i = 0; i < text.length(); i++) {
        unsigned char c = text[i];
        if (c >= 32 && c <= 126) { 
            out += (char)c; 
            continue; 
        }
        if (c >= 0xE0 && c <= 0xEF && i + 2 < text.length()) {
            unsigned char c2 = text[i+1];
            unsigned char c3 = text[i+2];
                if ((c2 >= 0x80 && c2 <= 0xBF) && (c3 >= 0x80 && c3 <= 0xBF)) {
                if (c == 0xEF && c2 == 0xBF && c3 == 0xBD) {
                      out += '?'; // Místo nesmyslného 'd' raději přiznáme otazník
                } else if (c == 0xE2 && c2 == 0x80) {
                    if (c3 == 0x93 || c3 == 0x94) out += '-';       // pomlčky
                    else if (c3 == 0x98 || c3 == 0x99) out += '\''; // apostrofy
                    else if (c3 == 0x9C || c3 == 0x9D) out += '\"'; // uvozovky
                    else if (c3 == 0xA6) out += "...";              // trojtečka
                }
                i += 2; 
                continue;
            }
        }
        if (c == 0xCC && i + 1 < text.length()) {
            unsigned char c2 = text[i+1];
            if (c2 >= 0x80 && c2 <= 0xBF) { 
                i++; 
                continue; 
            }
        }
        if (c == 0xC2 && i + 1 < text.length()) {
            unsigned char c2 = text[++i];
            if (c2 == 0xA0) out += ' '; // Pevná mezera (NBSP) - častá příčina divného formátování
            else if (c2 == 0xB4) out += '\''; // Čárka nahoře
            continue;
        }
        if (c == 0xC3 && i + 1 < text.length()) {
            unsigned char c2 = text[++i];
            switch(c2) {
                case 0xA1: case 0xA4: out += 'a'; break; // á, ä
                case 0x81: case 0x84: out += 'A'; break; // Á, Ä
                case 0xA9:            out += 'e'; break; // é
                case 0x89:            out += 'E'; break; // É
                case 0xAD:            out += 'i'; break; // í
                case 0x8D:            out += 'I'; break; // Í
                case 0xB3: case 0xB6: out += 'o'; break; // ó, ö
                case 0x93: case 0x96: out += 'O'; break; // Ó, Ö
                case 0xBA: case 0xBC: out += 'u'; break; // ú, ü
                case 0x9A: case 0x9C: out += 'U'; break; // Ú, Ü
                case 0xBD:            out += 'y'; break; // ý
                case 0x9D:            out += 'Y'; break; // Ý
            }
            continue;
        }
          if (c == 0xC4 && i + 1 < text.length()) {
            unsigned char c2 = text[++i];
            switch(c2) {
                case 0x8D: out += 'c'; break; // č
                case 0x8C: out += 'C'; break; // Č
                case 0x8F: out += 'd'; break; // ď
                case 0x8E: out += 'D'; break; // Ď
                case 0x9B: out += 'e'; break; // ě
                case 0x9A: out += 'E'; break; // Ě
            }
            continue;
        }  
        if (c == 0xC5 && i + 1 < text.length()) {
            unsigned char c2 = text[++i];
            switch(c2) {
                case 0x88: out += 'n'; break; // ň
                case 0x87: out += 'N'; break; // Ň
                case 0x99: out += 'r'; break; // ř
                case 0x98: out += 'R'; break; // Ř
                case 0xA1: out += 's'; break; // š
                case 0xA0: out += 'S'; break; // Š
                case 0xA5: out += 't'; break; // ť
                case 0xA4: out += 'T'; break; // Ť
                case 0xAF: out += 'u'; break; // ů
                case 0xAE: out += 'U'; break; // Ů
                case 0xBE: out += 'z'; break; // ž
                case 0xBD: out += 'Z'; break; // Ž
            }
            continue;
        }
        switch(c) {
            case 0xE1: case 0xE4: out += 'a'; break; // á, ä
            case 0xC1: case 0xC4: out += 'A'; break; // Á, Ä
            case 0xE8:            out += 'c'; break; // č
            case 0xC8:            out += 'C'; break; // Č
            case 0xEF:            out += 'd'; break; // ď (Win-1250)
            case 0xCF:            out += 'D'; break; // Ď (Win-1250)
            case 0xE9: case 0xEC: case 0xEA: out += 'e'; break; // é, ě (1250 = EC, 8859-2 = EA)
            case 0xC9: case 0xCC: case 0xCA: out += 'E'; break; // É, Ě (1250 = CC, 8859-2 = CA)
            case 0xED:            out += 'i'; break; // í
            case 0xCD:            out += 'I'; break; // Í
            case 0xF2: case 0xF1: out += 'n'; break; // ň (1250 = F2, 8859-2 = F1)
            case 0xD2: case 0xD1: out += 'N'; break; // Ň (1250 = D2, 8859-2 = D1)
            case 0xF3: case 0xF6: out += 'o'; break; // ó, ö
            case 0xD3: case 0xD6: out += 'O'; break; // Ó, Ö
            case 0xF8:            out += 'r'; break; // ř
            case 0xD8:            out += 'R'; break; // Ř
            case 0x9A: case 0xB9: out += 's'; break; // š (1250 = 9A, 8859-2 = B9)
            case 0x8A: case 0xA9: out += 'S'; break; // Š (1250 = 8A, 8859-2 = A9)
            case 0x9D: case 0xBB: out += 't'; break; // ť (1250 = 9D, 8859-2 = BB)
            case 0x8D: case 0xAB: out += 'T'; break; // Ť (1250 = 8D, 8859-2 = AB)
            case 0xFA: case 0xFC: case 0xF9: out += 'u'; break; // ú, ü, ů (ů je F9 pro obojí)
            case 0xDA: case 0xDC: case 0xD9: out += 'U'; break; // Ú, Ü, Ů
            case 0xFD:            out += 'y'; break; // ý
            case 0xDD:            out += 'Y'; break; // Ý
            case 0x9E: case 0xBE: out += 'z'; break; // ž (1250 = 9E, 8859-2 = BE)
            case 0x8E: case 0xAE: out += 'Z'; break; // Ž (1250 = 8E, 8859-2 = AE)
        }
    }
    return out;
}
//================================ 7.SVATEK =======================================
void updateSvatek() {
    if (WiFi.status() != WL_CONNECTED) return;
    WiFiClientSecure secureClient;
    secureClient.setInsecure();
    HTTPClient https;
    if (https.begin(secureClient, svatekUrl)) {
        if (https.GET() == HTTP_CODE_OK) {
            DynamicJsonDocument doc(512);
            deserializeJson(doc, https.getString());
            String rawSvatek = doc["name"] | "N/A"; 
            svatek = removeDiacritics(rawSvatek);   
            svatek.toUpperCase();
        }
        https.end();
    }
}
//================================= 8.SCROLL TEXT ================================
void prepareScrollText() {
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)) {
        scrollText = "Cekam na cas...";
        return;
    }
    const char* dny[] = {"Nedele", "Pondeli", "Utery", "Streda", "Ctvrtek", "Patek", "Sobota"};
    const char* mesice[] = {"Ledna", "Unora", "Brezna", "Dubna", "Kvetna", "Cervna", "Cervence", "Srpna", "Zari", "Rijna", "Listopadu", "Prosince"};
    char buf[128];
    sprintf(buf, "Dnes je %s %d. %s %d  *   Svatek ma %s   *", 
            dny[timeinfo.tm_wday], timeinfo.tm_mday, mesice[timeinfo.tm_mon], timeinfo.tm_year + 1900, svatek.c_str());
    scrollText = String(buf);
}

//====================================== 9.DRAW TITLE ===========================
void updateTitleCanvas() {
    titleCanvas.fillScreen(0x2104); // Pozadí lišty
    titleCanvas.setTextWrap(false);
    titleCanvas.setTextSize(1, 2);  // Šířka 1, Výška 2 (stejné proporce jako hodiny)
    
    // Barvy podle stavu
    if (currentTitle == "Nacitam buffer..." || currentTitle == "Chyba WiFi spojeni") { 
        titleCanvas.setTextColor(ST77XX_RED);
    } else if (currentTitle == "Prehravam...") { 
        titleCanvas.setTextColor(ST77XX_WHITE); 
    } else { 
        titleCanvas.setTextColor(ST77XX_CYAN);
    }
    if (needsTitleScroll) {
        int textWidth = currentTitle.length() * 6;
        int gap = 40; // Mezera mezi opakujícím se textem
        titleCanvas.setCursor(titleScrollPos, 1);
        titleCanvas.print(currentTitle);
        titleCanvas.setCursor(titleScrollPos + textWidth + gap, 1);
        titleCanvas.print(currentTitle);
    } else {
        titleCanvas.setCursor(titleScrollPos, 1);
        titleCanvas.print(currentTitle);
    }
    tft.drawRGBBitmap(0, 108, titleCanvas.getBuffer(), 160, 18);
}

void drawTitle() {
    if (currentTitle == lastDrawnTitle) return;
    lastDrawnTitle = currentTitle;
    tft.fillRect(0, 106, 160, 22, 0x2104); 
    tft.drawLine(0, 106, 160, 106, ST77XX_BLUE); 
    tft.drawLine(0, 127, 160, 127, ST77XX_BLUE); 
    int textWidth = currentTitle.length() * 6;
      if (textWidth > 150) {
        needsTitleScroll = true;
        titleScrollPos = 5; // Začneme hezky kousek od okraje
        lastTitleScrollTime = millis() + 1500; // Počkáme 1.5 vteřiny, než se text dá do pohybu
    } else {
        needsTitleScroll = false;
        titleScrollPos = (160 - textWidth) / 2; // Pokud je krátký, vycentrujeme ho na střed
        if (titleScrollPos < 5) titleScrollPos = 5; 
    }
      updateTitleCanvas();
}
//================================= 10.DRAW VOLUME ============================
void drawVolume() {
    // Vykreslení pozadí a horní/dolní ohraničující čáry (stejné jako u metadat)
    tft.fillRect(0, 106, 160, 22, 0x2104); 
    tft.drawLine(0, 106, 160, 106, ST77XX_BLUE);
    tft.drawLine(0, 127, 160, 127, ST77XX_BLUE);

    uint16_t barColor;
    if (curVolume < 50)       barColor = ST77XX_YELLOW;
    else if (curVolume < 80)  barColor = ST77XX_GREEN;
    else if (curVolume < 90)  barColor = ST77XX_ORANGE;
    else                      barColor = ST77XX_RED;

    tft.setFont();
    tft.setTextSize(1, 2); // UPRAVENO: Použita protažená velikost fontu
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(3, 109);
    tft.print("VOL");
    tft.setCursor(134, 109);
    if (curVolume < 100) tft.print(" ");
    if (curVolume < 10)  tft.print(" ");
    tft.print(curVolume);
    tft.print("%");
   
    int barX = 24; 
    int barY = 110; 
    int barWidth = 106; 
    int barHeight = 14; // UPRAVENO: Vyšší sloupec, aby ladil s fontem
    int filledWidth = map(curVolume, 0, 100, 0, barWidth);
    tft.drawRect(barX, barY, barWidth, barHeight, 0x5AEB);
    if (filledWidth > 4) {
        tft.fillRect(barX + 2, barY + 2, filledWidth - 4, barHeight - 4, barColor);
    }
      lastDrawnTitle = "VOL_CHANGE"; 
}

//============================== 11.GET TIME ==========================================
String getFormattedTime() {
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)) return "00:00:00";
    char timeStringBuff[10];
    strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M:%S", &timeinfo);
    return String(timeStringBuff);
}
//=============================== 12.TIME ONLY ===================================
void drawTimeOnly() {
    uint16_t barBg = 0x000F;
    tft.setFont();
    tft.setTextSize(1, 2);
    tft.setTextColor(ST77XX_CYAN, barBg);

    String fullTime = getFormattedTime();
    String hh = fullTime.substring(0, 2);
    String mm = fullTime.substring(3, 5);
    String ss = fullTime.substring(6, 8);

    tft.setCursor(2, 5); 
    tft.print(hh);
    tft.fillRect(16, 8, 2, 2, ST77XX_CYAN);
    tft.fillRect(16, 15, 2, 2, ST77XX_CYAN);
    tft.setCursor(21, 5);
    tft.print(mm);
    tft.fillRect(35, 8, 2, 2, ST77XX_CYAN);
    tft.fillRect(35, 15, 2, 2, ST77XX_CYAN);
    tft.setCursor(40, 5);
    tft.print(ss);
}
//=================================== 13.WIFI ICON ==========================
void drawWifiIcon() {
    int wifiX = 80;
    long rssi = WiFi.RSSI();
    int bars = 0;
    uint16_t wifiColor = ST77XX_RED;
    uint16_t inactiveColor = 0x4208; // Tmavě šedá barva pro chybějící signál

    if (WiFi.status() == WL_CONNECTED) {
        if (rssi > -55)       { bars = 5; wifiColor = ST77XX_GREEN; }
        else if (rssi > -65)  { bars = 4; wifiColor = 0xAFE5; }
        else if (rssi > -75)  { bars = 3; wifiColor = ST77XX_YELLOW; }
        else if (rssi > -85)  { bars = 2; wifiColor = ST77XX_ORANGE; }
        else if (rssi > -95)  { bars = 1; wifiColor = ST77XX_RED; }
    }

    if (bars > 0) {
        tft.fillRect(wifiX,      18, 2, 3,  (bars >= 1) ? wifiColor : inactiveColor);
        tft.fillRect(wifiX + 3,  15, 2, 6,  (bars >= 2) ? wifiColor : inactiveColor);
        tft.fillRect(wifiX + 6,  12, 2, 9,  (bars >= 3) ? wifiColor : inactiveColor);
        tft.fillRect(wifiX + 9,   9, 2, 12, (bars >= 4) ? wifiColor : inactiveColor);
        tft.fillRect(wifiX + 12,  6, 2, 15, (bars >= 5) ? wifiColor : inactiveColor);
    } else {
        tft.drawLine(wifiX + 4, 7, wifiX + 16, 19, ST77XX_RED);
        tft.drawLine(wifiX + 5, 7, wifiX + 17, 19, ST77XX_RED);
        tft.drawLine(wifiX + 16, 7, wifiX + 4, 19, ST77XX_RED);
        tft.drawLine(wifiX + 17, 7, wifiX + 5, 19, ST77XX_RED);
    }
}

//================================ 13b. DRAW OUTPUT ICON =======================
void drawOutputIcon() {
    int ix = 128; // Souřadnice X: Vpravo
    int iy = 38;  // Souřadnice Y: Posunuto o 8 pixelů dolů
    
    if (isMuted) {
        tft.fillRoundRect(ix, iy, 30, 16, 3, ST77XX_RED);
        tft.drawRoundRect(ix, iy, 30, 16, 3, ST77XX_WHITE);
        tft.fillRect(ix + 5, iy + 6, 3, 4, ST77XX_WHITE);
        tft.fillTriangle(ix + 8, iy + 6, ix + 8, iy + 10, ix + 12, iy + 2, ST77XX_WHITE);
        tft.fillTriangle(ix + 8, iy + 10, ix + 12, iy + 14, ix + 12, iy + 2, ST77XX_WHITE);
        
        tft.drawLine(ix + 16, iy + 4, ix + 24, iy + 12, ST77XX_WHITE);
        tft.drawLine(ix + 24, iy + 4, ix + 16, iy + 12, ST77XX_WHITE);
    } 
    else if (rezimSluchatka) {
        tft.fillRoundRect(ix, iy, 30, 16, 3, ST77XX_BLUE);
        tft.drawRoundRect(ix, iy, 30, 16, 3, ST77XX_WHITE); 
        int bx = ix + 15;
        int by = iy + 8;
        uint16_t c = ST77XX_WHITE;
        
        tft.drawLine(bx, by - 5, bx, by + 5, c);         // Prostřední svislá čára
        tft.drawLine(bx, by - 5, bx + 3, by - 2, c);     // Z horní špičky doprava dolů
        tft.drawLine(bx + 3, by - 2, bx - 3, by + 2, c); // Zprava křížem přes střed doleva dolů
        tft.drawLine(bx - 3, by - 2, bx + 3, by + 2, c); // Zleva křížem přes střed doprava dolů
        tft.drawLine(bx + 3, by + 2, bx, by + 5, c);     // Zprava do spodní špičky
    }
}

//================================ 14.UPDATE ICONS =====================================
void updateIcons() {
     uint16_t barBg = 0x000F;
     tft.fillRect(78, 5, 22, 18, barBg);
     drawWifiIcon(); 
     int bat = getBatteryPercent();
     int chargeMv = analogReadMilliVolts(CHARGE_PIN);
     bool isCharging = (chargeMv > 2100); 
     static bool blinkState = false;
     blinkState = !blinkState;
     static bool lastTimerState = false;
     static int lastConnectedClients = -1;
     bool forceRedraw = (bat != lastDrawnBattery) || (isCharging != lastChargingState) || isCharging || (isSleepTimerActive != lastTimerState) || (connectedWebClients != lastConnectedClients);

    if (forceRedraw) {
        lastDrawnBattery = bat;
        lastChargingState = isCharging;
        lastTimerState = isSleepTimerActive;
        lastConnectedClients = connectedWebClients;
   
        tft.fillRect(52, 5, 19, 18, barBg); 
        if (isSleepTimerActive) {
            int tx = 61; // Střed stopek
            int ty = 14; 
            tft.drawCircle(tx, ty, 5, ST77XX_YELLOW);
            tft.drawLine(tx, ty, tx, ty - 3, ST77XX_YELLOW);
            tft.drawLine(tx, ty, tx + 2, ty, ST77XX_YELLOW);
            tft.drawLine(tx - 2, ty - 6, tx + 2, ty - 6, ST77XX_YELLOW);
        }
      
        tft.fillRect(100, 5, 16, 18, barBg); 
        if (connectedWebClients > 0) {
            tft.fillRoundRect(103, 6, 9, 14, 2, ST77XX_GREEN); 
            tft.fillRect(105, 8, 5, 9, barBg); // Vykrojení displeje mobilu
        }
            tft.fillRect(116, 4, 44, 19, barBg); 
            uint16_t bCol = (bat < 20) ? ST77XX_RED : (bat < 40) ? ST77XX_ORANGE :(bat < 60) ? ST77XX_YELLOW : ST77XX_GREEN;
        
        int iconX = 146; // Pevná pozice na pravé straně
        int iconY = 6;   
        int iconW = 10;  // Úzká baterie
        int iconH = 15;  // Vysoká baterie
      
        tft.setTextSize(1, 2); 
        tft.setTextColor(ST77XX_CYAN, barBg);
        String batStr = String(bat) + "%";
        int textX = iconX - 4 - (batStr.length() * 6); // Spočítá délku textu a zarovná ho
        tft.setCursor(textX, 5); 
        tft.print(batStr);
    
        tft.drawRect(iconX, iconY, iconW, iconH, bCol);            
        tft.fillRect(iconX + 3, iconY - 2, 4, 2, bCol); // Plusový kontakt NAHOŘE
        tft.fillRect(iconX + 1, iconY + 1, iconW - 2, iconH - 2, barBg); // Čistý vnitřek    
     
        int fH = map(bat, 0, 100, 0, iconH - 2);
        if (fH > 0) {
             tft.fillRect(iconX + 1, iconY + iconH - 1 - fH, iconW - 2, fH, bCol); 
        }
        if (isCharging && blinkState) {
            int bx = iconX + 1; // Vnitřní hrana baterie
            int by = iconY + 1; // Vnitřní horní okraj
                for (int i = 0; i < 2; i++) {
                tft.drawLine(bx + 5 + i, by + 1, bx + 1 + i, by + 6, ST77XX_RED);
                tft.drawLine(bx + 1, by + 6 + i, bx + 6, by + 6 + i, ST77XX_RED); // Horizontální střed blesku
                tft.drawLine(bx + 5 + i, by + 6, bx + 1 + i, by + 12, ST77XX_RED);
            }
        }
    }
}
//============================= 15.DRAW SCREEN ===============================
void drawScreen() {
    tft.fillScreen(ST77XX_BLACK);

    uint16_t barBg = 0x000F;
    tft.fillRect(0, 0, 160, 24, barBg);
    tft.drawLine(0, 24, 160, 24, ST77XX_BLUE);
    
    // 1. Hodiny v horní liště
    tft.setFont();
    tft.setTextSize(1, 2);
    tft.setTextColor(ST77XX_CYAN, barBg);
    String fullTime = getFormattedTime();
    tft.setCursor(2, 5); tft.print(fullTime.substring(0, 2));
    tft.fillRect(16, 8, 2, 2, ST77XX_CYAN); tft.fillRect(16, 15, 2, 2, ST77XX_CYAN);
    tft.setCursor(21, 5); tft.print(fullTime.substring(3, 5));
    tft.fillRect(35, 8, 2, 2, ST77XX_CYAN); tft.fillRect(35, 15, 2, 2, ST77XX_CYAN);
    tft.setCursor(40, 5); tft.print(fullTime.substring(6, 8));

    lastDrawnBattery = -1;
    updateIcons(); 
    if (isSelecting) {
        auto getStation = [](int offset, int current) {
            return String(stationNames[(current + offset + stationCount) % stationCount]);
        };

        tft.setFont();
        tft.setTextSize(1);
        tft.setTextColor(0x52AA); 
        String sM2 = getStation(-2, currentStation);
        tft.setCursor((160 - sM2.length() * 6) / 2, 32);
        tft.print(sM2);
        tft.setTextColor(0xAD75); 
        String sM1 = getStation(-1, currentStation);
        tft.setCursor((160 - sM1.length() * 6) / 2, 45);
        tft.print(sM1);
        tft.setFont(&FreeSansBold9pt7b);
        tft.setTextColor(ST77XX_WHITE);
        String s0 = getStation(0, currentStation);
        int16_t x1, y1; uint16_t w, h;
        tft.getTextBounds(s0, 0, 0, &x1, &y1, &w, &h);
        tft.setCursor((160 - w) / 2, 70); 
        tft.print(s0);
        tft.drawLine(10, 54, 150, 54, ST77XX_RED);
        tft.drawLine(10, 75, 150, 75, ST77XX_RED);
        tft.setFont(); 
        tft.setTextSize(1);
        tft.setTextColor(0xAD75); 
        String sP1 = getStation(1, currentStation);
        tft.setCursor((160 - sP1.length() * 6) / 2, 83);
        tft.print(sP1);
        tft.setTextColor(0x52AA);
        String sP2 = getStation(2, currentStation);
        tft.setCursor((160 - sP2.length() * 6) / 2, 96);
        tft.print(sP2);
        tft.fillRect(0, 106, 160, 22, 0x2104);
        tft.drawLine(0, 106, 160, 106, ST77XX_BLUE);
        tft.setTextColor(ST77XX_WHITE);
        tft.setCursor(20, 112);
        tft.print("Potvrdte cekanim...");
        needsTitleScroll = false;
    } else {
        int logoW = stationLogos[currentStation].width;
        int logoH = stationLogos[currentStation].height;
        int logoX = (160 - logoW) / 2;
        int logoY = 37 + ((69 - logoH) / 2);
        tft.drawRGBBitmap(logoX, logoY, stationLogos[currentStation].data, logoW, logoH);
        lastDrawnTitle = "";
        drawTitle();
    }
     prepareScrollText();
     drawOutputIcon();
}
//============================== 16.CONNECT STATION ========================
void connectStation() {
    audio.stopSong();
    delay(100);
    currentTitle = "Nacitam buffer...";
    aktualniPisnicka = "Pripojuji ke stanici...";
    webSocket.broadcastTXT("SONG:" + aktualniPisnicka);
    drawScreen();
    drawTitle();
    streamConnectTime = millis();
    String host = String(stationHosts[currentStation]);
    if (host.startsWith("http")) {
        currentStreamUrl = host; 
    } else {
        currentStreamUrl = "http://" + host + String(stationPaths[currentStation]);
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        audio.connecttohost(currentStreamUrl.c_str());
    } else {
        currentTitle = "Chyba WiFi spojeni";
        drawTitle();
    }   
}

// ============================== 17. NOVÝ SYSTÉM METADAT ===================
void my_audio_events(Audio::msg_t m) {
        if (m.e == Audio::evt_streamtitle && m.msg) {
        String cleanTitle = removeDiacritics(String(m.msg)); 
        currentTitle = cleanTitle; 
        aktualniPisnicka = cleanTitle; 
        drawTitle(); 
        webSocket.broadcastTXT("SONG:" + aktualniPisnicka);
    }
        if (m.e == Audio::evt_name && m.msg) {
        String cleanStation = removeDiacritics(String(m.msg)); 
          if (currentTitle == "Nacitam buffer..." || currentTitle == "Prehravam...") {
              currentTitle = cleanStation; 
              aktualniPisnicka = cleanStation;
              drawTitle(); 
              webSocket.broadcastTXT("SONG:" + aktualniPisnicka);
        }
    }
}

// ========================== 18. CORE FUNKCE (ZÁKLADNÍ MOZEK RÁDIA) =======================================
void doPower() {
    isRadioPlaying = !isRadioPlaying; 
    if (isRadioPlaying) {
        connectStation();
        drawScreen();             
        lastDrawnBattery = -1;    
        updateIcons();
    } else {
        audio.stopSong();         
        tft.fillScreen(ST77XX_BLACK); 
        drawClock();              
    }
    lastActionTime = millis();
    webSocket.broadcastTXT("POWER:" + String(isRadioPlaying ? "1" : "0")); 
}

void doVolume(int v) {
    // --- PŘIDÁNO: Probuzení rádia, pokud zrovna spí ---
    if (!isRadioPlaying) {
        isRadioPlaying = true;
        connectStation();
        drawScreen();             
        lastDrawnBattery = -1;    
        updateIcons();
        webSocket.broadcastTXT("POWER:1"); 
    }
    curVolume = v;
    int internalVol = map(curVolume, 0, 100, 0, 21);
    audio.setVolume(internalVol);
    preferences.putInt("volume", curVolume);
    lastActionTime = millis(); 
    drawVolume(); // Vykreslí lištu hlasitosti přes nově probuzenou obrazovku
    webSocket.broadcastTXT("VOL:" + String(curVolume)); 
}

void doSetStation(int newStation) {
    if (newStation >= 0 && newStation < stationCount) {
        currentStation = newStation;
        preferences.putInt("station", currentStation);
        drawScreen();
        connectStation();
        isRadioPlaying = true; 
        webSocket.broadcastTXT("STATION:" + String(stationNames[currentStation])); 
        webSocket.broadcastTXT("POWER:1");
    }
}

void doSleep(int m) {
    currentSleepValue = m;
    if (currentSleepValue > 0) {
        isSleepTimerActive = true;
        sleepTimerDuration = currentSleepValue * 60000UL; 
        sleepTimerStart = millis();
    } else {
        isSleepTimerActive = false;
    }
    lastDrawnBattery = -1; 
    updateIcons();         
    webSocket.broadcastTXT("SLEEP:" + String(currentSleepValue));
}

void doEQ(int l, int m, int h) {
    eqLow = l; eqMid = m; eqHigh = h;
    audio.setTone(eqLow, eqMid, eqHigh); 
    preferences.putInt("eqLow", eqLow);
    preferences.putInt("eqMid", eqMid);
    preferences.putInt("eqHigh", eqHigh);
}

// ========================== 19. WEBPAGE ===================
#include "webpage.h"

void handleRoot() { server.send(200, "text/html", getWebPage()); }
void handlePower() { doPower(); server.send(200, "text/plain", "OK"); }
void handleVolume() { if (server.hasArg("v")) doVolume(server.arg("v").toInt()); server.send(200, "text/plain", "OK"); }
void handleSetStation() { if (server.hasArg("id")) doSetStation(server.arg("id").toInt()); server.send(200, "text/plain", "OK"); }
void handleSleepTimer() { if (server.hasArg("m")) doSleep(server.arg("m").toInt()); server.send(200, "text/plain", "OK"); }
void handleEQ() { if (server.hasArg("low")) doEQ(server.arg("low").toInt(), server.arg("mid").toInt(), server.arg("high").toInt()); server.send(200, "text/plain", "OK"); }
void handleStatus() { server.send(200, "text/plain", isRadioPlaying ? "1" : "0"); }
void handleVolUp() { isMuted = false; doVolume(min(curVolume + 5, 100)); server.send(200, "text/plain", "OK"); }
void handleVolDown() { isMuted = false; doVolume(max(curVolume - 5, 0)); server.send(200, "text/plain", "OK"); }
void handleMute() { 
    if (isMuted) { 
        isMuted = false; 
        doVolume(preMuteVolume); 
        if (rezimSluchatka) drawOutputIcon(); else drawScreen();
    } else { 
        isMuted = true; 
        preMuteVolume = curVolume; 
        doVolume(0); 
        drawOutputIcon(); 
    } 
    webSocket.broadcastTXT(isMuted ? "OUT:MUTE" : (rezimSluchatka ? "OUT:BT" : "OUT:SPK"));
    server.send(200, "text/plain", "OK"); 
}

// ========================== 20. WEBSOCKET EVENT LISTENER ===================
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    if (type == WStype_CONNECTED) {
        connectedWebClients++; // PŘIDÁNO: Telefon se připojil
        
        webSocket.sendTXT(num, "POWER:" + String(isRadioPlaying ? "1" : "0"));
        webSocket.sendTXT(num, "VOL:" + String(curVolume));
        webSocket.sendTXT(num, "STATION:" + String(stationNames[currentStation]));
        webSocket.sendTXT(num, "SONG:" + aktualniPisnicka);
        webSocket.sendTXT(num, "SLEEP:" + String(isSleepTimerActive ? currentSleepValue : 0));
        webSocket.sendTXT(num, "FW:" + fwVersion);
        
    } else if (type == WStype_DISCONNECTED) {
        if (connectedWebClients > 0) connectedWebClients--; // PŘIDÁNO: Telefon se odpojil
        
    } else if (type == WStype_TEXT) {
        String msg = String((char*)payload);
        if (msg == "POWER") { doPower(); } 
        else if (msg.startsWith("VOL:")) { doVolume(msg.substring(4).toInt()); } 
        else if (msg.startsWith("SET:")) { doSetStation(msg.substring(4).toInt()); } 
        else if (msg.startsWith("SLEEP:")) { doSleep(msg.substring(6).toInt()); } 
        else if (msg.startsWith("EQ:")) {
            int firstColon = msg.indexOf(':', 3);
            int secondColon = msg.indexOf(':', firstColon + 1);
            int l = msg.substring(3, firstColon).toInt();
            int m = msg.substring(firstColon + 1, secondColon).toInt();
            int h = msg.substring(secondColon + 1).toInt();
            doEQ(l, m, h);
        }
    }
}


/*
// ========================== 21/7. AUDIO VIZUALIZÉR =================================
void drawVisualizer() {
    static unsigned long lastVis = 0;
    if (millis() - lastVis < 30) return; 
    lastVis = millis();

    if (!isRadioPlaying) {
        tft2.fillScreen(ST77XX_BLACK);
        return;
    }

    uint32_t vu = audio.getVUlevel();
    int maxVol = max(vu >> 16, vu & 0xFFFF); 
    if (maxVol < 15) maxVol = 0;

    // 1. Získání základní hlasitosti (odmocnina stáhne obří čísla do rozmezí 0-180)
    float currentVol = sqrt(maxVol); 

    // 2. Extrémně zpomalený průměr (tvoří "tělo" hudby, které odečteme)
    static float slowVol = 0;
    slowVol = (slowVol * 0.95) + (currentVol * 0.05);

    // --- ROZDĚLENÍ DO TŘÍ VIRTUÁLNÍCH PÁSEM ---

    // A. BASY (Hledáme "Punch" - úder. Zajímají nás jen špičky vysoko nad průměrem)
    float punch = currentVol - (slowVol * 0.7);
    if (punch < 0) punch = 0;
    static float bassVol = 0;
    bassVol = (bassVol * 0.5) + (punch * 0.6); // Rychlý nástup, rychlejší pád

    // B. STŘEDY (Poklidné tělo melodie a zpěvu)
    static float midVol = 0;
    midVol = (midVol * 0.75) + (currentVol * 0.25);

    // C. VÝŠKY (Hledáme bleskové rány a činely - to, co se ti líbilo na pravé straně)
    float delta = currentVol - slowVol;
    if (delta < 0) delta = 0;
    static float highVol = 0;
    highVol = (highVol * 0.4) + (delta * 2.5); // Extrémně rychlý pád

    // --- MANUÁLNÍ NALADĚNÍ KAŽDÉHO SLOUPCE (Rozbití "schodu") ---
    // Tímto říkáme, jak moc má každý z 16 sloupců reagovat na Basy, Středy a Výšky.
    // Schválně jsou tu nepravidelná čísla, aby to vytvořilo přirozené zuby.
    const float pB[16] = {1.8, 1.5, 1.1, 0.7, 0.3, 0.1, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    const float pM[16] = {0.0, 0.0, 0.1, 0.3, 0.4, 0.5, 0.6, 0.7, 0.6, 0.5, 0.3, 0.1, 0.0, 0.0, 0.0, 0.0};
    const float pH[16] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.1, 0.3, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0}; 

    int segW = 8;  
    int gap = 2;
    static float peaks[16] = {0};   
    static int peakDelay[16] = {0}; 
    static int lastH[16] = {0};      
    static int oldPeaks[16] = {0};

    for (int i = 0; i < 16; i++) {
        // Součet energií pro daný sloupec podle naší manuální mapy výše
        float energy = (bassVol * pB[i]) + (midVol * pM[i]) + (highVol * pH[i]);
         //  energy *= 0.7;
        // Mapování na pixely (maximum energie je cca 160, strop displeje je 75)
        int targetH = map((long)energy, 0, 300, 0, 75); 
        if (targetH > 76) targetH = 76; // Tvrdý bezpečnostní limit
        if (targetH < 0) targetH = 0;

        int currentH = lastH[i];

        // Fyzika sloupce: vystřelí ihned, padá plynule
        if (targetH > currentH) {
            currentH = targetH; 
        } else {
            currentH -= 4; // Rychlost padání sloupců (gravitace)
            if (currentH < targetH) currentH = targetH;
        }
        
        if (currentH < 0) currentH = 0;

        // Logika bílých čepiček (Peaků)
        if (currentH >= peaks[i]) {
            peaks[i] = currentH;
            peakDelay[i] = 3; // Počet cyklů, kdy čepička čeká nahoře
        } else {
            if (peakDelay[i] > 0) peakDelay[i]--;
            else peaks[i] -= 2.0; // Rychlost padání čepičky
        }
        if (peaks[i] < 0) peaks[i] = 0;

        int drawPeak = (int)peaks[i];
        int x = i * (segW + gap);

        // VYKRESLOVÁNÍ S OPTIMALIZACÍ (Maže se jen to, co spadlo)
        if (oldPeaks[i] > 0 && oldPeaks[i] != drawPeak && oldPeaks[i] > currentH) {
            tft2.fillRect(x, 80 - oldPeaks[i], segW, 2, ST77XX_BLACK);
        }
        if (currentH < lastH[i]) {
            tft2.fillRect(x, 80 - lastH[i], segW, lastH[i] - currentH, ST77XX_BLACK);
        }

        if (currentH > 0) {
            if (currentH > 0)  tft2.fillRect(x, 80 - min(currentH, 40), segW, min(currentH, 40), ST77XX_GREEN);
            if (currentH > 40) tft2.fillRect(x, 40 - (min(currentH, 60) - 40), segW, min(currentH, 60) - 40, ST77XX_YELLOW);
            if (currentH > 60) tft2.fillRect(x, 20 - (currentH - 60), segW, currentH - 60, ST77XX_RED);
        }

        if (drawPeak > 0) {
            tft2.fillRect(x, 80 - drawPeak, segW, 2, ST77XX_WHITE);
        }

        lastH[i] = currentH;
        oldPeaks[i] = drawPeak;
    }
}

*/


 // ========================== 21/2. AUDIO VIZUALIZÉR =================================
void drawVisualizer() {
    static unsigned long lastVis = 0;
    if (millis() - lastVis < 30) return; 
    lastVis = millis();

    if (!isRadioPlaying) {
        tft2.fillScreen(ST77XX_BLACK);
        return;
    }

    uint32_t vu = audio.getVUlevel();
    int maxVol = max(vu >> 16, vu & 0xFFFF); 
    float rawVal = sqrt(maxVol) * 0.8;           // Citlivost

    static float smoothVol = 0;
    static float bassMem = 0;
    static float midMem = 0;
    static float highMem = 0;
    
    // --- ŠUMOVÁ BRÁNA A RESET PŘI TICHI ---
    if (maxVol < 15) { // Pokud je signál pod prahem šumu (např. ticho mezi písničkami)
        rawVal = 0;
        smoothVol = 0;
        bassMem = 0;
        midMem = 0;
        highMem = 0;
    }

    float delta = abs(rawVal - smoothVol);
    smoothVol = rawVal;

    bassMem = bassMem * 0.7 + rawVal * 0.3;         
    midMem = midMem * 0.5 + rawVal * 0.5;           
    highMem = highMem * 0.4 + delta * 2.0;          

    int segW = 8;  
    int gap = 2;   
    
    static float peaks[16] = {0};   
    static int peakDelay[16] = {0}; 
    static int lastH[16] = {0};      
    static int oldPeaks[16] = {0};   

    for (int i = 0; i < 16; i++) {
        float energy = 0;
        
        if (i <= 5) { // --- BASY A NIŽŠÍ STŘEDY (Sloupce 0 až 5) ---
            // Odstraněna divoká 'delta'. Sloupce nyní perfektně kopírují rytmus (rawVal)
            // s plynulým přechodem od tvrdého kopáku po táhlou basu.
            switch(i) {
                case 0: energy = rawVal * 1.1; break;                  // Úderný kopák (sníženo z 1.3)
                case 1: energy = rawVal * 0.9 + bassMem * 0.15; break; // Rychlý bas (méně paměti)
                case 2: energy = rawVal * 0.7 + bassMem * 0.25; break; // Střední bas
                case 3: energy = rawVal * 0.6 + bassMem * 0.35; break; // Táhlá basová linka
                case 4: energy = rawVal * 0.7 + midMem * 0.3; break;   // Přechod do nižších středů
                case 5: energy = midMem * 1.1; break;                  // Čisté nižší středy (sníženo z 1.1)
            }
            energy *= 0.60; // <-- HLAVNÍ BRZDA: Sníženo z 0.85 na 0.60. Tímto celou levoun stranu snížíš.
        } else if (i <= 10) { // Středy
            float ratio = (i - 5) / 5.0;
            energy = midMem * (1.0 - ratio) + highMem * ratio;
        } else { // Výšky
            float ratio = (i - 10) / 5.0;
            energy = highMem * (1.0 - ratio * 0.4); 
        }

        // --- CHYTRÝ RANDOMIZER (Aktivní pouze pokud hraje hudba) ---
        if (energy > 0.5) {
            energy *= random(92, 108) / 100.0; // Jemnější zvlnění než původně
            energy += random(-1, 2);
        } else {
            energy = 0; // Totální nula při tichu
        }

        int targetH = map((long)energy, 0, 250, 0, 80); 
        if (targetH > 80) targetH = 80;
        if (targetH < 0) targetH = 0;

        int currentH = lastH[i];
        if (targetH > currentH) {
            currentH = targetH; 
        } else {
            // Rychlost padání sloupců dolů. Zvýšeno z 3 na 4 pro svižnější reakci.
            currentH -= 4;      
            if (currentH < targetH) currentH = targetH;
        }
        
        if (currentH < 0) currentH = 0;

        // Logika vykreslování peaků (špiček)
        if (currentH >= peaks[i]) {
            peaks[i] = currentH;
            peakDelay[i] = 5;       
        } else {
            if (peakDelay[i] > 0) peakDelay[i]--;
            else peaks[i] -= 1.5;   
        }
        if (peaks[i] < 0) peaks[i] = 0;

        int drawPeak = (int)peaks[i];
        int x = i * (segW + gap); 

        // Smazání starého peaku
        if (oldPeaks[i] > 0 && oldPeaks[i] != drawPeak && oldPeaks[i] > currentH) {
            tft2.fillRect(x, 80 - oldPeaks[i], segW, 2, ST77XX_BLACK);
        }
        // Smazání přebytečné barvy při pádu sloupce dolů
        if (currentH < lastH[i]) {
            tft2.fillRect(x, 80 - lastH[i], segW, lastH[i] - currentH, ST77XX_BLACK);
        }
        // Vykreslení sloupce (Zelená -> Žlutá -> Červená)
        if (currentH > 0) {
            if (currentH > 0)  tft2.fillRect(x, 80 - min(currentH, 40), segW, min(currentH, 40), ST77XX_GREEN);
            if (currentH > 40) tft2.fillRect(x, 40 - (min(currentH, 60) - 40), segW, min(currentH, 60) - 40, ST77XX_YELLOW);
            if (currentH > 60) tft2.fillRect(x, 20 - (currentH - 60), segW, currentH - 60, ST77XX_RED);
        }
        // Vykreslení bílého peaku
        if (drawPeak > 0) {
            tft2.fillRect(x, 80 - drawPeak, segW, 2, ST77XX_WHITE);
        }

        lastH[i] = currentH;
        oldPeaks[i] = drawPeak;
    }
}

// ========================== 22. START ANIMACE TFT2 =================================

  volatile bool kittRunning = true;
  struct Star {
    float x, y, z;
    int lastSx, lastSy;
    int lastSize; 
             };
  void kittTask(void *pvParameters) {
    tft2.fillScreen(ST77XX_BLACK);
    const int NUM_STARS = 60;
    Star* stars = new Star[NUM_STARS];
     int cx = 80;
     int cy = 40;
      for (int i = 0; i < NUM_STARS; i++) {
        stars[i].x = random(-100, 100);
        stars[i].y = random(-100, 100);
        stars[i].z = random(10, 200);
        stars[i].lastSx = -1;
        stars[i].lastSy = -1;
        stars[i].lastSize = 1;
    }
                       // --- FÁZE 1: LET HVĚZD (dokud probíhá načítání/WiFi) ---
    while (kittRunning) {
        for (int i = 0; i < NUM_STARS; i++) {
                if (stars[i].lastSx >= 0 && stars[i].lastSx < 160 && stars[i].lastSy >= 0 && stars[i].lastSy < 80) {
                tft2.fillRect(stars[i].lastSx, stars[i].lastSy, stars[i].lastSize, stars[i].lastSize, ST77XX_BLACK);
            }
            stars[i].z -= 4.0;
            if (stars[i].z <= 1.0) {
                stars[i].x = random(-100, 100);
                stars[i].y = random(-100, 100);
                stars[i].z = 200.0;
                stars[i].lastSx = -1;
            }
            int sx = (stars[i].x * 50.0 / stars[i].z) + cx;
            int sy = (stars[i].y * 50.0 / stars[i].z) + cy;
            if (sx >= 0 && sx < 160 && sy >= 0 && sy < 80) {
                uint8_t c = map(stars[i].z, 1, 200, 255, 80);
                uint16_t color = tft2.color565(c, c, c);
                int size = 1;
                if (stars[i].z < 60) {
                    size = 3;       // Velmi blízko = čtvereček 3x3 px
                } else if (stars[i].z < 130) {
                    size = 2;       // Ve středu = čtvereček 2x2 px
                }
                tft2.fillRect(sx, sy, size, size, color);
                stars[i].lastSx = sx;
                stars[i].lastSy = sy;
                stars[i].lastSize = size;
            } else {
                stars[i].z = 0; 
                stars[i].lastSx = -1;
            }
        }
            vTaskDelay(30 / portTICK_PERIOD_MS); 
    }
    delete[] stars;
                                  
      tft2.fillScreen(ST77XX_BLACK);
      tft2.setFont(); // Návrat k výchozímu fontu
      tft2.setTextSize(2); 
      tft2.setTextColor(tft2.color565(0, 255, 0)); // Zářivě zelená
      tft2.setCursor(8, 32); 
      tft2.print("SYSTEM READY");
                  tone(40, 600, 150);  // 3. tón: Nízký (600 Hz) na 250 ms (schválně trochu delší)
                    delay(200);          
                  tone(40, 800, 150);  // 2. tón: Střední (800 Hz) na 150 ms
                    delay(200); 
                  tone(40, 1000, 150); // 1. tón: Vysoký (1000 Hz) na 150 ms        
       vTaskDelay(2000 / portTICK_PERIOD_MS);
       tft2.fillScreen(ST77XX_BLACK);
       vTaskDelete(NULL); 
}

// ========================== 23. DIAGNOSTIKA AUDIA (PRO SÉRIOVÝ MONITOR) =================
void audio_info(const char *info) {
    Serial.print("AUDIO INFO: ");
    Serial.println(info);
}

//=========================== 24. Pomocná funkce pro pípání na pinu 40
void pipni(int pocet) {
    for (int i = 0; i < pocet; i++) {
        tone(40, 1000); // Frekvence 2000 Hz
        delay(100);     // Délka tónu 100 ms
        noTone(40);     // Vypnutí tónu
        if (pocet > 1) {
            delay(100); // Mezera mezi pípnutími (pouze pokud pípá vícekrát)
        }
    }
}

//======================================= SETUP ====================================

void setup() {
    Serial.begin(115200);
    delay(1000);
    pinMode(TFT_BL, OUTPUT);
    pinMode(TFT2_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);  // Velký TFT1 naplno
    digitalWrite(TFT2_BL, HIGH); // Malý TFT2 naplno
    delay(500); // Necháme je svítit, než se začne inicializovat SPI

    Audio::audio_info_callback = my_audio_events;
    pinMode(BATTERY_PIN, INPUT);
    scrollCanvas.setTextWrap(false);
    pinMode(BUTTON_NEXT, INPUT_PULLUP);
    pinMode(BUTTON_PREV, INPUT_PULLUP);
    pinMode(CHARGE_PIN, INPUT);

    pinMode(PIN_TLACITKO_BT, INPUT_PULLUP); // Interní pull-up pro tlačítko
    pinMode(PIN_MAX_NAPAJENI, OUTPUT);      // Pin pro ovládání relé
    digitalWrite(PIN_MAX_NAPAJENI, LOW);   // Výchozí stav po zapnutí: Hrají reproduktory
    
    //================ Start velkeho TFT ===================
    SPI.begin(TFT_SCL, -1, TFT_SDA, TFT_CS);
    tft.initR(INITR_BLACKTAB);
    tft.setRotation(1);

    // 🚀🚀 ODPÁLENÍ PWM PRO OBA DISPLEJE ZVLÁŠŤ 🚀🚀
    ledcAttach(TFT_BL, 5000, 8); 
    ledcAttach(TFT2_BL, 5000, 8);
    ledcWrite(TFT_BL, 255);      
    ledcWrite(TFT2_BL, 255);
    
    //=============== Start malého vizualizéru ============
    tft2.initR(INITR_MINI160x80); 
    tft2.setRotation(1);           
    uint8_t madctl = 0x68; 
    tft2.sendCommand(0x36, &madctl, 1);
    tft2.fillScreen(ST77XX_BLACK);
    kittRunning = true;
    xTaskCreatePinnedToCore(kittTask, "KittTask", 2048, NULL, 1, NULL, 0);
    
    //================== UVODNI OBRAZOVKA ==========================
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_CYAN);
    tft.setCursor(10, 25); 
    tft.print("RADIO GEMINI");
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_YELLOW);
    tft.setCursor(5, 50);
    tft.print("Ver.III " + fwVersion + " S3 Edition"); 
    tft.setTextColor(0x7BEF);
    tft.setCursor(10, 75);
    tft.print("Collaboratively built by");
    tft.setCursor(10, 85);
    tft.print("You & Gemini Pro (2026)");       
    delay(1000); 
//================================= Pripojovani =============
    tft.fillRect(0, 95, 160, 33, ST77XX_BLACK);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(10, 102);
    tft.print("Pripojuji k WiFi...");
    tft.drawRoundRect(10, 115, 140, 8, 3, ST77XX_BLUE);

    WiFi.setSleep(false);
    wifiMulti.addAP("MTN", "4v2474MD");
    //wifiMulti.addAP("MTN EXT", "4v2474MD");
    wifiMulti.addAP("DP-Ridici", "GAMen3V_dpq-");
    wifiMulti.addAP("Y", "4v2474MD");
    wifiMulti.addAP("webradio", "webradio");
    
    unsigned long startWifiTime = millis();
    bool wifiWarningShown = false;
    
    int barPos = 0;
    int barDir = 1;

    while (WiFi.status() != WL_CONNECTED) {
      
        tft.fillRoundRect(11, 116, 138, 6, 2, ST77XX_BLACK); // Vyčistí vnitřek
        tft.fillRoundRect(11 + barPos, 116, 30, 6, 2, ST77XX_GREEN); // Vykreslí zelený blok
        
        barPos += (barDir * 18); // Rychlost
        if (barPos > 108) { barPos = 108; barDir = -1; } // Odraz od pravého okraje
        if (barPos < 0)   { barPos = 0;   barDir = 1; }  // Odraz od levého okraje

        // LUXUSNÍ VYSKAKOVACÍ OKNO po 8 vteřinách
        if (!wifiWarningShown && (millis() - startWifiTime > 8000)) {
            wifiWarningShown = true;
            
            uint16_t popupBg = tft.color565(150, 0, 0); 
            tft.fillRoundRect(10, 30, 140, 70, 5, popupBg);
            tft.drawRoundRect(10, 30, 140, 70, 5, ST77XX_WHITE);
            tft.setTextColor(ST77XX_WHITE);
            tft.setCursor(18, 38);
            tft.print("! WIFI NENALEZENA !");
            
            tft.drawLine(10, 50, 150, 50, ST77XX_WHITE);
            tft.setCursor(15, 56);
            tft.print("Vytvor Hotspot:");
            
            tft.setTextColor(ST77XX_YELLOW);
            tft.setCursor(15, 70);
            tft.print("NAZEV: webradio");
            tft.setCursor(15, 82);
            tft.print("HESLO: webradio");
        }
        wifiMulti.run();
        delay(30); 
    }
    kittRunning = false;
    delay(400); 
    
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org");
 
    tft.fillRect(0, 60, 160, 68, ST77XX_BLACK); 
    tft.setTextColor(ST77XX_GREEN);
    tft.setCursor(10, 65);
    tft.print("Pripojeno k WiFi!");
    
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(10, 85);
    tft.print("SSID: ");
    tft.setTextColor(ST77XX_YELLOW);
    tft.print(WiFi.SSID());
    
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(10, 100);
    tft.print("IP: ");
    tft.setTextColor(ST77XX_CYAN);
    tft.print(WiFi.localIP());
    delay(2000); 

    preferences.begin("radio", false);
    currentStation = preferences.getInt("station", 0);
    curVolume = preferences.getInt("volume", 80);  
    eqLow = preferences.getInt("eqLow", 0);
    eqMid = preferences.getInt("eqMid", 0);
    eqHigh = preferences.getInt("eqHigh", 0);
    
    audio.setTone(eqLow, eqMid, eqHigh);
    if (currentStation < 0 || currentStation >= stationCount) currentStation = 0;
    if (curVolume < 0 || curVolume > 100) curVolume = 80;  
    
    updateSvatek();
    
    audio.setConnectionTimeout(2000, 7200);
    audio.forceMono(false);
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(map(curVolume, 0, 100, 0, 21)); 
    connectStation();

    //============= WEB A WEBSOCKET SERVER
    server.on("/", handleRoot);
    server.on("/set", handleSetStation);
    server.on("/volup", handleVolUp);
    server.on("/voldown", handleVolDown);
    server.on("/mute", handleMute);
    server.on("/sleep", handleSleepTimer);
    server.on("/power", handlePower);
    server.on("/eq", handleEQ);
    server.on("/status", handleStatus);
    server.on("/song", []() { server.send(200, "text/plain", aktualniPisnicka); });
    server.on("/vol", handleVolume); 
    server.begin();
    
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);

    if (!MDNS.begin("radio")) {
        Serial.println("Chyba mDNS");
    } else {
        Serial.println("mDNS bezi na http://radio.local");
        MDNS.addService("http", "tcp", 80);
    }
} 

//================================== LOOP ==============================================================
void loop() {
    audio.loop();
    server.handleClient();
    webSocket.loop(); 
    drawVisualizer();
    
// 🚀🚀 ========= 1. AUTOMATICKÉ KALIBROVANÉ PODSVÍCENÍ DISPLEJŮ ========= 🚀🚀

    static unsigned long lastLdrTime = 0;
    if (millis() - lastLdrTime > 50) { 
        lastLdrTime = millis();
        int lightLevel = analogRead(LDR_PIN);
        int targetBrightness = map(lightLevel, 200, 3000, 15, 255);
        targetBrightness = constrain(targetBrightness, 15, 255);
        static float actualBrightness = 255;
        actualBrightness = (actualBrightness * 0.9) + (targetBrightness * 0.1);
        ledcWrite(TFT_BL, (int)actualBrightness); 
        
        // MALÝ DISPLEJ (TFT2 - Pin 14)
        // Tady je ten trik! Snížíme mu "maximální strop", aby odpovídal velkému
        // Zkus začít na 0.3 (tedy 30 % výkonu velkého) a uvidíš
        float koeficientMaly = 0.5;      //======================================================= JAS TFT 2 ======
        int brightnessMaly = (int)(actualBrightness * koeficientMaly);
        ledcWrite(TFT2_BL, brightnessMaly); 
    }

    // === NEBLOKUJÍCÍ DOKONČENÍ NÁVRATU Z BLUETOOTH ===
    if (probihaNavratZBT && (millis() - casStartuNavratu > 700)) {  //=== Cas blokujici na srovnani hlasitosti
        probihaNavratZBT = false; // Hotovo, odpočet končí
        
        // Nyní bezpečně přepneme relé
        digitalWrite(PIN_MAX_NAPAJENI, LOW);  
        Serial.println("REŽIM: Reproduktory");
        
        // Vrátíme původní hlasitost
        if (savedVolumeBeforeBT != -1) {     
             doVolume(savedVolumeBeforeBT);
             savedVolumeBeforeBT = -1;
        }
        connectStation();
        // Obnovíme displej
        if (isMuted) drawOutputIcon(); 
        else drawScreen(); 
    }

// 🔋 ========= 2.KONTROLA SLABÉ BATERIE (VÍCE ÚROVNÍ) ========= 🔋

    static unsigned long lastBatWarnCheck = 0;
    static bool probihaVarovaniBaterie = false;
    static unsigned long casStartuVarovani = 0;
    static int naposledyVarovanoPri = 100; 
           // 2a. FÁZE: Spuštění varování
    if (millis() - lastBatWarnCheck > 5000) { 
        lastBatWarnCheck = millis();
        int bat = getBatteryPercent();
        bool isCharging = (analogReadMilliVolts(CHARGE_PIN) > 2100); 
        if (isCharging) {
            naposledyVarovanoPri = 100; // Při připojení na nabíječku resetujeme paměť varování
        } 
        else if (isRadioPlaying && !probihaVarovaniBaterie) {
            int aktualniHranice = 0;
            if (bat <= 5) aktualniHranice = 5;
            else if (bat <= 10) aktualniHranice = 10;
            else if (bat <= 15) aktualniHranice = 15;
            else if (bat <= 20) aktualniHranice = 20;
            else if (bat <= 30) aktualniHranice = 30;
              if (aktualniHranice > 0 && naposledyVarovanoPri > aktualniHranice) {
                naposledyVarovanoPri = aktualniHranice; // Uložíme si, že pro tuto hranici je už varováno
                probihaVarovaniBaterie = true;
                casStartuVarovani = millis();   
                audio.stopSong();               
                      // Vykreslení velkého červeného okna
                tft.fillRoundRect(10, 37, 140, 69, 8, ST77XX_RED);
                tft.drawRoundRect(10, 37, 140, 69, 8, ST77XX_WHITE);
                tft.setTextColor(ST77XX_YELLOW);
                tft.setTextSize(2);
                tft.setCursor(50, 45); 
                tft.print("POZOR");
                tft.setTextColor(ST77XX_WHITE);
                tft.setTextSize(1, 2); 
                tft.setCursor(35, 75); // Posunuto mírně doleva (text je delší)
                tft.print("BATERIE POD ");
                tft.print(aktualniHranice); // Vypíše aktuální varovnou hranici (20, 15, 10 nebo 5)
                tft.print("%");
                 // --- TŘI KLESAJÍCÍ TÓNY (ZVUKOVÉ VAROVÁNÍ) ---
                 tone(40, 1000, 150); // 1. tón: Vysoký (1000 Hz) na 150 ms
                  delay(200);          
                 tone(40, 800, 150);  // 2. tón: Střední (800 Hz) na 150 ms
                  delay(200);         
                 tone(40, 600, 250);  // 3. tón: Nízký (600 Hz) na 250 ms (schválně trochu delší) 
           }
        }
    }
        //======== 2b. FÁZE: Dokončení varování (NEBLOKUJÍCÍ NÁVRAT PO 4 VTEŘINÁCH)
    if (probihaVarovaniBaterie && (millis() - casStartuVarovani > 4000)) {
        probihaVarovaniBaterie = false;  // Hotovo, okno zmizí
        connectStation();                // Znovu se připojí k rádiu a rozjede muziku
        if (isMuted) drawOutputIcon(); 
        else drawScreen();               // Překreslí se zpět do normálu
    }
    
    // ========= 3.KONTROLA WIFI A ZNOVUPŘIPOJENÍ =========
    if (WiFi.status() != WL_CONNECTED) {
        static unsigned long lastWifiRetry = 0;
        if (millis() - lastWifiRetry > 500) { 
            lastWifiRetry = millis();
            
            if (wifiMulti.run() == WL_CONNECTED) {
                if (isRadioPlaying) {
                    String connectedSSID = WiFi.SSID();
                    tft.fillRect(0, 106, 160, 22, 0x2104); 
                    tft.drawLine(0, 106, 160, 106, ST77XX_BLUE);
                     tone(40, 800, 100); 
                     delay(100); 
                     tone(40, 800, 100); 
                     delay(100);   
                     tone(40, 800, 100);    
                    tft.setFont();
                    tft.setTextSize(1,2);
                    tft.setTextColor(ST77XX_GREEN, 0x2104); 
                    tft.setCursor(5, 112);
                    tft.print("WiFi OK: ");
                    tft.print(connectedSSID);
                    delay(2000); 
                    connectStation(); 
                    updateIcons(); 
                }
            } else {
                if (isRadioPlaying) {
                      tone(40, 800, 100); 
                      delay(200);          
                      tone(40, 800, 300);                     // 2. tón: Střední (800 Hz) na 100 ms
                    tft.fillRect(0, 106, 160, 22, 0x2104);
                    tft.setCursor(5, 115);
                    tft.setTextColor(ST77XX_RED, 0x2104);
                    tft.print("Ztrata WiFi... hledam");
                    updateIcons();
                }
            }
        }
        
        if (isRadioPlaying) {
            return; 
        }
    }

   // ============== 4a. TLAČÍTKO NEXT ==============
    if (digitalRead(BUTTON_NEXT) == LOW) {
        if (nextPressStart == 0) {
            tone(40, 700, 100); 
            nextPressStart = millis();
            nextLongFired = false;
            if (!isRadioPlaying) {
                isRadioPlaying = true;
                connectStation();      
                drawScreen();          
                lastDrawnBattery = -1; 
                updateIcons();
                webSocket.broadcastTXT("POWER:1"); 
            }
        }
        if (!isSelecting && !nextLongFired && (millis() - nextPressStart > 600)) {
            isSelecting = true;
            nextLongFired = true;
            lastActionTime = millis();
            drawScreen();
        }
    } else if (nextPressStart != 0) {
        lastActionTime = millis();
        if (!nextLongFired) {
            if (isSelecting) {
                currentStation = (currentStation - 1 + stationCount) % stationCount;
                drawScreen();
            } else {
                   doVolume(min(curVolume + 2, 100));
            }
        }
        nextPressStart = 0;
    }

    // ============== 4b. TLAČÍTKO PREV ==============
    if (digitalRead(BUTTON_PREV) == LOW) {
        if (prevPressStart == 0) {
            tone(40, 600, 100);
            prevPressStart = millis();
            prevLongFired = false;
            if (!isRadioPlaying) {
                isRadioPlaying = true;
                connectStation();      
                drawScreen();          
                lastDrawnBattery = -1; 
                updateIcons();
                webSocket.broadcastTXT("POWER:1"); 
            }
        }
        if (!isSelecting && !prevLongFired && (millis() - prevPressStart > 600)) {
            isSelecting = true;
            prevLongFired = true;
            lastActionTime = millis();
            drawScreen();
        }
    } else if (prevPressStart != 0) {
        lastActionTime = millis();
        if (!prevLongFired) {
            if (isSelecting) {
                currentStation = (currentStation + 1) % stationCount; 
                drawScreen();
            } else {
                doVolume(max(curVolume - 2, 0));
            }
        }
        prevPressStart = 0;
    }

    if (isSelecting && (millis() - lastActionTime > 3000)) {                   
        isSelecting = false;
        doSetStation(currentStation); 
    }
    if (millis() - lastActionTime > 2000 && lastDrawnTitle == "VOL_CHANGE") {          
        lastDrawnTitle = ""; 
        if (isRadioPlaying) drawTitle(); 
    }
    if (currentTitle == "Nacitam buffer..." && (millis() - streamConnectTime > 4000)) {
        currentTitle = "Prehravam..."; 
        if (isRadioPlaying) drawTitle(); 
    }
    static unsigned long lastTimeUpdate = 0;
    static int lastDay = -1; 
      if (millis() - lastTimeUpdate > 1000) {
        lastTimeUpdate = millis();
      struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            if (lastDay == -1) {
                lastDay = timeinfo.tm_mday; 
            } else if (timeinfo.tm_mday != lastDay) {
                lastDay = timeinfo.tm_mday; 
                updateSvatek();              
                prepareScrollText();        
            }
        }

        if (!isSelecting && isRadioPlaying) {
            drawTimeOnly(); 
            updateIcons();
        }
    }


//================== 4c.Čtení tlačítka pro přepínání relé a MUTE (BT/OUT)
    if (digitalRead(PIN_TLACITKO_BT) == LOW) {
        if (btPressStart == 0) {
            btPressStart = millis();
            btLongFired = false;
        }
        
        // --- DLOUHÝ STISK (> 800 ms) = PŘEPNUTÍ BT/OUT ---
        if (!btLongFired && (millis() - btPressStart > 800)) {
            btLongFired = true;
            rezimSluchatka = !rezimSluchatka; // Přepne stav relé
            pipni(2);
            if (rezimSluchatka) {
                digitalWrite(PIN_MAX_NAPAJENI, HIGH); 
                Serial.println("REŽIM: Bluetooth");
                
                savedVolumeBeforeBT = curVolume;
                doVolume(100);
                
                drawOutputIcon(); 
            } else {
                // === NEBLOKUJÍCÍ OPRAVA ZTLUMENÍ ===
               // === OPRAVA: ZASTAVENÍ STREAMU A ZTLUMENÍ ===
                audio.stopSong();
                audio.setVolume(0);           // Ztlumíme rovnou na čipu
                probihaNavratZBT = true;      // Spustíme neblokující odpočet
                casStartuNavratu = millis();  // Uložíme si aktuální čas
                // Relé zatím NEPŘEPÍNÁME, čekáme...
            }
            webSocket.broadcastTXT(isMuted ? "OUT:MUTE" : (rezimSluchatka ? "OUT:BT" : "OUT:SPK"));
        }

        
    } else if (btPressStart != 0) {
        // --- KRÁTKÝ STISK (Při puštění tlačítka) = MUTE ---
        if (!btLongFired && (millis() - btPressStart > 50)) { // 50ms proti zákmytům
           pipni(1);
            if (isMuted) {
                isMuted = false;
                doVolume(preMuteVolume); // Vrátí předchozí hlasitost
                  if (rezimSluchatka) drawOutputIcon(); // Návrat k modré BT ikoně
                    else drawScreen(); // Návrat k logu stanice
                      } else {
                isMuted = true;
                preMuteVolume = curVolume; // Uloží si hlasitost pro pozdější návrat
                doVolume(0); // Úplně ztlumí zvuk
                drawOutputIcon(); // Vykreslí červenou ikonku MUTE
            }
           webSocket.broadcastTXT(isMuted ? "OUT:MUTE" : (rezimSluchatka ? "OUT:BT" : "OUT:SPK"));
        }
        btPressStart = 0; // Reset stisku
    }
    
// ================== 5a.POHYB ROLOVACÍHO TEXTU ===============================
    // --- 1. HORNÍ TEXT (Datum a Svátek) ---
    if (millis() - lastScrollTime > 25) {                     // <-- Zde je rychlost horního textu (20 ms)
        lastScrollTime = millis();
        if (isRadioPlaying) {
            scrollCanvas.fillScreen(ST77XX_BLACK); 
            scrollCanvas.setTextColor(ST77XX_YELLOW);
            int mezera = 30;                                
            int textWidth = (scrollText.length() * 6) + mezera;
            scrollCanvas.setCursor(scrollPos, 2);
            scrollCanvas.print(scrollText);
            scrollCanvas.setCursor(scrollPos + textWidth, 2);
            scrollCanvas.print(scrollText);
            tft.drawRGBBitmap(0, 25, scrollCanvas.getBuffer(), 160, 12);
            scrollPos--;
            if (scrollPos <= -textWidth) {
                scrollPos += textWidth; 
            }
        }
    }

    //=============== 5b. DOLNÍ TEXT (Metadata) =======================
    static unsigned long lastMetaScrollTime = 0;
    int metaSpeed = 20;                                     // <-- ZDE NASTAVÍŠ RYCHLOST METADAT (větší číslo = pomalejší posuv)

    if (isRadioPlaying && needsTitleScroll && lastDrawnTitle != "VOL_CHANGE") {
        if (millis() > lastTitleScrollTime && (millis() - lastMetaScrollTime > metaSpeed)) {
            lastMetaScrollTime = millis();
            
            int tWidth = currentTitle.length() * 6;
            int tGap = 40; 
            titleScrollPos--;
            
            if (titleScrollPos <= -(tWidth + tGap) + 5) {
                titleScrollPos = 5; 
                lastTitleScrollTime = millis() + 800;       // <-- ZDE NASTAVÍŠ PAUZU (1500 = 1.5 vteřiny čeká, než znovu vyjede)
            }
            updateTitleCanvas(); 
        }
    }

    // =============== 6.SLEEP TIMER KONTROLA =====================
    if (isSleepTimerActive) {
        if (millis() - sleepTimerStart >= sleepTimerDuration) {
            isSleepTimerActive = false;
            currentSleepValue = 0;     
            audio.stopSong();          
            isRadioPlaying = false;
            drawClock();               
            webSocket.broadcastTXT("SLEEP:0");
            webSocket.broadcastTXT("POWER:0");
        }
    }

    // ========= 7. AKTUALIZACE HODIN VE STANDBY REŽIMU =========
    static int lastMinute = -1;
    if (!isRadioPlaying) {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            if (timeinfo.tm_min != lastMinute) { 
                lastMinute = timeinfo.tm_min;
                drawClock(); 
            }
        }
    }
 }
