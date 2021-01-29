#include <EEPROMex.h>
#include <EEPROMVar.h>
#include "TinyGPS++.h"
#include <SoftwareSerial.h>
#include <LiquidCrystal.h>

// inne stale
static const int GPSBaud = 9600;
static const int BUTTON_DELAY = 400; 

// piny
static const int ACCEPT_PIN = 2;
static const int DECLINE_PIN = 3;
static const int LCD_LED_PIN = 11; // 10
static const int TX_PIN = 12; // 11
static const int RX_PIN = 13; // 12

// standardowe stany dzialania
static const int POSITION = 0; 
static const int SPEED = 1;
static const int ALTITUDE = 2; 
static const int DESTINATION = 3;
static const int DISTANCE = 4; 
static const int NAVIGATION = 5; 
static const int DATETIME = 6; 
static const int SIGNAL = 7;

// niestandardowe stany dzialania
static const int CLICK = 8; 
static const int ACCEPT = 9; 
static const int ERROR = 10; 

// inicjacja zmiennych
static volatile int current_mode = 0;
static volatile int prev_mode = 0;
static volatile int prev_helper_mode = 0;

// miejsce dla ustawien w EEPROM
static const int EE_LATITUDE = EEPROM.getAddress(sizeof(double));
static const int EE_LONGITUDE = EEPROM.getAddress(sizeof(double));
static const int EE_LIGHT_VAR = EEPROM.getAddress(sizeof(int));
static const int EE_ALT_VAR = EEPROM.getAddress(sizeof(int));
static const int EE_SPEED_VAR = EEPROM.getAddress(sizeof(int));
static const int EE_NAVIGATE = EEPROM.getAddress(sizeof(bool));
static const int EE_DIST_VAR = EEPROM.getAddress(sizeof(int));

// ustawienia
double hLatitude = 0.0;
double hLongitude = 0.0;
int light_var = 0;
int alt_var = 0;
int speed_var = 0;
bool navigate = false;
int dist_var = 0;

unsigned long interrupt_time = 0;
LiquidCrystal lcd(5, 6, 7, 8, 9, 10);
SoftwareSerial ss(RX_PIN, TX_PIN);
TinyGPSPlus gps;

////////////////////////////////////// GLOWNE ////////////////////////////////////////////////////////////

 /**
 * Stworzenie obslugi przerwan, inicjacja
 */
void setup() {
  pinMode(DECLINE_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(DECLINE_PIN), decline_button, RISING);
  pinMode(ACCEPT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ACCEPT_PIN), accept_button, RISING);
  ss.begin(GPSBaud);
  lcd.begin(16, 2);
  readApplySettings();
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
         case POSITION: // standardowe
            gps_position();
            break;
         case SPEED:
            gps_speed();
            break;
         case ALTITUDE:
            gps_altitude();
            break;
         case DESTINATION:
            gps_destination();
            break;
         case DISTANCE:
            gps_distance();
            break;
         case NAVIGATION:
            gps_navigation();
            break;
         case DATETIME:
            gps_datetime();
            break;
        case SIGNAL:
            gps_signal();
            break;
        case CLICK: // niestandardowe
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
      current_mode = current_mode >= SIGNAL // ostatni standardowy tryb
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
    case DESTINATION:
      print(F("USTAWIC NOWY CEL"), F("NIE          TAK"));
      break;
    case SPEED:
    case ALTITUDE:
    case DISTANCE:
      print(F("ZMIANA JEDNOSTKI"), F("NIE          TAK"));
      break;
    case NAVIGATION:
      print(F("ZMIANA FORMATU"), F("NIE          TAK"));
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
    case DESTINATION:
      hLatitude = gps.location.lat();
      hLongitude = gps.location.lng();
      if (EEPROM.isReady()) {
        EEPROM.writeDouble(EE_LATITUDE, hLatitude);
        EEPROM.writeDouble(EE_LONGITUDE, hLongitude);
      }
      break;
    case SPEED:
      speed_var = speed_var >= 3 ? 0 : speed_var + 1; 
      if (EEPROM.isReady()) {
        EEPROM.writeInt(EE_SPEED_VAR, speed_var);
      }
      break;
    case ALTITUDE:
      alt_var = alt_var >= 3 ? 0 : alt_var + 1; 
      if (EEPROM.isReady()) {
        EEPROM.writeInt(EE_ALT_VAR, alt_var);
      }
      break;
    case NAVIGATION:
      navigate = !navigate;
      if (EEPROM.isReady()) {
        EEPROM.writeBit(EE_NAVIGATE, 0, navigate);
      }
      break;
    case DISTANCE:
      dist_var = dist_var >= 3 ? 0 : dist_var + 1; 
      if (EEPROM.isReady()) {
        EEPROM.writeInt(EE_DIST_VAR, dist_var);
      }
      break;
    default:
      light_var = light_var >= 3 ? 0 : light_var + 1; 
      analogWrite(LCD_LED_PIN, light_var == 0 ? 0 : light_var * 64 - 1);
      if (EEPROM.isReady()) {
        EEPROM.writeInt(EE_LIGHT_VAR, light_var);
      }
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
 * Aktualna pozycja
 */
