# 50Hz Monitor
Monitoring the frequency of your main power.

The device is powered with 9V AC. The AC is converted with a full bridge rectifier producing positiv sinus cycles at 100 Hz rate.
A comparator generates interrupts on the ESP8266 MCU. A diode decouples the sinus from the following DC stage where 5V is generated.
 This drives the ESP and the LCD display. Timer1 is used to count a few sinus cycles and the MCU calculates the frequency.

The measured frequency is send periodically to a website for data storing and display the graph with RRD.



 
