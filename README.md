# PostureCoach

## Overview  
PostureCoach is a wearable Arduino‑based posture tracking device built for the 2026 IEEE SSCS Arduino Contest. It uses an accelerometer to detect poor posture and provides real‑time feedback through an OLED display.

## Features  
- Real‑time posture detection  
- OLED display feedback  
- Calibration mode  
- Modular code structure (sensors, display, logic)

## Hardware  
- Arduino Nano or Uno  
- MPU6050 accelerometer  
- SSD1306 OLED display  
- Battery pack and wiring

## Project Structure  
```
PostureCoach.ino        # Main sketch
sensors.cpp/h           # Accelerometer handling
display.cpp/h           # OLED rendering
logic.cpp/h             # Posture evaluation
pins.h                  # Pin definitions
ui_strings.h            # Display text constants
```

## Demo Video  
IEEE SSCS Arduino Contest Submission  
YouTube: `[https://www.youtube.com/watch?v=au9m7T2P-hg&pp=0gcJCRoMAYcqIYzv]`

## How to Run  
1. Install Arduino IDE  
2. Install required libraries: Wire, Adafruit_GFX, Adafruit_SSD1306  
3. Connect MPU6050 and OLED via I2C  
4. Upload `PostureCoach.ino`  
5. Power the device and follow on‑screen calibration

## Future Improvements  
- Bluetooth connectivity  
- Improved enclosure  
- Additional posture categories