void gps_position() {
  if (gps.location.isUpdated()) {
    print("AKT " + String(gps.location.lat(), 6), "POZ " + String(gps.location.lng(), 6));
  }
}

/**
 * Predkosc
 */
void gps_speed() {
  if (gps.speed.isUpdated()) {
    if (speed_var == 0) {
      print(F("PREDKOSC"), String(gps.speed.knots()) + " wezlow");
    } else if (speed_var == 1) {
      print(F("PREDKOSC"), String(gps.speed.mph()) + " mph");
    } else if (speed_var == 2) {
      print(F("PREDKOSC"), String(gps.speed.mps()) + " mps");
    } else {
      print(F("PREDKOSC"), String(gps.speed.kmph()) + " kmh");
    }
  }
}

/**
 * Aktualna wysokosc
 */
void gps_altitude() {
  if (gps.altitude.isUpdated()) {
    if (alt_var == 0) {
      print(F("WYSOKOSC NPM"), String(gps.altitude.meters()) + " m");
    } else if (alt_var == 1) {
      print(F("WYSOKOSC NPM"), String(gps.altitude.miles()) + " mil");
    } else if (alt_var == 2) {
      print(F("WYSOKOSC NPM"), String(gps.altitude.kilometers()) + " km");
    } else {
      print(F("WYSOKOSC NPM"), String(gps.altitude.feet()) + " ft");
    }
  }
}


/**
 * Daty i godzina
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
    if (dist_var == 0) {
      print(F("ODLEGLOSC CEL"), String(distance) + " m");
    } else if (dist_var == 1) {
      print(F("ODLEGLOSC CEL"), String(_GPS_MILES_PER_METER * distance) + " mil");
    } else if (dist_var == 2) {
      print(F("ODLEGLOSC CEL"), String(_GPS_KM_PER_METER * distance) + " km");
    } else {
      print(F("ODLEGLOSC CEL"), String(_GPS_FEET_PER_METER * distance) + " ft");
    }
  }
}


/**
 * Ilosc satelit i sygnal
 */
void gps_signal() {
  if (gps.satellites.isUpdated()) {
    print("SATELITY: " + String(gps.satellites.value()), "HDOP: " + String(gps.hdop.value()));
  }
}



/**
 * Nawigacja do zapisanego miejsca
 */
void gps_navigation() {
  if (gps.location.isUpdated()) {
    double course = TinyGPSPlus::courseTo(
                  gps.location.lat(), gps.location.lng(),
                  hLatitude, hLongitude);
    if (navigate == true) {
      print(F("DROGA DO CELU"), "      " + String(TinyGPSPlus::cardinal(course)) + "      ");
    } else {
      print(F("DROGA DO CELU"), String(course) + " deg");
    }
  }
}

/**
 * Aktywny cel
 */
void gps_destination() {
  if (gps.location.isUpdated()) {
    print("AKT " + String(hLatitude, 6), "CEL " + String(hLongitude, 6));
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
 * Odczytanie ustawien z EEPROM
 */ 
void readApplySettings() {
  if (EEPROM.isReady()) {
     hLatitude = EEPROM.readDouble(EE_LATITUDE);
     hLongitude = EEPROM.readDouble(EE_LONGITUDE);
     light_var = EEPROM.readInt(EE_LIGHT_VAR);
     alt_var = EEPROM.readInt(EE_ALT_VAR);
     speed_var = EEPROM.readInt(EE_SPEED_VAR);
     navigate = EEPROM.readBit(EE_NAVIGATE, 0);
     dist_var = EEPROM.readInt(EE_DIST_VAR);
  }
  analogWrite(LCD_LED_PIN, light_var == 0 ? 0 : light_var * 64 - 1);
}