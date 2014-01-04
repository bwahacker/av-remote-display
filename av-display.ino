/**
 * Mitch's AV display thing.
 *
 * BSD License.
 *
 * You'll need to modify it to match your environment and the serial stuff
 * will need serious modification if you don't have an Onkyo receiver!  :-)
 *
 * This uses the "Serial Shield", which shares the USB UART--this is
 * extremely unfortunate, as it makes programming the firmware, debugging,
 * etc., a ton harder than it should be.
 *
 * I am working on a new version that uses I2C and MAX3233 chips.
 * 
 * I don't recommend using the Serial Shield :-(.
 *
 */

#include <LiquidCrystal.h>

// Standard Hitachi mumble LCD driver configuration--
// Connections:
// rs (LCD pin 4) to Arduino pin 12
// rw (LCD pin 5) to Arduino pin 11
// enable (LCD pin 6) to Arduino pin 10
// LCD pin 15 to Arduino pin 13
// LCD pins d4, d5, d6, d7 to Arduino pins 5, 4, 3, 2

String g_volume = "--";
String g_listenMode = "ListenMode";
String g_inputSource = "InputSource";
String g_muting = "";
String g_power = "";

LiquidCrystal lcd(12, 11, 10, 5, 4, 3, 2);

int loopCount = 0;
int MAX_LCD_LINES = 2;
int MAX_LCD_COLS = 20;

int nextLine = -1;
String displayList[4] = { "", "", "", "" };

void clearAllStates()
{
    g_volume = "";
    g_listenMode = "";
    g_inputSource = "";
    g_muting = "";
}

// Debug -- I usually plug in a 4 line display for debugging
void myPrint(String s)
{ 
    int i;

    // insert at the end and scroll it up
    displayList[0] = displayList[1];
    displayList[1] = displayList[2];
    displayList[2] = displayList[3];

    // XXX: handle an overflow beyond MAX_LCD_COLS bytes?
    for (i = s.length(); i < MAX_LCD_COLS; i++) {
        s = s + " ";
    }
    
    displayList[3] = s;
    
    for (i = 0; i < 4; i++) {
        lcd.setCursor(0, i);
        lcd.print(displayList[i]);
    }
}

void setup()
{
    lcd.begin(MAX_LCD_COLS, MAX_LCD_LINES);    
    lcd.clear();          
    
    myPrint("Starting up...");
    
    Serial.begin(9600);
    
    myPrint("Serial initialized...");
    myPrint("");  
}

int hexCharToDecInt(char c)
{
    int v = 0;
    if (c >= 48 && c <= 57) {
        v = c - 48;
    } else if (c >= 65 && c <= 70) {
        v = c - 65 + 10; 
    } else if (c >= 97 && c <= 102) {
        v = c - 97 + 10; 
    } else {
       myPrint("E100: c = " + String(c)); 
    }
    
    return v;
}

void waitForMessage()
{
    String readString = "";

    // from some Arduino example, I forget where.
    while (Serial.available()) {
        delay(3); 
        if (Serial.available() > 0) {
            char c = Serial.read();
            
            // 0x1A is the end-of-message for the Onkyo
            if (c == '\n' || c == '\r' || c == 0x1A) {  
                processMessage(readString);
                readString = "";
            } else {
                readString += c;
            }
        } 
    }
}

void
sendCommands(boolean everything)
{
    char * cmds[5] = { "!1MVLQSTN\r\n", // volume
                       "!1LMDQSTN\r\n", // listen mode
                       "!1SLIQSTN\r\n", // input selected
                       "!1AMTQSTN\r\n", // muting
                       "!1PWRQSTN\r\n", // power
     };
     
     int i;
     int loopMax = 5;
     if (! everything) {
         loopMax = 1;
     }
     
     for (i = 0; i < loopMax; i++) {
         Serial.write(cmds[i]);
         if (everything) {
            delay(10);
            waitForMessage();
         }
     }
}

