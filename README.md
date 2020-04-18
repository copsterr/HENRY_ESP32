# SQUEEZER


Hello! This device is called "Squeezer". It's purpose is to encourage elders to exercise more.
Wondering that squeezing is not an exercise? I think not!
This device deliver a soothing experience of squeezing. It's even connected to the internet.
You can watch video while exercise. (How cool is that?! :D)
---

## Here's something you should know about this device.
1. It is a prototype. You shouldn't expect robust stability from this device. There are always unseen flaws in product development.
2. The server is belong to the programmer (me). So a year later, this device might not work properly. (unless you pay me the server cost and maintenance service)
3. The web is hackathoned to demonstrate the device's functionality. You shouldn't expect anything fancy like animation, explosion, or a unicorn from it.
4. It is recommended to use a WiFi router rather than using a mobile Hotspot unless the connection stability is not guaranteed. (Damn NETPIE!)
5. I don't do maintenance service. Extra maintenance rather than consulting will be charged. (and it's very expensive because I don't have money :/)
---

## For nerds
1. Hardware Dependencies
  - ESP32 DevkitC V4 Wrover-B module
  - 1.44 TFT Adafruit Module (Actually it's a copy from China)
  - Loadcell 20kg
  - Loadcell Driver HX711
  - LiPo 330mAh + 5V Regulator + LiPo Charger

2. Software Dependencies
  - Programming platform: Arduino IDE (1.8.12)
  - Libraries: Adafruit_GFX.h, Adafruit_ST7735.h, SPI.h, math.h, WiFi.h, MicroGear.h

3. Web Service Dependencies
  - Jquery
  - Bootstrap 4
  - NETPIE Microgear (for more information -> www.netpie.io)