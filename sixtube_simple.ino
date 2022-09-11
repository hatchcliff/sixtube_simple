// Sixtube_simple, Version 1.0.0, 4th January 2022.
// ================================================
//
// Program for a 6-digit, Arduino Nano, SN74141/DS3231 clock from RLB Designs (http://rlb-designs.com/),
// based on original code by Robin Birtles and Chris Gerekos.
//
// The program is designed with limited functionality, with the intention of making the clock as easy as
// easy to set up as possible.  Date and time functions and an automatic cathode cleaning cycle are
// provided, but there are no alarm functions, and no run-time selectable options.
//
// A set of 8 #define directives set various compile-time options.  Of these, the following 6 are probably the
// most interesting with regard to customising the clock:
//     - FADE      Fading or instantaneous display changes.
//     - FSKIP     Adjustable rate of fade.
//     - CLOCK12   12 or 24 hour clock.
//     - PERDATE   Periodic display of the date in time mode (for 9 seconds at 03, 13, 23 etc. minutes past the hour).
//     - AUTO_BST  Automatic switching between Greenwich Mean Time (GMT) and British Summer Time (BST).
//     - CLEAN     Time in milliseconds to hold each number on display during the cathode cleaning cycle.
// These options are selected by including or commenting out their #define directives, or by editing an associated
// numerical value, as in the case of FSKIP and CLEAN.
//
// The clock is operated using 4 buttons: <mode>, <set>, <+> and <->.
//     - <mode>  Toggles between TIME and DATE modes.
//     - <set>   Toggles between SET and DISPLAY modes when pressed for 0.7 seconds or longer.
//               In SET mode, one of the three pairs of Nixie tubes displaying hours, minutes etc. blinks on and off to
//               indicate that it is selected and may be adjusted with <+> and <-> buttons.
//               Short presses on <set> (less than 0.7 seconds) select the next pair in rotation.
//               When setup is complete, a final long press on <set> toggles back to DISPLAY mode.
//
// TIME/DATE and SET/DISPLAY modes are selected independently, so the clock can show:
//     - Time in display mode.
//     - Date in display mode.
//     - Time in set mode.
//     - Date in set mode.
//
// The <mode> and <set> buttons are unresponsive for a few seconds every 10 minutes during the automatic cathode
// cleaning cycle. 
//
// Cliff Hatch, 4th January 2022.


#include "RTClib.h"

#define PERSIST 7    //Milliseconds for which each tube is lit during the scan (one scan = one execution of loop()).
                     //The recommended setting is PERSIST=7, which gives plenty of scope for fading (by lighting the
                     //numbers fading in/out for increasing/decreasing portions of the PERSIST period).  PERSIST=7
                     //also works well without fading.
                 
                     //The 6 tubes are scanned in 3 pairs.  The time taken for a complete scan (performed during a single
                     //execution of loop()) is approximately (3 * PERSIST) + 2 ms.  The  extra 2 milliseconds is spent
                     //doing things other than refreshing the display, such as checking for button presses.  The scan
                     //frequency is approximately 1000 / ((3 * PERSIST) + 2) Hz.  So setting PERSIST=7 gives a
                     //scan rate of 44 Hz.

                     //Settings higher than 7 are not useful because they slow the scan to the point where visible flicker occurs.

#define FADE         //Define to fade numbers in and out gradually when the display changes.  Comment out the definition
                     //to change numbers instantaneously.

#define FSKIP 2      //Fade skip.  Set to 1 to perform fade calculations on every scan, 2 to perform them on every 2nd scan, 3 on
                     //every 3rd scan etc..  Higher numbers make fading slower.  Setting FSKIP=1 or 2 (and PERSIST=7) produce
                     //nice subtle effects.  FSKIP=3 makes fading very slow, and perhaps less attractive.  Values higher
                     //than 3 are not useful because they produce visible flicker.

                     //If FADE is not defined, the value of FSKIP has no effect.

#define CLOCK12      //Define CLOCK12 to obtain a 12 hour clock display.  Comment out the definition to obtain a 24 hour display.
                     //Note that this definition does not affect set mode; it always uses 24 hour format and this must be set
                     //correctly to ensure that the date increments at midnight, rather than midday.

#define PERDATE      //Define PERDATE to display the date periodically in time mode (for 9 seconds at 03, 13, 23 etc. minutes
                     //past the hour).  Comment out the definition to remove this feature.

#define AUTO_BST     //Define AUTO_BST to change automatically between Greenwich Mean Time (GMT) and  British Summer Time (BST).
                     //Comment out the definition to remove this feature.

                     //When the time is changed automatically, the seconds are reset along with the hour.  This introduces
                     //an error, of magnitude between 0 milliseconds and the time taken to execute one scan.  Assuming the
                     //normal scan rate of 44 Hz, the error is in the range 0 to 23 milliseconds.  The specified accuracy
                     //of the DS3231 real time clock is ±2ppm when operating in the temperature range 0°C to +40°C.  This
                     //amounts to ±63 seconds per year, so an additional error of 0.046 seconds maximum resulting from two
                     //automatic clock changes per year is insignificant.

