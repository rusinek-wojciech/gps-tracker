#include <EEPROMex.h>
#include <EEPROMVar.h>
#include "TinyGPS++.h"
#include <SoftwareSerial.h>
#include <LiquidCrystal.h>

// inne stale
static const int GPSBaud = 9600;
static const int EE0 = EEPROM.getAddress(sizeof(double));
static const int EE1 = EEPROM.getAddress(sizeof(double));
static const int BUTTON_DELAY = 400; 

// piny
static const int ACCEPT_PIN = 2;
static const int DECLINE_PIN = 3;
static const int LCD_LED_PIN = 10;
static const int TX_PIN = 11;
static const int RX_PIN = 12;

// standardowe stany dzialania
static const int POSITION = 0; 
static const int DATETIME = 1; 
static const int DISTANCE = 2; 
static const int ALTITUDE = 3; 
static const int NAVIGATION = 4; 

// niestandardowe stany dzialania
static const int CLICK = 5; 
static const int ACCEPT = 6; 
static const int ERROR = 7; 

// inicjacja zmiennych
static volatile int current_mode = POSITION;
static volatile int prev_mode = POSITION;
static volatile int prev_helper_mode = POSITION;

// ustawienia
double hLatitude = 0.0;
double hLongitude = 0.0;
boolean light_on = false;

unsigned long interrupt_time = 0;
LiquidCrystal lcd(4, 5, 6, 7, 8, 9);
SoftwareSerial ss(RX_PIN, TX_PIN);
TinyGPSPlus gps;

////////////////////////////////////// GLOWNE ////////////////////////////////////////////////////////////

 /**
 * Stworzenie obslugi przerwan, inicjacja
 */
void setup() {
  ss.begin(GPSBaud);
  lcd.begin(16, 2);
  analogWrite(LCD_LED_PIN, 0);
  pinMode(DECLINE_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(DECLINE_PIN), decline_button, RISING);
  pinMode(ACCEPT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ACCEPT_PIN), accept_button, RISING);
  readPosition();
}

/**
 * Glowna petla
 */
void loop() {
  while (ss.available() > 0) {
    if (gps.encode(ss.read())) {

        // czy jest blad, ignoruj niestandardowe stany
        if (gps.location.isValid()) {
          if (current_mode == ERROR) {
            current_mode = prev_mode;
          }
        } else {
          if (current_mode != CLICK && current_mode != ACCEPT && current_mode != ERROR) {
            prev_mode = current_mode;
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
void decline_button() {
  unsigned long this_time = millis(); // eliminacja "odbic"
  if (this_time - interrupt_time > BUTTON_DELAY) {

    if (current_mode == CLICK) {
      current_mode = prev_mode; // odrzuc
    } else if (current_mode == ERROR) {
        // brak dzialania
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
void accept_button() {
  unsigned long this_time = millis(); // eliminacja "odbic"
  if (this_time - interrupt_time > BUTTON_DELAY) {

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

//////////////////////////////////// OBSLUGA NIESTANDARDOWYCH STANOW //////////////////////////////////////////////////////////////

/**
 * Pierwsze nacisniecie prawego przycisku
 */
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

/**
 * Klikniecie "TAK"
 */
void accept() {
  switch (prev_mode) {
    case DISTANCE:
    case NAVIGATION:
      hLatitude = gps.location.lat();
      hLongitude = gps.location.lng();
      writePosition();
      break;
    default:
      analogWrite(LCD_LED_PIN, (light_on == true ? 0 : 255));
      light_on = !light_on;
      break;
  }
  current_mode = prev_mode;
}

/**
 * Blad, brak sygnalu
 */
void error() {
 print(F("TRWA LACZENIE"), F("Z GPS,CZEKAJ"));
}

//////////////////////////////////// OBSLUGA STANDARDOWYCH STANOW //////////////////////////////////////////////////////////////

/**
 * Wyswietlanie aktualnej pozycji, zoptymalizowane
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
 * Odleglosc do zapisanej lokalizacji
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
 * Aktualna wysokosc oraz ilosc satelit
 */
void gps_altitude() {
  if (gps.satellites.isUpdated() || gps.altitude.isUpdated()) {
    print("SATELITY: " + String(gps.satellites.value()), String(gps.altitude.meters()) + " m npm");
  }
}

/**
 * Nawigacja do zapisanego miejsca
 */
void gps_navigate() {
  if (gps.location.isUpdated()) {
    double course = TinyGPSPlus::courseTo(
                    gps.location.lat(), gps.location.lng(),
                    hLatitude, hLongitude);
    print(F("DROGA DO CELU"), "      " + String(TinyGPSPlus::cardinal(course)) + "      ");
  }
}

///////////////////////////////////// POMOCNICZE /////////////////////////////////////////////////////////////

/**
 * String builder czasu
 */ 
String getTime() {
  String hour = (gps.time.hour() < 10 ? "0" : "") + String(gps.time.hour());
  String minute = (gps.time.minute() < 10 ? "0" : "") + String(gps.time.minute());
  String second = (gps.time.second() < 10 ? "0" : "") + String(gps.time.second());
  return hour + String(":") + minute + String(":") + second;
}

/**
 * Wyswietlanie na LCD
 */ 
void print(String msg1, String msg2) {
  lcd.setCursor(0, 0);
  lcd.print(msg1);
  lcd.setCursor(0, 1);
  lcd.print(msg2);
}

/**
 * Odczytanie lokalizacji z EEPROM
 */ 
void readPosition() {
  if (EEPROM.isReady()) {
     hLatitude = EEPROM.readDouble(EE0);
     hLongitude = EEPROM.readDouble(EE1);
  }
}

/**
 * Zapis lokalizacji do EEPROM
 */ 
void writePosition() {
  if (EEPROM.isReady()) {
     EEPROM.writeDouble(EE0, hLatitude);
     EEPROM.writeDouble(EE1, hLongitude);
  }
}