void processVolume(String s)
{
    if (s.length() != 2) {
        myPrint("volume length error");
        delay(2000);
    }
    char hexLeft = s.charAt(0);
    char hexRight = s.charAt(1);
    
    int intTens = hexCharToDecInt(hexLeft) * 16;
    int intOnes = hexCharToDecInt(hexRight);
    
    int volume = intTens + intOnes;
    g_volume = String(volume);
}

struct ListenMode {
    String key;
    String expansion;  
};

void
processListenMode(String s)
{
    ListenMode MODES[] = {
        { "00", "Stereo" },
        { "01", "Direct" },
        { "02", "Surround" },
        { "03", "Film" },
        { "04", "THX" },
        
        { "0C", "All-Ch Stereo" },
        { "11", "Pure Audio" },
 
        { "40", "Straight Decode" },
        { "42", "THX Cinema" },
        { "43", "THX Surround Ex" },
        { "44", "THX Music" },
        { "45", "THX Game" },
 
        { "80", "PLII Movie" },
        { "81", "PLII Music" },
        { "82", "Neo:6 Cinema" },
        { "83", "Neo:6 Music" },
        { "85", "Neo:6 THX Cinema" },
        { "86", "PLII Game" }
      };

    int found = 0;    
    int i; 
    for (i = 0; i < 17; i++) {
       if (s == MODES[i].key) {
          s = MODES[i].expansion;
          found = 1;
          break;
       }
    }
    
    if (!found) {
       s = "LMode:" + s; 
    }
    
    g_listenMode = s;    
}

void
processInputSource(String s)
{
    if (s == "00") {
        s = "HDMIswitch";
    } else if (s == "10") {
        s = "AppleTV";
    } else if (s == "02") {
        s = "Wii";
    } else if (s == "23") {
        s = "CD";
    } else if (s == "01") {
        s = "CBL/SAT";
    } else if (s == "03") {
        s = "PS4";
    } else {
        s = "Input:" + s;
    }
    
    g_inputSource = s;
}

void
processMuting(String s)
{
    if (s == "00") {
        s = "";
    } else if (s == "01") {
        s = "MUTING";
    } else {
        s = "MUTE:" + s;
    }
    
    g_muting = s;
}

void
processPower(String s)
{
    if (s == "00") {
        s = "Standing by";
        clearAllStates();
    } else if (s == "01") {
        s = "";
    } else {
        s = "power:" + s;
    }
    
    g_power = s;
}

void
processMessage(String s)
{
    if (s.length() <= 5) {
        myPrint("short message");
        myPrint(s);
        delay(2000);
        return; // empty or invalid message.
    }

    String sub = s.substring(0, 5);
    String remainder = s.substring(5, 7);

    if (sub == "!1MVL") {
       processVolume(remainder);
    } else if (sub == "!1LMD") {
       processListenMode(remainder);
    } else if (sub == "!1SLI") {
       processInputSource(remainder);
    } else if (sub == "!1AMT") {
       processMuting(remainder);
    } else if (sub == "!1PWR") {
       processPower(remainder); 
    } else {
       myPrint("unknown message");
       myPrint(s);
       delay(2000);
    }
}


void updateScreen()
{
    //
    //  First line
    // 
    String line = "";
    if (g_volume != "") {
        line = "" + g_volume;
    }
    
    while (line.length() < 10) {
        line = line + " ";
    }
    
    line = line + g_inputSource;
    while (line.length() < MAX_LCD_COLS) {
        line = line + " ";
    }
    
    lcd.setCursor(0, 0);
    lcd.print(line);
    
    //
    //  second line
    // 
    if (g_muting != "") {
        line = "------ MUTING ------";
    } else if (g_power != "") {
        line = g_power;
    } else {
        line = g_listenMode;
    }
    
    while (line.length() < MAX_LCD_COLS) {
        line = line + " ";
    }
    
    lcd.setCursor(0, 1);
    lcd.print(line);
}

void loop()
{
    loopCount++;
     
    // we check the volume more frequently than not...
    boolean everything = (loopCount % 3) == 0;
    sendCommands(everything);
    updateScreen();
    
    delay(200);  
}