#define CLEAN 350    //The period in milliseconds to display each number when cycling to reduce cathode poisoning.

#define TOFF 10      //Value used to turn tubes off with no number displayed.

RTC_DS3231 rtc; // Real time clock object 
DateTime nNow;  //The date and time now
TimeSpan nSecondSpan, nMinuteSpan, nHourSpan, nDaySpan; //Second, minute, hour and day expressed as TimeSpans for adjusting the clock.

//Global variables for receiving date/time data from the real time clock.
byte nHour, nMinute, nSecond, nDay, nMonth;
int nYear;          //Four digit year, e.g. 2021

byte nNumber[6] =    {TOFF, TOFF, TOFF, TOFF, TOFF, TOFF};  //Numbers for display.  Tube indices, left to right, are 5, 1, 3, 2, 4, 0.

// a, b, c, d connections to Nixie tube driver SN74141(1)
int nCathode_1_a = 2;                
int nCathode_1_b = 3;
int nCathode_1_c = 4;
int nCathode_1_d = 5;

// a, b, c, d connections to Nixie tube driver SN74141(2)
int nCathode_2_a = 6;                
int nCathode_2_b = 7;
int nCathode_2_c = 8;
int nCathode_2_d = 9;

// Connections to Nixie tube anodes.
// The 6 tubes are arranged as 3 pairs, each pair sharing an anode connection.
int nAnode1 = 11;  //Tubes 0 and 3 (digitbered left to right)
int nAnode2 = 12;  //Tubes 1 and 4
int nAnode3 = 13;  //Tubes 2 and 5

#ifdef AUTO_BST
byte nDayOfTheWeek; //0=Sunday, 1=Monday, 2=Tuesday, etc..
#endif //AUTO_BST

void setup() 
{
    #define DTIME "14:00:00" //Default time to set when restarting after power fail.
                             //Make the default in the afternoon, so that when the user enters set mode
                             //the 24 hour format is clear (set mode always uses 24 hour, regardless of
                             //the display mode setting, see #define CLOCK12 above).
    
    Serial.begin(57600);   // Open the USB port at 57600 baud
            
    rtc.begin();          // Start the real time clock

    Serial.println("Starting sixtube_simple, V1.0");
     
    if (rtc.lostPower())
    {
        char s[80];
        
        // Real time clock power failed, reset to DTIME on the date the sketch was compiled.
        rtc.adjust(DateTime( __DATE__, DTIME));  //Set default datetime
        
        //Report the setting on the serial port
        sprintf(s, "Starting real time clock with default setting %s %s", __DATE__, DTIME);
        Serial.println(s);
    }

    //Configure Arduino outputs to Nixie tube display.
    //The 6 tubes are lit in pairs during the scan.  Each pair shares one anode connection.  
    pinMode(nAnode1, OUTPUT);      
    pinMode(nAnode2, OUTPUT);
    pinMode(nAnode3, OUTPUT);  
    //Two SN74141 drivers receive a, b, c and d signals which select the cathodes/numbers to be displayed. 
    pinMode(nCathode_1_a, OUTPUT);      
    pinMode(nCathode_1_b, OUTPUT);      
    pinMode(nCathode_1_c, OUTPUT);      
    pinMode(nCathode_1_d, OUTPUT);    
    pinMode(nCathode_2_a, OUTPUT);      
    pinMode(nCathode_2_b, OUTPUT);      
    pinMode(nCathode_2_c, OUTPUT);      
    pinMode(nCathode_2_d, OUTPUT);      
   
    //All analog pins as digital inputs with pull-up resistor activated.
    pinMode(A0, INPUT_PULLUP);   // SET   button
    pinMode(A1, INPUT_PULLUP);   // MODE  button  
    //pinMode(A2, INPUT_PULLUP); // ALARM button - not used in this version
    pinMode(A3, INPUT_PULLUP);   // MINUS button
    pinMode(A6, INPUT);          // PLUS  button (1K resistor to +VCC)
    //pinMode(A7, INPUT);        // Spare
    //pinMode(10, OUTPUT);       // ALARM tone output - not used in this version 
    digitalWrite (A6, true);     // set input high
    //digitalWrite (A7, HIGH);   // set input high

    //Initialise time spans for clock adjustment
    nSecondSpan = TimeSpan(0, 0, 0, 1);
    nMinuteSpan = TimeSpan(0, 0, 1, 0);
    nHourSpan   = TimeSpan(0, 1, 0, 0);
    nDaySpan    = TimeSpan(1, 0, 0, 0);
}


