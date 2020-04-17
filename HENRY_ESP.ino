/*
 * Such a huge work for a fraction of money.
 */

/*
 * Todo:
 * 1. Change 16 fractions to 32
 * 2. Add sound
 */

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <SPI.h>
#include "HX711.h"
#include <math.h>
#include <WiFi.h>
#include "MicroGear.h"
#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"
#include "credentials.h"

/* DEFINES */
#define TFT_CS        5
#define TFT_RST       4 // Or set to -1 and connect to Arduino RESET pin
#define TFT_DC        2
#define TFT_BLK       12
#include "medal.h" // include picture

// define colors
#define COLOR_BLUE      0x557A
#define COLOR_LIGHTBLUE 0xDF9F
#define COLOR_GREEN     0x960A
#define COLOR_YELLOW    0xF628 
#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800

#define LOADCELL_DOUT_PIN1 25
#define LOADCELL_SCK_PIN1  26

#define MAIN_BTN_PIN 33

#define SEC2MILLIS_RATIO 1000

#define REST_TIME         8  // 60 seconds
#define MESAURE_GRIP_TIME 8 // 120 seconds

// NETPIE
#define TARGET_ALIAS "server"


/* PROTOTYPES */
void drawClock(uint8_t clock_size=2, uint16_t color=COLOR_BLUE);
void drawCdtRing(uint8_t fraction, uint16_t color=COLOR_BLUE);
void drawNeedle(int8_t scale);


/* PV */
extern uint8_t medal[32768]; // medal photo
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

HX711 scale1;
const int32_t loadcell_base_1 = 140000;
const int32_t scale_factor    = 1000;  // can change this later
int32_t scale_reading = 0;
int16_t max_strength = 0; //kg
uint8_t number_of_set = 0;
uint16_t good_grip_cnt = 0;
uint16_t grip_cnt = 0;
uint8_t score = 0;
int8_t grip_level = 0;

uint32_t timer = 0;
uint32_t timediff = 0;
uint16_t rem_time = 0;

// display related
uint8_t old_fraction = 0;
uint8_t curr_fraction = 16;

// wireless
int wifi_status = WL_IDLE_STATUS;
int microgear_status = 0;
WiFiClient client;
MicroGear microgear(client);
String json = "";
String command = "";

// DFPlayer
SoftwareSerial mySoftwareSerial(15, 13); // RX, TX
DFRobotDFPlayerMini myDFPlayer;
uint8_t df_vloume = 20;


/* ENUMS */
typedef enum {
  STATE_STB = 0x00,     // standby
  STATE_MEASURE_MAX,    // measure maximum strength
  STATE_CONNECT_CLOUD,  // connect wifi and netpie
  STATE_EXERCISE,       // excercise
  STATE_REST,
  STATE_CLEANUP,
  STATE_TEST
} state_t;

state_t state = STATE_STB;



void setup(void) {
  Serial.begin(115200);
  print_wakeup_reason();

  mySoftwareSerial.begin(9600, SWSERIAL_8N1, 15, 13);

  // init TFT Display
  tft.initR(INITR_144GREENTAB);
  pinMode(TFT_BLK, OUTPUT); digitalWrite(TFT_BLK, HIGH); 
  tft.fillScreen(COLOR_BLACK);

  // init scales
  scale1.begin(LOADCELL_DOUT_PIN1, LOADCELL_SCK_PIN1);

  // init btn
  pinMode(MAIN_BTN_PIN, INPUT_PULLUP);

//  // init dfplayer
//  myDFPlayer.begin(mySoftwareSerial);
//  myDFPlayer.volume(df_vloume);  //Set volume value. From 0 to 30
//
//  // play turnon
//  myDFPlayer.play(0); delay(5000); myDFPlayer.pause();

  /* Add Microgear Event listeners */
  microgear.on(MESSAGE, onMsghandler); /* Call onMsghandler() when new message arraives */
  microgear.on(PRESENT, onFoundgear); /* Call onFoundgear() when new gear appear */
  microgear.on(ABSENT, onLostgear); /* Call onLostgear() when some gear goes offline */
  microgear.on(CONNECTED, onConnected); /* Call onConnected() when NETPIE connection is established */

  // start timer
  timer = millis();
  Serial.println("\r\n -- setup done -- \r\n");

}

