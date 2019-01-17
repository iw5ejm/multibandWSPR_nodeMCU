// Si5351_WSPR
//
// Simple WSPR beacon for ESP8266, with the Etherkit or SV1AFN Si5351A Breakout
// Board, by Jason Milldrum NT7S.
// 
// Original code based on Feld Hell beacon for Arduino by Mark 
// Vandewettering K6HX, adapted for the Si5351A by Robert 
// Liesenfeld AK6L <ak6l@ak6l.org>.  Timer setup
// code by Thomas Knutsen LA3PNA.
//
// Hardware info
// ---------------------
// serial debug port baud rate: 74880
// Si5351A is connected via I2C on pin D1 (SCL) and D2 (SDA) as marked on Wemos D1 mini Lite
// freq0 is used in clock0 output, freq1 in clock1, freq2 in clock2
// on SV1AFN board clock0 is marked J1, clock1 J2, clock2 J3.
//
// Hardware Requirements
// ---------------------
// This firmware must be run on an ESP8266 compatible board
// testde on Wemos D1 Mini Lite
//
// Required Libraries
// ------------------
// Etherkit Si5351 (Library Manager)
// Etherkit JTEncode (Library Manager)
// Time (Library Manager)
// Wire (Arduino Standard Library)
// NTPtimeESP
// ESP8266WiFi
//
// License
// -------
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject
// to the following conditions:
// 
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
// ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
// CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//

#include <si5351.h>
#include "Wire.h"
#include <JTEncode.h>
#include <int.h>
#include <TimeLib.h>
#include <NTPtimeESP.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>


#define TONE_SPACING            146           // ~1.46 Hz
#define WSPR_DELAY              683          // Delay value for WSPR
#define WSPR_CTC                10672         // CTC value for WSPR
#define SYMBOL_COUNT            WSPR_SYMBOL_COUNT
#define CORRECTION              -12242             // Change this for your ref osc -12000 

#define TX_LED_PIN              2  //integrated onboard led, marked as D4 on Wemos D1 mini lite
#define SYNC_LED_PIN            16 //marked as D4 on Wemos D0 mini lite

//****************************************************
//* SYNCRONIZE SYSTEM TIME with NTP SERVERS
//* need to be modified, obsolete library in use...
//****************************************************
#define SEND_INTV     10
#define RECV_TIMEOUT  10

//If you want use more than one jack output uncomment relative define:

//#define clock1 //if uncommented this define enables jack 2 (clock1 of si5351) outputREMEMBER TO CHANGE FREQUENCY BELOW

//#define clock2 //if uncommented this define enables jack 3 (clock1 of si5351) output


// Global variables
Si5351 si5351;
JTEncode jtencode;

#define MAXCH      5 //Change this according to the number of bands inserted below
unsigned long freq0[] = {14097158UL, 10140258UL, 7040158UL, 5366258UL, 3594158UL}; //CHANGE THIS: is the freq of multiband output on jack 1 (clock0)

//unsigned long freq0 = 14097158UL;                  // RFU
#ifdef clock1
unsigned long freq1 =  7040158UL;                // Change this: if used is the freq of single band output on jack 2 (clock1)
#endif
#ifdef clock2
unsigned long freq2 = 28126158UL;                // Change this: if used is the freq of single band output on jack 3 (clock2)
#endif
int ch=0; // automatic frequency switch index
bool warmup=0;

char call[7] = "IW5EJM";                        // Change this
char loc[5] = "JN53";                           // Change this
uint8_t dbm = 10;
uint8_t tx_buffer[SYMBOL_COUNT];

const char* ssid = "SSIDofWIFI";       //SSID of your Wifi network: Change this
const char* password = "PASSWORDofWIFI";      //Wi-Fi Password:            Change this


//**** How the station is named in your NET
const char* WiFi_hostname = "WSPRmultiTX";

//**** Sync the soft clock every 12 hours
#define NTPSYNC_DELAY  12

//**** NTP Server to use
const char* NTP_Server = "ntp1.inrim.it"; //italian national institute for measures

//**** Your time zone UTC related (floating point number)
#define TIME_ZONE 1.0f

NTPtime NTPch(NTP_Server);   
strDateTime dateTime;

// --------------------------------------
// epochUnixNTP set the UNIX time
// number of seconds sice Jan 1 1970
// --------------------------------------

