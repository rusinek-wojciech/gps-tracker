#include <EEPROMex.h>
#include <EEPROMVar.h>
#include "TinyGPS++.h"
#include <SoftwareSerial.h>
#include <LiquidCrystal.h>

// inne stale
static const uint32_t GPSBaud = 9600;
static const uint32_t EE0 = EEPROM.getAddress(sizeof(double));
static const uint32_t EE1 = EEPROM.getAddress(sizeof(double));

// piny
static const uint32_t OK_BUTTON = 2;
static const uint32_t NEXT_BUTTON = 3;
static const uint32_t LCD_LED = 10;
static const uint32_t TX_PIN = 11;
static const uint32_t RX_PIN = 12;


// standardowe tryby
static const uint32_t POSITION = 0; 
static const uint32_t DATETIME = 1; 
static const uint32_t DISTANCE = 2; 
static const uint32_t ALTITUDE = 3; 
static const uint32_t NAVIGATION = 4; 

// niestandardowe tryby
static const uint32_t CLICK = 5; 
static const uint32_t ACCEPT = 6; 
static const uint32_t ERROR = 7; 

// inicjacja zmiennych
static volatile uint32_t current_mode = POSITION;
static volatile uint32_t prev_mode = POSITION;
static volatile uint32_t prev_helper_mode = POSITION;
double hLatitude = 0.0;
double hLongitude = 0.0;
boolean light_on = false;
boolean pressed = false;
unsigned long interrupt_time = 0;
LiquidCrystal lcd(4, 5, 6, 7, 8, 9);
SoftwareSerial ss(RX_PIN, TX_PIN);
TinyGPSPlus gps;

////////////////////////////////////// GLOWNE ////////////////////////////////////////////////////////////

 /**
 * Funkcja setup
 */
void setup() {
  ss.begin(GPSBaud);
  lcd.begin(16, 2);
  analogWrite(LCD_LED, 0);
  pinMode(NEXT_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(NEXT_BUTTON), next_button, RISING);
  pinMode(OK_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(OK_BUTTON), ok_button, RISING);
  readPosition();
}

/**
 * Glowna petla
 */
void loop() {
  while (ss.available() > 0) {
    if (gps.encode(ss.read())) {

        // czy jest blad
        if (!gps.location.isValid()) { 
          if (current_mode != CLICK && current_mode != ACCEPT) {
            prev_mode = DATETIME;
            current_mode = ERROR;
          }
        }

        // czyszczacz ekranu
        if (current_mode != prev_helper_mode) { 
          lcd.clear();
        }
        prev_helper_mode = current_mode;


        // glowne stany
        switch (current_mode) { 
         case POSITION:
          gps_pos();
          break;
         case DATETIME:
          gps_datetime();
          break;
         case DISTANCE:
          gps_distance();
          break;
         case ALTITUDE:
          gps_altitude();
          break;
         case NAVIGATION:
          gps_navigate();
          break;
        case CLICK:
          click();
          break;
        case ACCEPT:
          accept();
          break;
        case ERROR:
          error();
          break;
       }
    }
  }
  
  // brak danych z GPS
  if (millis() > 5000 && gps.charsProcessed() < 10) { 
    print(F("NIE WYKRYTO"), F("SPRAWDZ KABLE"));
    delay(5000);
  }
}

////////////////////////////////////// PRZERWANIA ////////////////////////////////////////////////////////////

/**
 * Do przerwan next button
 */
void next_button() {
  unsigned long this_time = millis(); // eliminacja "odbic"
  if (this_time - interrupt_time > 200) {

    if (current_mode == CLICK) {
      current_mode = prev_mode; // odrzuc
    } else if (current_mode == ERROR) {

    } else {
      current_mode = current_mode >= NAVIGATION // ostatni standardowy tryb
                ? POSITION  // pierwszy standardowy tryb
                : current_mode + 1; 
    }

  }  
  interrupt_time = this_time;
}

/**
 * Do przerwan ok button
 */