void loop() {
  
  switch( state ) {
    case STATE_STB:
      // sleep for 10 seconds of inactivity
      if (millis() - timer >= 10 * SEC2MILLIS_RATIO) {
        Serial.println("Going to sleep...");
        esp_sleep_enable_ext0_wakeup(GPIO_NUM_33, 0);
        esp_deep_sleep_start();
      }

      // if user press main btn within 10 sec then start the process
      if (!digitalRead(MAIN_BTN_PIN)) {
        delay(200);
        timer = millis();
        state = STATE_MEASURE_MAX;
        Serial.println("Measuring Max Strength!");

        // play maxgrip
//        myDFPlayer.play(1); delay(3000); myDFPlayer.pause();

        // update screen
        drawCdtRing(16, COLOR_YELLOW);
        drawGauge();
        drawNeedle(0);

      }
      break;
      
    case STATE_MEASURE_MAX:
      if (scale1.is_ready()) {
        scale_reading = scale1.read();
        Serial.println(scale_reading);

        // update screen
        drawGripStr(get_weight(scale_reading));
        delay(200);
      }

      // change state when timeout
      if (millis() - timer >= 7 * SEC2MILLIS_RATIO) {
        timer = millis();
        state = STATE_CONNECT_CLOUD;
        Serial.println("Measuring completed. Connecting to cloud...");
        
        max_strength =  get_weight(scale_reading);// calculated to kg
        Serial.print("Max Strength: "); Serial.println(max_strength);
        
        // update screen
        drawGripStr(max_strength);

        // play green pls
//        myDFPlayer.play(8); delay(4000); myDFPlayer.pause();
      }
      
      break;
      
    case STATE_CONNECT_CLOUD:
      wifi_status = connect_wifi();
      if (wifi_status == WL_CONNECTED) {
        Serial.println("Wifi Connected");

        // connect to netpie
        microgear.init(KEY, SECRET, ALIAS);
        microgear.connect(APPID);
      } 
      else {
        Serial.println("Wifi Lost");
      }

      state = STATE_EXERCISE;
      drawCdtRing(16, COLOR_BLUE);
      timer == millis(); // start timer for exercise

      break;
      
    case STATE_EXERCISE:
      // update screen
      timediff = (millis() - timer)/1000;
      rem_time = MESAURE_GRIP_TIME - timediff;
      curr_fraction = (rem_time)/7.5;  // <----- Time fraction
      if (curr_fraction != old_fraction) {
        drawCdtRing(curr_fraction, COLOR_BLUE);
        old_fraction = curr_fraction; // update fraction
      }
      
      // measure grip strength
      if (scale1.is_ready()) {
        scale_reading = scale1.read();
        Serial.print("Max Strength: "); Serial.print(max_strength);
        Serial.print("   Optimal: "); Serial.print(max_strength/3);
        Serial.print("   Reading: "); Serial.println(get_weight(scale_reading));

        grip_cnt++;
        grip_level = get_grip_level(max_strength, scale_reading);        

        // grip ok
        if (grip_level == 0) {
          good_grip_cnt++;
        }

        // update screen
        drawSetCnt(number_of_set+1);
        drawGauge(); 
        drawNeedle(grip_level);
      }
      
      command = "LOL";

      // check if measuring is completed
      if (millis() - timer >= MESAURE_GRIP_TIME * SEC2MILLIS_RATIO) {
        // update screen
        // draw enlarging clock
        for (int i = 0; i < 2; ++i) {
          tft.fillScreen(COLOR_BLACK);
          drawClock(i, COLOR_BLUE);
          delay(500);
        }
        tft.fillScreen(COLOR_BLACK);
        drawClock(2, COLOR_RED);
        delay(500);
        
        // flow control
        if (number_of_set == 3) { // start from zero
          state = STATE_CLEANUP;
          score = (uint8_t)(((float)good_grip_cnt / (float)grip_cnt)*100);
          command = "END";
        }
        else {
          state = STATE_REST;
          number_of_set++;

          old_fraction = 0;
          curr_fraction = 0;
        }

        Serial.print("Number of Set: "); Serial.print(number_of_set+1);
        Serial.print("   Grip count: "); Serial.print(grip_cnt);
        Serial.print("   Good cound: "); Serial.println(good_grip_cnt);

        timer = millis(); // start timer for resting
      }

      break;

    case STATE_REST:
//      Serial.println("resting...");

      // update screen
      timediff = (millis() - timer)/1000;
      rem_time = REST_TIME - timediff;
      curr_fraction = (rem_time)/1;  // <--- Time fraction

      if (curr_fraction != old_fraction) {
        drawCdtRing(curr_fraction, COLOR_BLUE);
        old_fraction = curr_fraction; // update fraction

        if (REST_TIME - timediff < 4) {
          drawClock(2, COLOR_BLACK); // shadow the clock
        } else {
          drawClock(2, COLOR_BLUE);
        }
      }
      
      command = "LOL";
      
      // display counter
      if (REST_TIME - timediff < 4) {
        drawCenterNum(REST_TIME - timediff);
      }

      // flow control
      if (millis() - timer >= REST_TIME * SEC2MILLIS_RATIO) {
        // update screen
        tft.fillScreen(COLOR_BLACK);
        delay(500);
        
        state = STATE_EXERCISE;
        timer = millis(); // restart timer again
      }
      break;
      
    case STATE_CLEANUP:
      // display result
      drawImage(medal);
      delay(2000);

      tft.fillScreen(COLOR_BLACK);
      drawPercent(score);
      delay(2000);
    
      // reset variables
      number_of_set = 0;

      Serial.println("CLEANUP");
      state = STATE_STB;
      break;
      
    case STATE_TEST:

      wifi_status = connect_wifi();
      if (wifi_status == WL_CONNECTED) {
        Serial.println("Wifi Connected");

        // connect to netpie
        microgear.init(KEY, SECRET, ALIAS);
        microgear.connect(APPID);
      }

      while(1);
      
      break;
  }

  // loop microgear 
  if (state == STATE_EXERCISE || state == STATE_REST || state == STATE_CLEANUP) {
    // check microgear status
    microgear_status = microgear.connected();
      
    if (microgear_status != 0) { // still connected to netpie
      microgear.loop();

      // send message to web
      json = createJSON(command, state, number_of_set+1, grip_level, rem_time, score);
      microgear.chat(TARGET_ALIAS, json);

      // too low delay cause netpie to go boom
      delay(500);
      
    }
    else {
      // alert user for dos
      Serial.println("\r\n[ALERT] CONNECTION LOST.\r\n");
    }
  }

}