time_t epochUnixNTP()
{
    Serial.println(">>>>>>>> Time Sync function called <<<<<<<<<");

//**** BIG ISSUE: in case of poor connection, we risk to remain in this loop forever
    NTPch.setSendInterval(SEND_INTV);
    NTPch.setRecvTimeout(RECV_TIMEOUT);
    do
    {
      dateTime = NTPch.getNTPtime(TIME_ZONE, 1);
      delay(1);
    }
    while(!dateTime.valid);
    NTPch.printDateTime(dateTime);
    setTime(dateTime.hour,dateTime.minute,dateTime.second,dateTime.day,dateTime.month,dateTime.year); 
    Serial.println(now());

  return 0;
}


 
// Loop through the string, transmitting one character at a time.
void encode()
{
    uint8_t i;

    jtencode.wspr_encode(call, loc, dbm, tx_buffer);
    
    // Reset the tone to 0 and turn on the output //unused portion of code due to warmup
    // si5351.set_clock_pwr(SI5351_CLK0, 1);
    // si5351.set_clock_pwr(SI5351_CLK1, 1);
    //si5351.set_clock_pwr(SI5351_CLK2, 1);

    digitalWrite(TX_LED_PIN, LOW);
    Serial.println("TX ON");
    
    // Now do the rest of the message
    for(i = 0; i < SYMBOL_COUNT; i++)
    {
        si5351.set_freq((freq0[ch] * 100) + (tx_buffer[i] * TONE_SPACING), SI5351_CLK0);
        
        #ifdef clock1
        si5351.set_freq((freq1 * 100) + (tx_buffer[i] * TONE_SPACING), SI5351_CLK1);
        #endif
        
        #ifdef clock2
        si5351.set_freq((freq2 * 100) + (tx_buffer[i] * TONE_SPACING), SI5351_CLK2);
        #endif

      delay(WSPR_DELAY);

    }
        
    // Turn off the output
    si5351.set_clock_pwr(SI5351_CLK0, 0);
    #ifdef clock1
    si5351.set_clock_pwr(SI5351_CLK1, 0);
    #endif
    #ifdef clock2
    si5351.set_clock_pwr(SI5351_CLK2, 0);
    #endif
    
    digitalWrite(TX_LED_PIN, HIGH);
    Serial.println("TX OFF");
    ch++;
    if (ch==MAXCH) ch=0;
}

void ssidConnect()
{
  Serial.println(ssid);
  Serial.println(password);
  WiFi.begin(ssid, password);
 
  while (WiFi.status() != WL_CONNECTED) {
    delay(700);
    Serial.print(".");
  }
     
  Serial.println();
  Serial.print(F("Connected to "));
  Serial.println(ssid);
  Serial.print(F("IP address: "));
  Serial.println(WiFi.localIP());
}
 
void setup()
{
  Serial.begin(74880); while (!Serial);
  Serial.println("COM setup successful");
  delay(10);

  WiFi.mode(WIFI_STA);
  // connect to WiFi network
  ssidConnect();
  
  
  // Use the LED as a keying indicator.
  pinMode(TX_LED_PIN, OUTPUT);
  pinMode(SYNC_LED_PIN, OUTPUT);
  digitalWrite(TX_LED_PIN, HIGH);
  digitalWrite(SYNC_LED_PIN, HIGH);

  // Set time sync provider
  setSyncProvider(epochUnixNTP);  //set function to call when sync required 
    
  // Initialize the Si5351
  // Change the 2nd parameter in init if using a ref osc other
  // than 25 MHz
  Serial.println("start radio module setup");
  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 27000000UL, CORRECTION);
  Serial.println("Module intializated");

  
  // Set CLK0 output
  si5351.set_freq(freq0[ch] * 100, SI5351_CLK0);
  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA); // Set for max power
  si5351.set_clock_pwr(SI5351_CLK0, 0); // Disable the clock initially

#ifdef clock1
 // Set CLK1 output
  si5351.set_freq(freq1 * 100, SI5351_CLK1);
  si5351.drive_strength(SI5351_CLK1, SI5351_DRIVE_8MA); // Set for max power
  si5351.set_clock_pwr(SI5351_CLK1, 0); // Disable the clock initially
#endif

#ifdef clock2
 // Set CLK2 output
  si5351.set_freq(freq2 * 100, SI5351_CLK2);
  si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_8MA); // Set for max power
  si5351.set_clock_pwr(SI5351_CLK2, 0); // Disable the clock initially
#endif

  Serial.println("Radio Module setup successful");
  Serial.println("Entering loop...");
}
 
void loop()
{  

  // Trigger every 5 minute
  // WSPR should start on the 1st second of the minute, but there's a slight delay
  // in this code because it is limited to 1 second resolution.
  
  // 30 seconds before enable si5351a output to eliminate startup drift
  if((minute() + 1) % 5 == 0 && second() == 30 && !warmup)
    { warmup=1;
    
      si5351.set_freq(freq0[ch] * 100, SI5351_CLK0);
      si5351.set_clock_pwr(SI5351_CLK0, 1);
      
      #ifdef clock1
      si5351.set_clock_pwr(SI5351_CLK1, 1);
      #endif
      #ifdef clock2
      si5351.set_clock_pwr(SI5351_CLK2, 1);
      #endif

    }

  if(minute() % 5 == 0 && second() == 0)
    {
      Serial.println(now());
      encode();
      warmup=0;
      delay(1000);
     }
  }
