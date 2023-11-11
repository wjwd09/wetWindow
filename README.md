# wetWindow
Microcontroller setup to notify user via email or text that they left their window open in the rain.

Utilizes Zybo Z7 10 micocontroller, built with Vivado 2023.2 and Vitis Classic IDE 2023.2.
https://digilent.com/reference/programmable-logic/zybo-z7/start

Water sensor is simple resistance value, water shorting contacts sends a HIGH signal.
https://www.adafruit.com/product/4965

Data is sent over via PMOD ESP32 to third party API.
https://digilent.com/reference/pmod/pmodesp32/reference-manual

SMTP2GO is third party email API that will send email on behalf of user via HTTP POST requests
https://app.smtp2go.com/


SETUP:
PMOD ESP32 is installed on PMOD port JC
Water sensor is connected to PMOD port JE, power is provided by pins 5 and 6, and signal data is being read by port 1 configured for GPIO