/* ISR */



/* USER FN */
int32_t get_weight(int32_t scale_reading) {
  return abs(((scale_reading-loadcell_base_1)/scale_factor));
}

int8_t get_grip_level(int32_t max_str, int32_t scale_read) {
  int8_t level = 0;
  float measured = ((float)get_weight(scale_read)/(float)max_str);
          
  if (measured > 0.7) level = 3;
  else if (measured < 0.7 && measured >= 0.6) level = 2;
  else if (measured < 0.6 && measured >= 0.5) level = 1;
  else if (measured < 0.5 && measured >= 0.3) level = 0;
  else if (measured < 0.3 && measured >= 0.2) level = -1;
  else if (measured < 0.2 && measured >= 0.1) level = -2;
  else level = -3;

  return level;
}

int connect_wifi(void) {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int wl_status = WL_DISCONNECTED;

  for (uint8_t attempt = 0; attempt < 10; attempt++) {
    wl_status = WiFi.status();
    delay(500);
    Serial.print(".");
  
    if (wl_status == WL_CONNECTED) {
      Serial.println("");
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());

      break;
    }
  }

  return wl_status;
}

/* tft related fn start */
void drawImage(uint8_t image[]) {
  uint16_t col = 0, row = 0;
  for (row = 0; row < 128; ++row) {
    for (col = 0; col < 128; ++col) {
      uint16_t temp_color = 0x0000;
      uint8_t first_color = image[(2*col) + (256*row)];
      uint8_t second_color = image[(2*col+1) + (256*row)];
      temp_color = (temp_color | first_color) << 8;
      temp_color = temp_color | second_color;

      tft.drawPixel(col, row, temp_color);
    }
  }
}

