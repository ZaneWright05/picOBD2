# picOBD2
A project for COMP2215 making use of the provided hardware kit and some additional hardware.
The project in action can be seen [here](https://youtube.com/shorts/yGXlc7FlCXg?feature=share) (this is the code in /picoOBD2)
## General Overview
This project allows you to use Pi Pico(s) to communicate with car's ecu to retrieve real time data like RPM or throttle position. 
There are three versions of this project in this repo:
 - one to have the pico communicate with a car and output OBD2 responses to a serial monitor  ([CAN_TEST](https://github.com/ZaneWright05/PICOtoCAN/tree/main/CAN_TEST)),
 - one to have two picos working together, one querying the can frames and another to run the UI and display the infomation, communicating over UART  ([MCP2515Code](https://github.com/ZaneWright05/PICOtoCAN/tree/main/MCP2515Code)) and ([UICode](https://github.com/ZaneWright05/PICOtoCAN/tree/main/UICode)),
 - and a third which combines both the UI and querying, with both modules sharing the pico SPI bus ([picOBD2](https://github.com/ZaneWright05/picOBD2/tree/main/picoOBD2)). This is the version that will be kept up to date and worked on further. 
## Hardware used
- Pi pico 2 W and a pico WH (if using two picos)
- OBD2 Pigtail cable ([amazon link](https://www.amazon.co.uk/dp/B07WSMBSL3?ref=ppx_yo2ov_dt_b_fed_asin_title))<br/>
  <img src="https://github.com/user-attachments/assets/10a9105b-ee05-4dee-a7eb-6e798b36af1a" width="250">
- Waveshare Pico-CAN-B ([wiki](https://www.waveshare.com/wiki/Pico-CAN-B)) <br/>
  <img src="https://github.com/user-attachments/assets/89a147fd-ed5c-4329-9678-9714a44d76dc" width="250">
- Waveshare 1.3" OLED ([wiki](https://www.waveshare.com/wiki/Pico-OLED-1.3#Overview)) <br/>
  <img src="https://github.com/user-attachments/assets/3cf13fed-5005-493d-bff7-f5395fa8a677" width="250">
- Male to Female jumper cables to connect the Picos
## Vehicle Requirements
For this project to work the car being read from must expose the CAN bus through the OBD2 port (most EU cars post 2008 should). If the car you are using does not you will have to expose the CAN bus in another way, only recommended for people with lots of knowledge on this.
## Libraries used
Both are found on the Wiki page attatched to the modules:
- Waveshare UI code for the OLED screen
- Waveshare MCP215 code for the CAN Hat (modified send function)
## Set up
This is the set up for running the code on my 2006 Toyota RAV4 which exposes CAN on it's obd2 port:
1. First connect the CAN B hat and the Pico WH, ensuring they are the correct way (most will have a "usb" marker to indicate which way round it should be plugged in).
2. Find the CAN High, CAN Low and ground cables exposed by the OBD2 Pigout. Some cars will have both signal and chassis ground, while some will only have chassis, if signal is exposed its reccomended to use it. The pinout for OBD2 is shown below, pins 5 (4 if using chassis ground), 6 and 14 are the ones we need to connect to the can hat. Its likely the pigout cable will explain what colour each cable is but if not a multi-meter can be used to check continuity between the pins and cables.<br/><img src="https://github.com/user-attachments/assets/6a32a430-6196-497d-aaac-834ef6bb6cd2" width="500"> <br/> Taken from: https://www.flexihub.com/oobd2-pinout/.<br/>
For my setup the connections were:
- pin 5 <-yellow-> gnd
- pin 6 <-green-> CAN high
- pin 14 <-brown/white-> CAN low <br/>
<img src="https://github.com/user-attachments/assets/618e7e91-579f-42d0-9a65-6d32a8f4d1c0" height="250"> <br/>
  **Note:** if you are running ([CAN_TEST](https://github.com/ZaneWright05/PICOtoCAN/tree/main/CAN_TEST)) or ([picOBD2](https://github.com/ZaneWright05/picOBD2/tree/main/picoOBD2)), you can now run the code and disregard the rest of the setup, assuming all connections are correct.
3. At this point you should flash the MCP2515Code file onto this pico but the CAN_TEST file will also work at this point. When testing the setup both files should outut logs through the serial port.
4. Now move to the second Pico and attatch the OLED screen. Now jumper cables need to be attatched for the UART communication. For this you will need 3 jumper cables (RX, TX, GND), thier connections will vary depending on your pico headers but in my case I used male to female jumpers. Both files are set up to use UART0 so need you need to connect pins 0 and 1.<br/>
The connections between the picos are:
- pin 3 (gnd) <-> pin 3 (gnd)
- pin 1 (TX) <-> pin 2 (RX)
- pin 2 (RX) <-> pin 1 (TX)
<br/><img src="https://github.com/user-attachments/assets/58b70836-65f6-4f3e-943f-2700f4a8287b" height="250">
5. This step is optional as each pi can be powered separately on through their micro-usb port, but if you only want to use one cable as I did connecting the picos on their VSYS pins will allow power to be shared among them:<br/>
- pin 39 (vsys) <-> pin 39 (vsys)
- pin 38 (gnd) <-> pin 38 (gnd)
<br/><img src="https://github.com/user-attachments/assets/afac0069-5c56-43bc-8457-d510bc145a5f" height="250">
6. Flash the UICode file onto the second pico, the project should now be ready to use when the picos are powered. If you'd like to test if they are working outside of the can functionality comment out the MCP2512_Send(...) and MCP2515_Receive(...) lines in `fetch_PID_response()` and uncomment the line which sets the reply with a fixed value.
<br/><img src="https://github.com/user-attachments/assets/e44faa25-5520-423d-a2ff-7b6048ea0351" height="250">
## Usage
- key0 to shift between the data being queried
- key1 to select and begin querying (press again to deselect)
## Known issues
- in the lastest version of picOBD2 vehicle speed and oil temp are not working (with my setup), more time will need to be spent fine tuning the software and ensuring that the ECU responding supports these PIDS, there are special PIDs that can be sent to retrieve this information, found in the link below.
## Future extensions
- web app output and input, instead of relying on a screen use the pico W's ability to host a small webserver to allow the communication.
- dials use of servos/stepper motors to display the information
- extension to dynamically allow given PIDs, on start up request supported PIDs then have these be the set that can be queryied
- display more than one data type at the same time (simultaneous querys are impossible on the CAN bus but alternating the frames sent may allow a relatively fast response)
## Sidenotes
- This code makes use of the waveshare code for UI and sending MCP2515 messages, the reason for this is because I am using the modules this code was designed for, other modules/or libraries could be used with relative ease.
- This code only allows the user to query a small range of PIDs on the vehicle can, adding more would not be a challenge and the PIDs to query and format of the response can be found [here](https://www.csselectronics.com/pages/obd2-pid-table-on-board-diagnostics-j1979).
- I powered the pico through a 12v powered USB port (standard ciggarette lighter usb) inside the car, but use of a step down converter (12v to 5v) could be used to take power directly from the car (OBD2 pin 16) reducing the need for an additional cable outside of the ones already being used.