void SetSN74141(byte nEncode1, byte nEncode2)
{
    // SN74141 decoder/driver truth table.
    // d c b a  #
    // L,L,L,L  0
    // L,L,L,H  1
    // L,L,H,L  2
    // L,L,H,H  3
    // L,H,L,L  4
    // L,H,L,H  5
    // L,H,H,L  6
    // L,H,H,H  7
    // H,L,L,L  8
    // H,L,L,H  9
    // Numbers outside the range 0 to 9 display blank
    
    int a,b,c,d;  //Inputs to SN74141s
    
    extern int nCathode_1_a, nCathode_1_b, nCathode_1_c, nCathode_1_d;
    extern int nCathode_2_a, nCathode_2_b, nCathode_2_c, nCathode_2_d;
  
    //Set a, b, c and d inputs to SN74141(1)
    switch( nEncode1 )
    {
        case 0: a=0; b=0; c=0; d=0; break;
        case 1: a=1; b=0; c=0; d=0; break;
        case 2: a=0; b=1; c=0; d=0; break;
        case 3: a=1; b=1; c=0; d=0; break;
        case 4: a=0; b=0; c=1; d=0; break;
        case 5: a=1; b=0; c=1; d=0; break;
        case 6: a=0; b=1; c=1; d=0; break;
        case 7: a=1; b=1; c=1; d=0; break;
        case 8: a=0; b=0; c=0; d=1; break;
        case 9: a=1; b=0; c=0; d=1; break;
        default: a=1; b=1; c=1; d=1;
        break;
    }  
  
    //Write a, b, c and d to to SN74141(1).
    digitalWrite(nCathode_1_a, a);
    digitalWrite(nCathode_1_b, b);
    digitalWrite(nCathode_1_c, c);
    digitalWrite(nCathode_1_d, d);

    //Set a, b, c and d inputs to SN74141(2)
    switch( nEncode2 )
    {
        case 0: a=0; b=0; c=0; d=0; break;
        case 1: a=1; b=0; c=0; d=0; break;
        case 2: a=0; b=1; c=0; d=0; break;
        case 3: a=1; b=1; c=0; d=0; break;
        case 4: a=0; b=0; c=1; d=0; break;
        case 5: a=1; b=0; c=1; d=0; break;
        case 6: a=0; b=1; c=1; d=0; break;
        case 7: a=1; b=1; c=1; d=0; break;
        case 8: a=0; b=0; c=0; d=1; break;
        case 9: a=1; b=0; c=0; d=1; break;
        default: a=1; b=1; c=1; d=1;
        break;
    }
  
    //Write a, b, c and d to SN74141(2)
    digitalWrite(nCathode_2_a, a);
    digitalWrite(nCathode_2_b, b);
    digitalWrite(nCathode_2_c, c);
    digitalWrite(nCathode_2_d, d);
}


//Display all 6 digits.
void DisplayDigits()
{
    extern byte nNumber[];  //Numbers to display
    extern int nAnode1, nAnode2, nAnode3; //Anode connections

    #ifdef FADE

    //Code for display with fade
    
    static int nOldNumber[6] = {TOFF, TOFF, TOFF, TOFF, TOFF, TOFF};  //When the display changes, fade out these old numbers and fade in the new ones from nNumber[].
    static int nFadeIn;            //Fade in counter.  Starts at zero and increments on successive scans, reaching PERSIST when fading is complete.
    static int nFadeOut;           //Fade out counter.  Starts at PERSIST and decrements on successive scans, reaching zero when fading is complete.
    static int nFadeSkip = FSKIP;  //Fade skip counter
    static int j;

    for ( j=0; j<6; j++ )
        if ((nOldNumber[j] != nNumber[j]) && (nFadeOut == 0))
            nFadeOut = PERSIST;    //At least one digit has changed, initialise the fade out counter

    nFadeIn = PERSIST - nFadeOut;
    
    // Anode channel 1 - tubes 0, 3
    SetSN74141(nOldNumber[3], nOldNumber[0]);
    digitalWrite(nAnode1, true);   
    delay(nFadeOut);
    SetSN74141(nNumber[3], nNumber[0]);   
    delay(nFadeIn);
    digitalWrite(nAnode1, false);
  
    // Anode channel 2 - tubes 1, 4
    SetSN74141(nOldNumber[4], nOldNumber[1]);
    digitalWrite(nAnode2, true);   
    delay(nFadeOut);
    SetSN74141(nNumber[4], nNumber[1]);   
    delay(nFadeIn);
    digitalWrite(nAnode2, false);
  
    // Anode channel 3 - tubes 2, 5
    SetSN74141(nOldNumber[5], nOldNumber[2]);   
    digitalWrite(nAnode3, true);   
    delay(nFadeOut);
    SetSN74141(nNumber[5], nNumber[2]);   
    delay(nFadeIn);
    digitalWrite(nAnode3, false);

    if (nFadeOut > 0)
    {
        nFadeSkip--;

        if (nFadeSkip == 0)
        {
            //Perform fade calculations
            nFadeOut--;  //Decrement the fade out counter
 
            if (nFadeOut == 0)
                for ( j=0; j<6; j++ )
                    nOldNumber[j] = nNumber[j];  //Fading complete.  Set old numbers to current numbers ready for next change.
    
            nFadeSkip = FSKIP;  //Reset fade skip counter
        }
    } 

    #else  //FADE

    //Code for display without fade
    
    // Anode channel 1 - tubes 0, 3
    SetSN74141(nNumber[3], nNumber[0]);
    digitalWrite(nAnode1, true);   
    delay(PERSIST);
    digitalWrite(nAnode1, false);
  
    // Anode channel 2 - tubes 1, 4
    SetSN74141(nNumber[4], nNumber[1]);
    digitalWrite(nAnode2, true);
    delay(PERSIST);   
    digitalWrite(nAnode2, false);
  
    // Anode channel 3 - tubes 2, 5
    SetSN74141(nNumber[5], nNumber[2]);   
    digitalWrite(nAnode3, true);   
    delay(PERSIST);
    digitalWrite(nAnode3, false);

    #endif //FADE
}