void drawClock(uint8_t clock_size, uint16_t color) {
    uint8_t outer_ring, inner_ring, \
            inner_circle, \
            stick_start_x, stick_start_y, stick_width, stick_height, \
            btn_start_x, btn_start_y, btn_width, btn_height, btn_radius;
  
  if (clock_size == 0) {
    outer_ring = 16;
    inner_ring = 14;
    
    inner_circle = 12;
    
    stick_start_x = 1;
    stick_start_y = 2 + outer_ring;

    btn_start_x = 5;
    btn_start_y = 5 + stick_start_y;

    btn_radius = 1;
  }
  else if (clock_size == 1) {
    outer_ring = 20;
    inner_ring = 18;
    
    inner_circle = 16;
    
    stick_start_x = 2;
    stick_start_y = 3 + outer_ring;

    btn_start_x = 5;
    btn_start_y = 5 + stick_start_y;

    btn_radius = 1;
  }
  else {
    outer_ring = 24;
    inner_ring = 22;
    
    inner_circle = 20;
    
    stick_start_x = 2;
    stick_start_y = 4 + outer_ring;

    btn_start_x = 6;
    btn_start_y = 6 + stick_start_y;

    btn_radius = 3;
  }

  stick_width = stick_start_x*2 + 1;
  stick_height = stick_start_y - outer_ring;
  btn_width = 2*btn_start_x + 1;
  btn_height = btn_start_y - stick_start_y;
  
  tft.fillCircle(tft.width()/2, tft.height()/2, outer_ring, color);
  tft.fillCircle(tft.width()/2, tft.height()/2, inner_ring, COLOR_BLACK);
  tft.fillCircle(tft.width()/2, tft.height()/2, inner_circle, color);
  tft.fillRect(tft.width()/2 - stick_start_x, tft.height()/2 - stick_start_y, stick_width, stick_height, color);
  tft.fillRoundRect(tft.width()/2 - btn_start_x, tft.height()/2 - btn_start_y, btn_width, btn_height, btn_radius, color);

}

void drawSoundShell(uint8_t flip) {
  if (flip == 0) {
    tft.fillCircle(tft.width()/2, tft.height()/2, 40, COLOR_BLUE);
    tft.fillCircle(tft.width()/2, tft.height()/2, 38, COLOR_BLACK);

    tft.fillCircle(tft.width()/2, tft.height()/2, 36, COLOR_BLUE);
    tft.fillCircle(tft.width()/2, tft.height()/2, 32, COLOR_BLACK);
  }
  else {
    tft.fillCircle(tft.width()/2, tft.height()/2, 40, COLOR_BLUE);
    tft.fillCircle(tft.width()/2, tft.height()/2, 36, COLOR_BLACK);

    tft.fillCircle(tft.width()/2, tft.height()/2, 34, COLOR_BLUE);
    tft.fillCircle(tft.width()/2, tft.height()/2, 32, COLOR_BLACK);
  }

  uint8_t x0, x1, x2, y0, y1, y2;
  
  x0 = 0; x1 = 128; x2 = 64;
  y0 = 0; y1 = 0; y2 = 64;
  tft.fillTriangle(x0, y0, x1, y1, x2, y2, COLOR_BLACK);

  x0 = 0; x1 = 128; x2 = 64;
  y0 = 128; y1 = 128; y2 = 64;
  tft.fillTriangle(x0, y0, x1, y1, x2, y2, COLOR_BLACK);
}

