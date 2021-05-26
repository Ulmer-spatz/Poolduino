#include <Wire.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>

// Display
#include "Adafruit_SSD1306.h"
#include <Adafruit_GFX.h>

// LED
#include <Adafruit_NeoPixel.h>

// Atlas Komponenten
#include "Ezo_i2c.h"

Ezo_board ph = Ezo_board(99, "PH");
Ezo_board temperature = Ezo_board(102, "TEMP");
Ezo_board orp = Ezo_board(98, "ORP");
Ezo_board PH_DOWN = Ezo_board(103, "PH_DOWN");
Ezo_board ORP_UP = Ezo_board(104, "ORP_UP");

// Ports Eingang
#define Eingang_Flow 4
#define Eingang_Red 6
#define Eingang_Blue 14
#define Eingang_Silver 5

#define Led 7
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(3, Led, NEO_GRB + NEO_KHZ800);


/************** Hier rumfroschen **************/
String Version = "Poolduino 3.0 Uno";
unsigned int localPort = 900;      // local port to listen on

// IP Adresse und Port vom Loxone Server
const char * udpAddress = "192.168.178.39";
const int udpPort = 900;

// ml PH- pro Impuls
//uint8_t PHminus = "25";

// Abfrage Intervall von TMP, PH und ORP in Sekunden
int t = 300;

byte mac[] = {0xDE, 0x11, 0x22, 0x33, 0x44, 0x56};
/***********/

// Display
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);


byte i = 0; //Zähler für Schleife
int silver = 0;
String flow;


// buffers for receiving and sending data

//String UDP_data;
char packetBuffer_empfang[UDP_TX_PACKET_MAX_SIZE];

String UDP_sendtext;

//create UDP instance
EthernetUDP Udp;



void setup() {
  Wire.begin();                         //start the I2C
  
  //**************** Display & LED ****************
  pixels.begin();
  pixels.setPixelColor(2, pixels.Color(0, 70, 0)); //Grün
  pixels.show();
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x64
    
    for (;;);
  }
  pixels.setPixelColor(1, pixels.Color(0, 70, 0));
  pixels.show();
  
  display.clearDisplay();
  display.setTextSize(1);             
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(Version);
  display.display();

  /****************  LAN *************************/
  Ethernet.init(10);  // Most Arduino shields
  // start the Ethernet

  if (!Ethernet.begin(mac)) ;
  else {
    
  }

  pixels.setPixelColor(0, pixels.Color(0, 70, 0)); // Moderately bright green color.
  pixels.show();


  Udp.begin(localPort);
  pinMode(Eingang_Flow, INPUT_PULLUP);
  pinMode(Eingang_Red, INPUT_PULLUP);
  pinMode(Eingang_Blue, INPUT_PULLUP);
  pinMode(Eingang_Silver, INPUT_PULLUP);
}

void loop() {

  pixels.setPixelColor(0, pixels.Color(0, 0, 0)); // Moderately bright green color.
  pixels.setPixelColor(1, pixels.Color(0, 0, 0));
  pixels.show();


  /***************   FLOW Schalter/Buttons Anfang ****************/
  if (digitalRead(Eingang_Flow) == LOW) {
    // AN
    sendUDP("Flow:1");
    flow = "An";
  } else {
    //Aus
    sendUDP("Flow:0");
    flow = "Aus";
  }

  if (digitalRead(Eingang_Blue) == LOW) {
      sendUDP("Blue:1");

  }
  if (digitalRead(Eingang_Red) == LOW) {
      sendUDP("Red:1");

  }
  if (digitalRead(Eingang_Silver) == LOW) {
    
    if (silver == 0) {
      silver = 1;
    }
    else
    {
      silver = 0;
    }
  }


  /*************** FLOW Schalter/Buttons Ende ********************/


  checkUDP();
  i++;

  if ( i > t) {
    // check if there is a valid temperature reading
    if ((temperature.get_error() == Ezo_board::SUCCESS) && (temperature.get_last_received_reading() > -1000.0)) {

      ph.send_read_with_temp_comp( temperature.get_last_received_reading() );
    } else {
      ph.send_read_cmd();
    }
    temperature.send_read_cmd();
    delay(1000);                                          // read command needs a second to process

    orp.send_read_cmd();
    delay(1000);

    receive_reading(ph);                                  //get the reading from the PH circuit
   

    receive_reading(temperature);                         //get the reading from the RTD circuit
    

    receive_reading(orp);                         //get the reading from the RTD circuit
   
    i = 0;
  }

  /*********  Display ***************/
  if (silver == 0) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.print(F("PH: "));
    display.println(ph.get_last_received_reading());
    display.print(F("ORP: "));
    display.println(orp.get_last_received_reading());
    display.print(F("Temp: "));
    display.println(temperature.get_last_received_reading());
    display.print(F("Flow: "));
    display.print(flow);
    display.display();

  }
  if (silver == 1) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(Version);

    display.print("IP: ");
    display.println(Ethernet.localIP());

    display.print("LAN: ");
    display.println(Ethernet.linkStatus());
    display.display();
  }

  delay(1000);
}





// function to decode the reading after the read command was issued
void receive_reading(Ezo_board &Sensor) {

  Sensor.receive_read_cmd();                            // get the response data

  switch (Sensor.get_error()) {                         // switch case based on what the response code is.
    case Ezo_board::SUCCESS:
     

      // ******************  Daten an Loxone senden *****************
      UDP_sendtext = Sensor.get_name();
      UDP_sendtext = UDP_sendtext + ":";
      UDP_sendtext = UDP_sendtext + Sensor.get_last_received_reading();
      sendUDP(UDP_sendtext);
      // ************************************************************

      break;

    case Ezo_board::FAIL:
                               //means the command has failed.
      break;

    case Ezo_board::NOT_READY:
     
      break;

    case Ezo_board::NO_DATA:
        
      break;
  }
}

void checkUDP()
{
  int packetSize = Udp.parsePacket();
  if (packetSize) {

    Udp.read(packetBuffer_empfang, 20);
    
    delay(100);


    /*************  Wert zerlegen *****************/
    char* pos;
    String pumpe;
    byte ml;
    pos = strstr(packetBuffer_empfang, ":");
    if (pos)
    {
      ml = atof(pos + 1);

      
    }

    pumpe = packetBuffer_empfang;

    /**************  PH - */
    if (pumpe.startsWith("PH-:"))

    {
      PH_DOWN.send_cmd_with_num("D,", ml);
      delay (1000);
    }
    /**************  Chloren */
    if (pumpe.startsWith("CL+:"))
    {
      
      ORP_UP.send_cmd_with_num("D,", ml);
      delay (1000);
    }

    /* Daten leeren */
    for (unsigned int a = 0; a < 10; a++)
    {
      packetBuffer_empfang[a] = 0;
    }

  }
}

void sendUDP(String text)
{
  Udp.beginPacket(udpAddress, udpPort);
  Udp.print(text);
  Udp.endPacket();
}