//Read date and time from the real time clock into global variables
void ReadRealTimeClock()
{
    extern RTC_DS3231 rtc; // Real time clock object 
    extern byte nSecond, nMinute, nHour, nDay, nMonth;
    extern int nYear;

    #ifdef AUTO_BST
    extern byte nDayOfTheWeek;
    #endif //AUTO_BST

    nNow = rtc.now(); //Read the real time clock

    //Set global variables
    nSecond = nNow.second();
    nMinute = nNow.minute();
    nHour   = nNow.hour();
    nDay    = nNow.day();
    nMonth  = nNow.month();
    nYear   = nNow.year();

    #ifdef AUTO_BST
    nDayOfTheWeek = nNow.dayOfTheWeek();
    #endif //AUTO_BST
}


//Advance/retard the clock by nSpan.
//Used to adjust days, hours, minutes and seconds in set mode.
//bAdvance = true to advance, false to retard.
void AdjustDHMS(bool bAdvance, TimeSpan nSpan)
{
    extern DateTime nNow;

    if (bAdvance)
        rtc.adjust(nNow + nSpan); //Advance
    else
        rtc.adjust(nNow - nSpan); //Retard
}


//Advance/retard one month.
//This cannot be done in the same way as days, hours, minutes and seconds because different months
//represent different time spans.  This function adjusts the month, and wraps it if necessary (to keep
//it within the range 1 to 12) without carrying to the year.
//bAdvance = true to advance, false to retard.
void AdjustMonth(bool bAdvance)
{
    extern DateTime nNow;
    extern byte nSecond, nMinute, nHour, nDay, nMonth;
    extern int nYear;

    if(bAdvance)
    {
        nMonth++;

        if (nMonth == 13)
            nMonth = 1; //Wrap around to January
            
        nNow = DateTime(nYear, nMonth, nDay, nHour, nMinute, nSecond);  //Advance one month
    }
    else
    {
        nMonth--;

        if (nMonth == 0)
            nMonth = 12; //Wrap around to December
            
        nNow = DateTime(nYear, nMonth, nDay, nHour, nMinute, nSecond);  //Retard one month
    }
        
    rtc.adjust(nNow);
}


//Advance/retard one year
//This cannot be done in the same way as days, hours, minutes and seconds because different years
//represent different time spans.
//bAdvance = true to advance, false to retard.
void AdjustYear(bool bAdvance)
{
    extern DateTime nNow;
    extern byte nSecond, nMinute, nHour, nDay, nMonth;
    extern int nYear;

    if(bAdvance)
        nNow = DateTime(++nYear, nMonth, nDay, nHour, nMinute, nSecond);  //Advance one year
    else
        nNow = DateTime(--nYear, nMonth, nDay, nHour, nMinute, nSecond);  //Retard one year
        
    rtc.adjust(nNow);
}


//Adjust the date/time.
//nPair specifies the pair of digits to adjust.
//bTimeMode specifies whether to change date or time.
//bAdvance = true to advance, false to retard.
void Adjust(byte nPair, bool bTimeMode, bool bAdvance)
{ 
    if(nPair == 1)
    {
        if(bTimeMode)
            AdjustDHMS(bAdvance, nHourSpan);   //Advance/retard 1 hour
        else
            AdjustDHMS(bAdvance, nDaySpan);    //Advance/retard 1 day
    }
    else if(nPair == 2)
    {
        if(bTimeMode)
            AdjustDHMS(bAdvance, nMinuteSpan); //Advance/retard 1 minute
        else              
            AdjustMonth(bAdvance);             //Advance/retard 1 month
    }
    else if(nPair == 3)
    {
        if(bTimeMode)
            AdjustDHMS(bAdvance, nSecondSpan); //Advance/retard 1 second
        else
            AdjustYear(bAdvance);              //Advance/retard 1 year
    }
}