void drawCdtRing(uint8_t fraction, uint16_t color) {
  uint8_t x0, x1, x2, y0, y1, y2;

  if (fraction == 16) {
    // draw full circle
    tft.fillCircle(tft.width()/2, tft.height()/2, 55, color);
    tft.fillCircle(tft.width()/2, tft.height()/2, 52, COLOR_BLACK);
  }
  
  if (fraction == 0) {
    x0 = 32; x1 = 64; x2 = 64;
    y0 = 0; y1 = 0; y2 = 64;
    tft.fillTriangle(x0, y0, x1, y1, x2, y2, COLOR_BLACK);
  }
  
  if (fraction == 1) {
    x0 = 0; x1 = 32; x2 = 64;
    y0 = 0; y1 = 0; y2 = 64;
    tft.fillTriangle(x0, y0, x1, y1, x2, y2, COLOR_BLACK);
  }
  
  if (fraction == 2) {
    x0 = 0; x1 = 64; x2 = 0;
    y0 = 0; y1 = 64; y2 = 32;
    tft.fillTriangle(x0, y0, x1, y1, x2, y2, COLOR_BLACK);
  }

  if (fraction == 3) {
    x0 = 0; x1 = 64; x2 = 0;
    y0 = 32; y1 = 64; y2 = 64;
    tft.fillTriangle(x0, y0, x1, y1, x2, y2, COLOR_BLACK);
  }
  
  if (fraction == 4) {
    x0 = 0; x1 = 64; x2 = 0;
    y0 = 64; y1 = 64; y2 = 96;
    tft.fillTriangle(x0, y0, x1, y1, x2, y2, COLOR_BLACK);
  }

  if (fraction == 5) {
    x0 = 0; x1 = 64; x2 = 0;
    y0 = 96; y1 = 64; y2 = 128;
    tft.fillTriangle(x0, y0, x1, y1, x2, y2, COLOR_BLACK);
  }

  if (fraction == 6) {
    x0 = 0; x1 = 64; x2 = 32;
    y0 = 128; y1 = 64; y2 = 128;
    tft.fillTriangle(x0, y0, x1, y1, x2, y2, COLOR_BLACK);
  }

  if (fraction == 7) {
    x0 = 32; x1 = 64; x2 = 64;
    y0 = 128; y1 = 64; y2 = 128;
    tft.fillTriangle(x0, y0, x1, y1, x2, y2, COLOR_BLACK);
  }

  if (fraction == 8) {
    x0 = 64; x1 = 64; x2 = 96;
    y0 = 128; y1 = 64; y2 = 128;
    tft.fillTriangle(x0, y0, x1, y1, x2, y2, COLOR_BLACK);
  }

  if (fraction == 9) {
    x0 = 96; x1 = 64; x2 = 128;
    y0 = 128; y1 = 64; y2 = 128;
    tft.fillTriangle(x0, y0, x1, y1, x2, y2, COLOR_BLACK);
  }

  if (fraction == 10) {
    x0 = 128; x1 = 64; x2 = 128;
    y0 = 128; y1 = 64; y2 = 96;
    tft.fillTriangle(x0, y0, x1, y1, x2, y2, COLOR_BLACK);
  }

  if (fraction == 11) {
    x0 = 128; x1 = 64; x2 = 128;
    y0 = 96; y1 = 64; y2 = 64;
    tft.fillTriangle(x0, y0, x1, y1, x2, y2, COLOR_BLACK);
  }

  if (fraction == 12) {
    x0 = 128; x1 = 64; x2 = 128;
    y0 = 64; y1 = 64; y2 = 32;
    tft.fillTriangle(x0, y0, x1, y1, x2, y2, COLOR_BLACK);
  }

  if (fraction == 13) {
    x0 = 128; x1 = 64; x2 = 128;
    y0 = 32; y1 = 64; y2 = 0;
    tft.fillTriangle(x0, y0, x1, y1, x2, y2, COLOR_BLACK);
  }

  if (fraction == 14) {
    x0 = 128; x1 = 64; x2 = 96;
    y0 = 0; y1 = 64; y2 = 0;
    tft.fillTriangle(x0, y0, x1, y1, x2, y2, COLOR_BLACK);
  }

  if (fraction == 15) {
    x0 = 96; x1 = 64; x2 = 64;
    y0 = 0; y1 = 64; y2 = 0;
    tft.fillTriangle(x0, y0, x1, y1, x2, y2, COLOR_BLACK);
  }
}

void drawGauge(void) {
  tft.fillRect(tft.width()/2-30, tft.height()/2, 60, 20, COLOR_YELLOW);
  tft.fillRect(tft.width()/2-10, tft.height()/2, 20, 20, COLOR_GREEN);
}

void drawNeedle(int8_t scale) {
  uint8_t x0, y0, x1, y1, x2, y2;

  x0 = tft.width()/2 + (scale * 10) - 5;  y0 = tft.height()/2 + 35;
  x1 = tft.width()/2 + (scale * 10);      y1 = tft.width()/2 + 25;
  x2 = tft.width()/2 + (scale * 10) + 5;  y2 = tft.width()/2 + 35;

  tft.fillRect(28, 89, 72, 11, COLOR_BLACK);
  tft.fillTriangle(x0, y0, x1, y1, x2, y2, COLOR_RED);
}