void ok_button() {
  unsigned long this_time = millis(); // eliminacja "odbic"
  if (this_time - interrupt_time > 200) {

    if (current_mode == CLICK) {
      current_mode = ACCEPT; // przyjmij
    } else if (current_mode == ERROR) {
      current_mode = CLICK;
    } else {
      prev_mode = current_mode;
      current_mode = CLICK;
    }

  }
  interrupt_time = this_time;
}

//////////////////////////////////// OBSLUGA STANOW PRZERWAN //////////////////////////////////////////////////////////////

void click() {
  switch (prev_mode) {
    case DISTANCE:
    case NAVIGATION:
      print(F("USTAWIC NOWA?"), F("NIE          TAK"));
      break;
    default:
      print(F("ZMIANA JASNOSCI"), F("NIE          TAK"));
      break;
  }
}

void accept() {
  switch (prev_mode) {
    case DISTANCE:
    case NAVIGATION:
      hLatitude = gps.location.lat();
      hLongitude = gps.location.lng();
      writePosition();
      break;
    default:
      analogWrite(LCD_LED, (light_on == true ? 0 : 255));
      light_on = !light_on;
      break;
  }
  current_mode = prev_mode;
}

void error() {
 print(F("TRWA LACZENIE"), F("Z GPS,CZEKAJ"));
}

//////////////////////////////////// OBSLUGA GLOWNYCH STANOW //////////////////////////////////////////////////////////////

/**
 * Wyswietlanie aktualnej pozycji
 */
void gps_pos() {
  if (gps.location.isUpdated()) {
    print(String(gps.location.lat(), 6) + " szer  ", String(gps.location.lng(), 6) + " dlug  ");
  }
}


/**
 * Wyswietlanie daty i godziny
 */
void gps_datetime() {
  if (gps.time.isUpdated() || gps.date.isUpdated()) {
    String date = gps.date.day() + String("-") + gps.date.month() + String("-") + gps.date.year();
    String time = getTime(); 
    print(date, time);
  }
}

/**
 * Odleglosc do domu
 */
void gps_distance() {
  if (gps.location.isUpdated()) { 
    double distance = TinyGPSPlus::distanceBetween(
                      gps.location.lat(), gps.location.lng(),
                      hLatitude, hLongitude);
    print(F("ODLEGLOSC CEL"), String(distance) + " m");
  }
}

/**
 * Wysokosc
 */
void gps_altitude() {
  if (gps.satellites.isUpdated() || gps.altitude.isUpdated()) {
    print("SATELITY: " + String(gps.satellites.value()), String(gps.altitude.meters()) + " m npm");
  }
}

void gps_navigate() {
  if (gps.location.isUpdated()) {
    double course = TinyGPSPlus::courseTo(
                    gps.location.lat(), gps.location.lng(),
                    hLatitude, hLongitude);
    print(F("DROGA DO CELU"), "      " + String(TinyGPSPlus::cardinal(course)) + "      ");
  }
}

///////////////////////////////////// POMOCNICZE /////////////////////////////////////////////////////////////

String getTime() {
  String hour = (gps.time.hour() < 10 ? "0" : "") + String(gps.time.hour());
  String minute = (gps.time.minute() < 10 ? "0" : "") + String(gps.time.minute());
  String second = (gps.time.second() < 10 ? "0" : "") + String(gps.time.second());
  return hour + String(":") + minute + String(":") + second;
}

void print(String msg1, String msg2) {
  lcd.setCursor(0, 0);
  lcd.print(msg1);
  lcd.setCursor(0, 1);
  lcd.print(msg2);
}

void readPosition() {
  if (EEPROM.isReady()) {
     hLatitude = EEPROM.readDouble(EE0);
     hLongitude = EEPROM.readDouble(EE1);
  }
}

void writePosition() {
  if (EEPROM.isReady()) {
     EEPROM.writeDouble(EE0, hLatitude);
     EEPROM.writeDouble(EE1, hLongitude);
  }
}