void loop()
{
    //Numbers for display
    static byte nSecondLo, nSecondHi;
    static byte nMinuteLo, nMinuteHi;
    static byte nHourLo, nHourHi;
    static byte nDayLo, nDayHi;
    static byte nMonthLo, nMonthHi;
    static byte nYearLo, nYearHi;

    //Button status
    static byte nModeInput;
    static byte nSetInput;
    static int nPlusInput;
    static int nMinusInput;

    //Button pressed flags
    static bool bModeButtonPressed = false;
    static bool bSetButtonPressed = false;
    static bool bPlusButtonPressed = false;
    static bool bMinusButtonPressed = false;
    
    //Mode flags
    static bool bTimeMode = true;  //true = TIME mode, false = DATE mode.
    static bool bSetMode = false;  //true = SET mode, false = DISPLAY mode
        
    static bool bBlink = false;  //Toggles between true and false every 500 milliseconds to control the blinking of pairs of tubes in set mode, true = turn tubes on, false = turn tubes off
    static byte nPair = 1;       //Pair of Nixie tubes selected in set mode: 1=left, 2=middle, 3=right.  nPair is incremented every time the <set> button is pressed.
    static long nSetTimer;       //Used to discriminate between long and short presses of the set button.
        
    static unsigned long nRunTime;    //Run time in milliseconds since the program started, returned by millis().  Overflows to zero every 50 days approximately.
    static unsigned long nBlinkTime;  //Run time in milliseconds at which last blink event occurred.  Digits are blinked to show that they may be adjusted with the + and - buttons.
    static unsigned long nEventTime;  //General purpose variable for timing.

    static byte nYearTwoDigit; //Year expressed in two digits, e.g. 21

    //Global variables for receiving date/time data from the real time clock.
    extern byte nHour, nMinute, nSecond, nDay, nMonth;
    extern int nYear;
   
    extern DateTime nNow; //The time now

    #ifdef FADE
    static int nFadeTime = ((3 * PERSIST) + 2) * FSKIP;  //The time taken to complete fades in milliseconds
    #endif  //FADE

    #ifdef CLOCK12
    static byte nHour12;     //Supports 12 hour clock display
    #endif //CLOCK12

    #ifdef PERDATE
    static bool bPerDate = false; //true = periodic date display in progress, false = not in progress, displaying time normally.
    #endif  //PERDATE
     
    #ifdef AUTO_BST
    static bool bAutoChange = false;
    #endif //AUTO_BST

    nPlusInput = analogRead(A6) < 100 ? 0 :1; //Measures the I/P on A6 is less that +5V
    nMinusInput = digitalRead(A3);
        
    // TOGGLE BETWEEN TIME AND DATE MODE /////////////////////////
    ////////////////////////////////////////////////////////////// 
     
    nModeInput = digitalRead(A1);
    
    if(nModeInput == 0)
        bModeButtonPressed = true;

    if (bModeButtonPressed && (nModeInput == 1))
    {
        //The mode button was pressed and has now been released
        bTimeMode = !bTimeMode; //Toggle between TIME and DATE modes.

        bModeButtonPressed = false;
    }

    // TOGGLE BETWEEN SET AND DISPLAY MODE /////////////
    ////////////////////////////////////////////////////
     
    nSetInput = digitalRead(A0);

    //Check for a long press on the set button.
    if(nSetInput == 0)
    {
        bSetButtonPressed = true;
        nSetTimer = 0;

        //Measure the length of the button press with 10 millisecond resolution.       
        //Scanning is suspended and the display goes blank while this loop is processed.
        //Monitoring the pressed status of the set button is the program's only task for the moment.
        while(nSetInput == 0)
        {
            delay(10);  //Delay 10 milliseconds
            nSetTimer++;
            nSetInput = digitalRead(A0);
        } 

        //The set button has been released.
        if(nSetTimer >= 70)
        {
            //The button was pressed for 700 milliseconds (0.7 seconds) or more.  Toggle between SET and DISPLAY modes.
            bSetMode = !bSetMode;
            
            if (!bSetMode)
            {
                nPair = 1;  //Default the selected pair back to 1 when exiting set mode.
                bPerDate = false;  //Clear flag, in case the periodic date display was on when set mode was entered
            }
        }
    }

    // READ THE DS3231 REAL TIME CLOCK //
    ////////////////////////////////////

    //Read the real time clock once per scan.
    //Read the data into the global variables nNow, nYear, nMonth, nDay, nHour, nMinute, nSecond and nDayOfTheWeek.
    //Work with these variables for the remainder of the scan.
    ReadRealTimeClock();

    #ifdef AUTO_BST

    // AUTOMATIC GMT/BST CHANGES //
    ///////////////////////////////
    
    if ((nDayOfTheWeek == 0) && (nDay >= 25) && (nMinute == 0) && (nSecond == 0) && !bAutoChange)
    {
        //It is Sunday, on the hour, in the last week of the month (assuming a 31 day month).  No automatic time change executed yet.
        if ((nHour == 1) && (nMonth == 3))
        {
            AdjustDHMS(true, nHourSpan);  //It is 01:00:00 GMT on the last Sunday of March.  Advance the clock 1 hour to 02:00:00 BST.
            bAutoChange = true;           //Flag automatic time change executed
        }
      
        if ((nHour == 2) && (nMonth == 10))
        {
            AdjustDHMS(false, nHourSpan);  //It is 02:00:00 BST on the last Sunday of October.  Retard the clock 1 hour to 01:00:00 GMT.
            bAutoChange = true;            //Flag automatic time change executed
        }
   }
           
   if (bAutoChange)
       if (nHour == 3)
           bAutoChange = false;  //Reset the flag at 03:00:00 BST or GMT, ready for next time.
        
    #endif //AUTO_BST

    // CHECK SET, PLUS AND MINUS BUTTONS IN SET MODE //
    ///////////////////////////////////////////////////
    
    if(bSetMode)
    {
        if (bSetButtonPressed && (nSetInput == 1))
        {
            //The set button was pressed and has now been released.
            //Switch to the next pair of digits in order 1, 2, 3, back to 1 etc..
            if (++nPair > 3)
                nPair = 1;
    
            bSetButtonPressed = false;
        }
 
        //Detect plus/minus buttons pressed
        nPlusInput = analogRead(A6) < 100 ? 0 :1; //Measures the I/P on A6 is less that +5V
        nMinusInput = digitalRead(A3);
  
        //Set flags accordingly
        if(nPlusInput == 0)
            bPlusButtonPressed = true;
       if(nMinusInput == 0)
            bMinusButtonPressed = true;
            
        if((bPlusButtonPressed == true) && (nPlusInput == 1))
        {
            //The plus button was pressed and has now been released.
            bPlusButtonPressed = false; //Clear flag

            Adjust(nPair, bTimeMode, true); //Advance the clock
        }
        
        if(bMinusButtonPressed && (nMinusInput == 1))
        {
            //The minus button was pressed and has now been released.
            bMinusButtonPressed = false; //Clear flag

            Adjust(nPair, bTimeMode, false);  //Retard the clock
        }
    }
    
    // ASSEMBLE NUMBERS TO DISPLAY //
    /////////////////////////////////
    
    //Determine high and low order numbers
    nYearTwoDigit = nYear % 100;
    nYearLo = nYearTwoDigit % 10;
    nYearHi = nYearTwoDigit / 10;
    
    nMonthLo = nMonth % 10;
    nMonthHi = nMonth / 10;
    
    nDayLo = nDay % 10;
    nDayHi = nDay / 10;

    #ifdef CLOCK12
    
    //Support 12 hour clock if CLOCK12 is defined
    if (bSetMode)
    {
        //Always display 24 hour clock in set mode
        nHourLo = nHour % 10;
        nHourHi = nHour / 10;
    }
    else
    {
        //Display 12 hour clock in display mode
        if (nHour <= 12)
            nHour12 =  nHour;
        else
            nHour12 = nHour - 12;

        nHourLo = nHour12 % 10;
        nHourHi = nHour12 / 10;
    }
    
    #else //CLOCK12
    
    //Support 24 hour clock if CLOCK12 is not defined
    nHourLo = nHour % 10;
    nHourHi = nHour / 10;
    
    #endif //CLOCK12

    nMinuteLo = nMinute % 10;
    nMinuteHi = nMinute / 10;

    nSecondLo = nSecond % 10;
    nSecondHi = nSecond / 10;

    nRunTime = millis();  //Get milliseconds since program started to run, for controlling blinking.
 
    //Check to see if nRunTime has overflowed to zero (happens every 50 days or so).
    //Reset nBlinkTime if overflow has occurred.  nBlinkTime is used to blink digits on and off in set mode.
    //Resetting it alters the timing of one blink but occurs very rarely - only if you happen to  be setting
    //the clock when overflow occurs.
    if (nRunTime < nBlinkTime)
        nBlinkTime = 0;
  
    // TIME MODE ///////////////////////////////////////
    ////////////////////////////////////////////////////
    
    if(bTimeMode)
    {
        if(bSetMode)
        {
            //The clock is showing TIME in SET mode.
           
            if(nRunTime > nBlinkTime + 500)  //Blink the selected pair of tubes on/off every 500 milliseconds.
            {
                bBlink = !bBlink;       //Toggle the blink flag  
                nBlinkTime = nRunTime;  //Reset blink time counter

                //Blink the appropriate pair of digits
                if(nPair == 1)
                {
                    if(bBlink)
                    {
                        //Turn pair 1 on
                        nNumber[5] = nHourHi;
                        nNumber[1] = nHourLo;
                    }
                    else
                    {
                        //Turn pair 1 off
                        nNumber[5] = TOFF;
                        nNumber[1] = TOFF;
                    }

                    //Make sure the other pairs are on                
                    nNumber[3] = nMinuteHi;
                    nNumber[2] = nMinuteLo;
                    nNumber[4] = nSecondHi;
                    nNumber[0] = nSecondLo;
                }
                else if(nPair == 2)
                {
                    if(bBlink)
                    {
                        //Turn pair 2 on
                        nNumber[3] = nMinuteHi;
                        nNumber[2] = nMinuteLo;
                    }
                    else
                    {
                        //Turn pair 2 off
                        nNumber[3] = TOFF;
                        nNumber[2] = TOFF;
                    }
                
                    //Make sure the other pairs are on
                    nNumber[5] = nHourHi;
                    nNumber[1] = nHourLo;
                    nNumber[4] = nSecondHi;
                    nNumber[0] = nSecondLo;
                }
                else if(nPair == 3)
                {
                    if(bBlink)
                    {
                        //Turn pair 3 on
                        nNumber[4] = nSecondHi;
                        nNumber[0] = nSecondLo;
                    }
                    else
                    {
                        //Turn pair 3 off
                        nNumber[4] = TOFF;
                        nNumber[0] = TOFF;
                    }

                    //Make sure the other pairs are on
                    nNumber[5] = nHourHi;
                    nNumber[1] = nHourLo;
                    nNumber[3] = nMinuteHi;
                    nNumber[2] = nMinuteLo;
                }
            
            }
        }
        else
        {
            //The clock is showing TIME in DISPLAY mode.

            #ifdef PERDATE
            
            // DISPLAY DATE PERIODICALLY IN TIME MODE /////
            ///////////////////////////////////////////////
            
            //Display date for 9 seconds every 03:01, 13:01, 23:01, etc. minutes past the hour.
            if((nMinuteLo == 3) && (nSecond == 1) && !bPerDate)
                bPerDate = true;
            else if ((nMinuteLo == 3) && (nSecond == 10) && bPerDate)
                bPerDate = false;

            if (bPerDate)
            {
                 //Date numbers for display
                 nNumber[5] = nDayHi;
                 nNumber[1] = nDayLo;
                 nNumber[3] = nMonthHi;
                 nNumber[2] = nMonthLo;
                 nNumber[4] = nYearHi;
                 nNumber[0] = nYearLo;
            }
            else
            {
                //Time numbers for display
                nNumber[5] = nHourHi;
                nNumber[1] = nHourLo;
                nNumber[3] = nMinuteHi;
                nNumber[2] = nMinuteLo;
                nNumber[4] = nSecondHi;
                nNumber[0] = nSecondLo;
            }
            
            #else  //PERDATE

            //Time numbers for display
            nNumber[5] = nHourHi;
            nNumber[1] = nHourLo;
            nNumber[3] = nMinuteHi;
            nNumber[2] = nMinuteLo;
            nNumber[4] = nSecondHi;
            nNumber[0] = nSecondLo;

            #endif  //PERDATE
        
        }
        
        DisplayDigits();  // Display time

    }

    // DATE MODE /////
    //////////////////
   
    else
    {
        if(bSetMode)
        {
            //The clock is showing the DATE in SET mode.
            
            if(nRunTime-nBlinkTime > 500)  //Blink selected pair of digits on/off every 500 milliseconds.
            {
                bBlink = !bBlink;      //Toggle the blink flag
                nBlinkTime = nRunTime; //Reset blink time counter
               
                //Blink the appropriate pair of digits
                if(nPair == 1)
                {
                    if(bBlink)
                    {
                        //Turn pair 1 on
                        nNumber[5] = nDayHi;
                        nNumber[1] = nDayLo;
                    }
                    else
                    {
                        //Turn pair 1 off
                        nNumber[5] = TOFF;
                        nNumber[1] = TOFF;
                    }

                    //Make sure the other pairs are on
                    nNumber[3] = nMonthHi;
                    nNumber[2] = nMonthLo;
                    nNumber[4] = nYearHi;
                    nNumber[0] = nYearLo;
                }
                else if(nPair == 2)
                {
                    if(bBlink)
                    {
                        //Turn pair 2 on
                        nNumber[3] = nMonthHi;
                        nNumber[2] = nMonthLo;
                    }
                    else
                    {
                        //Turn pair 2 off
                        nNumber[3] = TOFF;
                        nNumber[2] = TOFF;
                    }

                    //Make sure the other pairs are on
                    nNumber[5] = nDayHi;
                    nNumber[1] = nDayLo;
                    nNumber[4] = nYearHi;
                    nNumber[0] = nYearLo;
                }
                else if(nPair == 3)
                {
                     if(bBlink)
                     {
                         //Turn pair 3 on
                         nNumber[4] = nYearHi;
                         nNumber[0] = nYearLo;
                     }
                     else
                     {
                         //Turn pair 3 off
                         nNumber[4] = TOFF;
                         nNumber[0] = TOFF;
                     }
                     
                     //Make sure the other pairs are on
                     nNumber[5] = nDayHi;
                     nNumber[1] = nDayLo;
                     nNumber[3] = nMonthHi;
                     nNumber[2] = nMonthLo;
                 }
            }                 
        }
        else
        {
            //The clock is showing the DATE in DISPLAY mode.
            
            nNumber[5] = nDayHi;
            nNumber[1] = nDayLo;
            nNumber[3] = nMonthHi;
            nNumber[2] = nMonthLo;
            nNumber[4] = nYearHi;
            nNumber[0] = nYearLo;
        }
  
        DisplayDigits();  // Display date
    }
        
 
    // CATHODE CLEANING CYCLE //
    ////////////////////////////

   //This cycle ensures that all numbers are lit regulalry, which helps to reduce cathode poisoning.
   
    if (!bSetMode) //Don't run the cycle in set mode.
    {
        //Cycle the display periodically at 05:01, 15:01, 25:01 etc. past the hour.
        //All buttons are unresponsive during this cycle because normal scanning is suspended until it is complete.
        //If you alter the timing, don't make it on the hour, because this would interfere with automatic GMT/BST switching.
        if((nMinuteLo == 5) && (nSecond == 1))
        {
            #ifdef FADE
            //Fade to blank display (looks slightly better than fading to the cycling numbers directly).
            nNumber[5] = TOFF;
            nNumber[1] = TOFF;
            nNumber[3] = TOFF;
            nNumber[2] = TOFF;
            nNumber[4] = TOFF;
            nNumber[0] = TOFF;

            nRunTime = millis();
            nEventTime = nRunTime;

            while (nRunTime < nEventTime + nFadeTime)
            {
                DisplayDigits();
                nRunTime = millis();

                if (nRunTime < nEventTime)
                    nEventTime = 0;         //nRunTime has overflowed to zero.  Reset nEventTime.  Extends the fade time but occurs very rarely.
            }
            #endif  //FADE

            //Cycle through all numbers
            //Count alternate tubes up and down to make the display more interesting.
            for (int j=0; j<=9; j++)
            {
                nNumber[5] = j;    //Count up 
                nNumber[1] = 9-j;  //Count down
                nNumber[3] = j;    //Count up
                nNumber[2] = 9-j;  //Count down
                nNumber[4] = j;    //Count up
                nNumber[0] = 9-j;  //Count down

               #ifdef FADE
               //Fade these numbers onto the display
                nRunTime = millis();
                nEventTime = nRunTime;

                while(nRunTime < nEventTime + nFadeTime)
                {
                    DisplayDigits();
                    nRunTime = millis();

                    if (nRunTime < nEventTime)
                        nEventTime = 0;         //nRunTime has overflowed to zero.  Reset nEventTime.  Extends the clean time but occurs very rarely.
                }
                #endif  //FADE

                //Display numbers for CLEAN milliseconds
                nRunTime = millis();
                nEventTime = nRunTime;

                while(nRunTime < nEventTime + CLEAN)
                {
                    DisplayDigits();
                    nRunTime = millis();

                    if (nRunTime < nEventTime)
                        nEventTime = 0;         //nRunTime has overflowed to zero.  Reset nEventTime.  Extends the fade time but occurs very rarely.
                }

                #ifdef FADE
                //Fade back to blank display
                nNumber[5] = TOFF;
                nNumber[1] = TOFF;
                nNumber[3] = TOFF;
                nNumber[2] = TOFF;
                nNumber[4] = TOFF;
                nNumber[0] = TOFF;
    
                nRunTime = millis();
                nEventTime = nRunTime;
    
                while (nRunTime < nEventTime + nFadeTime)
                {
                    DisplayDigits();
                    nRunTime = millis();
    
                    if (nRunTime < nEventTime)
                        nEventTime = 0;         //nRunTime has overflowed to zero.  Reset nEventTime.  Extends the display time but occurs very rarely.
                }
                #endif  //FADE
            }
        }
    }
}