void drawSetCnt(int8_t num) {
  char set = '0';
  if (num == 0) set = '0';
  else if (num == 1) set = '1';
  else if (num == 2) set = '2';
  else if (num == 3) set = '3';
  else if (num == 4) set = '4';
  else set ='x';
  
  tft.drawChar(tft.width()/2 - 8, tft.height()/2 - 35, set, COLOR_BLUE, COLOR_BLACK, 3);
}

void drawGripStr(int16_t kg) {
  String kg_str = String(kg);
  uint8_t str_len = kg_str.length();

//  tft.drawChar(tft.width()/2 - 8, tft.height()/2 - 35, set, COLOR_BLUE, COLOR_BLACK, 3);
  
  // erase remaining number
  tft.fillRect(40, 29, 55, 22, COLOR_BLACK);

  tft.setTextSize(3);
  tft.setTextColor(COLOR_BLUE, COLOR_BLACK);
  
  if (str_len > 1) {
    // set cursor more to the left
    tft.setCursor(45, 29);
  }
  else {
    tft.setCursor(55, 29);
  }

  tft.println(kg_str);
}

void drawCenterNum(uint8_t num) {
  char n = '0';
  if (num == 0) n = '0';
  else if (num == 1) n = '1';
  else if (num == 2) n = '2';
  else if (num == 3) n = '3';
  else n ='x';

  tft.setCursor(49, 43);
  tft.setTextColor(COLOR_BLUE, COLOR_BLACK);
  tft.setTextSize(6);
  tft.println(n);

}

void drawPercent(uint8_t percent) {
  String percent_str = String(percent);
  uint8_t str_len = percent_str.length();

  // draw bar graph
  uint8_t graph_width = 5;
  uint8_t graph_height = 2;
  
  // draw yellow bar
  tft.fillRect(tft.width()/2 - graph_width, tft.height()/2 + 20, 2*graph_width, 15*graph_height, COLOR_YELLOW);
  // draw red bar
  tft.fillRect(tft.width()/2 - 3*graph_width, tft.height()/2 + 20 + 7*graph_height, 2*graph_width, 8*graph_height, COLOR_RED);
  // draw blue bar
  tft.fillRect(tft.width()/2 + graph_width, tft.height()/2 + 20 + 5*graph_height, 2*graph_width, 10*graph_height, COLOR_BLUE);


  // draw percent
  tft.setTextColor(COLOR_BLUE, COLOR_BLACK);
  tft.setTextSize(4);
  
  // draw number
  if (str_len == 1) {
    tft.setCursor(40, 35);
  }
  else if (str_len == 2) {
    tft.setCursor(28, 35);
  }
  else {
    tft.setCursor(17, 35);
  }
  
  tft.print(percent_str);
  tft.print("%");
}

void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

/* tft related fn end */


/* microgear related fn start */
String createJSON(String cmd, state_t state, uint8_t num_set, int8_t griplevel, uint16_t rem_time, uint8_t statistics) {
  String json = "{\"cmd\":\"" + cmd + "\","; // create command
  json += "\"state\":\"" + String(state) + "\",";
  json += "\"set\":\"" + String(num_set) + "\",";
  json += "\"grip_level\":\"" + String(griplevel) + "\",";
  json += "\"remaining_time\":\"" + String(rem_time) + "\",";
  json += "\"stat\":\"" + String(statistics) + "\"}";

  return json;
}

/* If a new message arrives, do this */
void onMsghandler(char *topic, uint8_t* msg, unsigned int msglen) {
  Serial.print("Incoming message --> ");
  msg[msglen] = '\0';
  Serial.println((char *)msg);
}

void onFoundgear(char *attribute, uint8_t* msg, unsigned int msglen) {
  Serial.print("Found new member --> ");
  for (int i = 0; i < msglen; i++)
    Serial.print((char)msg[i]);
  Serial.println();
}

void onLostgear(char *attribute, uint8_t* msg, unsigned int msglen) {
  Serial.print("Lost member --> ");
  for (int i = 0; i < msglen; i++)
    Serial.print((char)msg[i]);
  Serial.println();
}

/* When a microgear is connected, do this */
void onConnected(char *attribute, uint8_t* msg, unsigned int msglen) {
  Serial.println("Connected to NETPIE...");
  /* Set the alias of this microgear ALIAS */
  microgear.setAlias(ALIAS);

  json = createJSON("CONNECT", state, 0, 0, 0, 0);
  microgear.chat(TARGET_ALIAS, json);
}
/* microgear related fn end */
