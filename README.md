# simile
An 8x8 matrix animation display and control software.

### Hardware
- ESP32 DOIT DEVKIT V1
- 8x8 Dot Matrix Module
- External 3.0V Supply

### Running
To run the control software:
```bash
$ pip install pyserial streamlit pillow
$ python3 -m streamlit.py
```

### Connections
Upload the code to the ESP32 and make the following connections:
- VCC and GND of Matrix to Supply.
- SCK to Pin 18
- MOSI/DIN to Pin 23
- CS to Pin 5

### Preview
