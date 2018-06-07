/************************************************************
*
*		5/21/18 replaced debug_pc.printf with print
*
*
*************************************************************/

#ifndef PWP_BG96_
#define PWP_BG96_


#include "mbed.h"
#include "pinmap.h"         // pinmap needed for hardware flow control
#include "InitDevice.h"
#include "em_system.h"
#include "PursuitAlertRevCInterface.h"
#include "setupSWOForPrint.h"


#define CMD_TIMEOUT 10000    // modem response timeout in milliseconds


LowPowerTicker toggleTicker;

//DigitalOut skywire_reset(PF5);    // Skywire Enable  U5 to Reset Pchannel FET - When High Cell is Reset
//DigitalOut skywire_rts(PE13);		// CTS and RTS are connected
//DigitalOut skywire_on(PF4);    // Turn on skywire modem

enum Skywire_Modem {
    NL_SW_1xRTT_V,      // Verizon 2G Modem - CE910-DUAL
    NL_SW_1xRTT_S,      // Sprint 2G Modem - CE910-DUAL
    NL_SW_1xRTT_A,      // Aeris 2G Modem - CE910-DUAL
    NL_SW_GPRS,         // AT&T/T-Mobile 2G Modem
    NL_SW_EVDO_V,       // Verizon 3G Modem
    NL_SW_EVDO_A,       // Aeris 3G Modem
    NL_SW_HSPAP,        // AT&T/T-Mobile 3G Modem
    NL_SW_HSPAPG,       // AT&T/T-Mobile 3G Modem w/ GPS
    NL_SW_HSPAPE,       // GSM 3G Modem, EU
    NL_SW_LTE_TSVG,     // Verizon 4G LTE Modem ***** Our CAT3 Modem!  *****
    NL_SW_LTE_TNAG,     // AT&T/T-Mobile 4G LTE Modem
    NL_SW_LTE_TEUG,     // GSM 4G LTE Modem, EU
    NL_SW_LTE_GELS3,    // VZW LTE CAT 1 Modem
    NL_SW_LTE_S7588     // VZW LTE CAT 4 Modem
};

Skywire_Modem MODEM;

// --CHANGE THIS FOR YOUR SETUP (IF APPLICABLE)--
// the APN below is for private static IP addresses
// the LE910-based Skywire can automatically get the required APN
char const *APN = "NIMBLINK.GW12.VZWENTP";
Timer t;            // for timeout in WaitForResponse()
Timer t2;           // for timeout in read_line()
Ticker watchdog;    // to break out of infinite sleep() loops
volatile bool wakeup;

bool sw1;       // Boolean to check if button 1 is pressed
bool sw2;       // Boolean to check if button 2 is pressed

// char array for reading from Skywire
char str[255];

// Device IMEI to use as dweet thingname
char IMEI[16];
// Device Unique ID
uint64_t UniqueID; 

// Variables for GPS
float latitude;
float longitude;
int number;

// Variables for UART comms
volatile int rx_in=0;
volatile int rx_out=0;
const int buffer_size = 600;
char volatile rx_buffer[buffer_size+1];
char rx_line[buffer_size];

// Provisioning Flag for CE910 and DE910 only
int prov_flag = 0;

/**
* This is a callback! Do not call frequency-dependent operations here.
*
* For a more thorough explanation, go here: 
* https://developer.mbed.org/teams/SiliconLabs/wiki/Using-the-improved-mbed-sleep-API#mixing-sleep-with-synchronous-code
**/
void ledToggler(void) {
    led0_n = !led0_n;

}

void watchdog_wakeup() {
    wakeup = true;
}

// Function to blink LED0 for debugging
void blink_leds(int num)
{
    for (int i = 0; i < num; i++) {
        led1_n = 0;
        wait(0.5);
        led1_n = 1;
        wait(0.5);
    }
}


// Interrupt for the Skywire
void Skywire_Rx_interrupt()
{
// Loop just in case more than one character is in UART's receive FIFO buffer
// Stop if buffer full
    while ((cell.readable()) && (((rx_in + 1) % buffer_size) != rx_out)) {
        rx_buffer[rx_in] = cell.getc();
        rx_in = (rx_in + 1) % buffer_size;
    }
    return;
}

bool DataAvailable()
{
    return !(rx_in == rx_out);
}

void ClearBuffer()
{
    
    __disable_irq();
    rx_in = 0;
    rx_out = 0;
    __enable_irq();
}
// Read line from the UART
void read_line() 
{
    int i;
    i = 0;
    t2.reset();
    t2.start();
    
    // Loop reading rx buffer characters until end of line character
    while ((i==0) || (rx_line[i-1] != '\n')) {
        while (rx_in == rx_out) {
            if (t2.read_ms() > CMD_TIMEOUT)
            {
                t2.stop();
                return;
            }
        }
        
        if(rx_buffer[rx_out] != '\r')
        {
            rx_line[i] = rx_buffer[rx_out];
            i++;
        }

        rx_out = (rx_out + 1) % buffer_size;
    }
    rx_line[i-1] = 0;
    return;
}


// Wait for specific response from modem
bool WaitForResponse(const char *response) 
{
    //ClearDisplay();
    print("Waiting for:\n%s\n", response);
    print("Received:\n");
    //UpdateDisplay();
    t.reset();
    t.start();
    do {
        t.reset();
        while(!DataAvailable())
        {
            if (t.read_ms() > CMD_TIMEOUT)
            {
                print("Command timeout\n");
                //UpdateDisplay();
                t.stop();
                return false;
            }
        }
        read_line();
        // don't write to the LCD if the line is >= 100 characters
        if(strlen(rx_line) < 1000 && strlen(rx_line) > 1)
        {
            print("%s\n", rx_line);
            //UpdateDisplay();
        }
    } while (strncmp(rx_line, response, strlen(response)));
    
    wait(1.0);
    
    return true;
}

// Retries sending a modem command until the expected response is received
// timeout is in milliseconds (unimplemented)
bool SendUntilSuccess(const char* const message, const char* const expect, int timeout=10000)
{
    do {
        ClearBuffer();
        cell.printf(message);
    } while (!WaitForResponse(expect));
    
    return true;
}

// Get and parse the AT+GMM response
// Depending on what it gets, parse and return
int GetGMMResponse() 
{
    //int ret_val = 0;
    do {
        read_line();
        print("Waiting for: %s\nReceived: %s\n", "OK", rx_line);
        //UpdateDisplay();
        if (!strncmp(rx_line, "GE910-QUAD-V3", 13)) {
            MODEM = NL_SW_GPRS;
            print("Mdm: NL-SW-GPRS\n");
            //UpdateDisplay();
            //ret_val = 1;
            return 1;
        }
        if (!strncmp(rx_line, "CE910-DUAL", 10)) {
            MODEM = NL_SW_1xRTT_V;
            print("Mdm: NL-SW-1xRTT\n");
            //UpdateDisplay();
            //ret_val = 1;
            return 1;
        }
        if (!strncmp(rx_line, "DE910-DUAL", 10)) {
            MODEM = NL_SW_EVDO_V;
            print("Mdm: NL-SW-EVDO\n");
            //UpdateDisplay();
            //ret_val = 1;
            return 1;
        }
        if (!strncmp(rx_line, "HE910-NAD", 9)) {
            MODEM = NL_SW_HSPAP;
            print("Mdm: NL-SW-HSPAP\n");
            //UpdateDisplay();
            //ret_val = 1;
            return 1;
        }
        if (!strncmp(rx_line, "HE910-D", 7)) {
            MODEM = NL_SW_HSPAPG;
            print("Mdm: NL-SW-HSPAPG\n");
            //UpdateDisplay();
            //ret_val = 1;
            return 1;
        }
        if (!strncmp(rx_line, "LE910-SVG", 9)) {
            MODEM = NL_SW_LTE_TSVG;
            print("Mdm: NL-SW-LTE-TSVG\n");
            //UpdateDisplay();
            //ret_val = 1;
            return 1;
        }
        if (!strncmp(rx_line, "LE910-NAG", 9)) {
            MODEM = NL_SW_LTE_TNAG;
            print("Mdm: NL-SW-LTE-TNAG\n");
            //UpdateDisplay();
            //ret_val = 1;
            return 1;
        }
        if (!strncmp(rx_line, "HL7588", 6)) {
            MODEM = NL_SW_LTE_S7588;
            print("Mdm: NL-SW-LTE-S7588\n");
            //UpdateDisplay();
            //ret_val = 1;
            return 1;
        }
        if (!strncmp(rx_line, "ELS31-V", 7)) {
            MODEM = NL_SW_LTE_GELS3;
            print("Mdm: NL-SW-LTE-GELS3\n");
            //UpdateDisplay();
            //ret_val = 1;
            return 1;
        }
    } while (strncmp(rx_line, "OK", 2));
    return 0;
}


int GetSkywireModel()
{
    // send 3GPP standard command for model name
    print("Sending AT+CGMM\n");
    //UpdateDisplay();
    cell.printf("AT+CGMM\r");
    if (GetGMMResponse()) {
        return 0;
    }
    // Otherwise, we have an error (or no modem), so sit here and blink
    else {
        print("ERROR: Unable to detect modem\n");
        //UpdateDisplay();
        while(1) {
            blink_leds(1);
        }
    }
}

int GetMEID()
{
    switch(MODEM)
    {
        case NL_SW_1xRTT_V:      // Verizon 2G Modem - CE910-DUAL
            cell.printf("AT#MEID?\r");
            WaitForResponse("#MEID");
            sscanf(rx_line, "#MEID: %14s", IMEI);
            ClearBuffer();
            break;
        case NL_SW_1xRTT_S:      // Sprint 2G Modem - CE910-DUAL
        case NL_SW_1xRTT_A:      // Aeris 2G Modem - CE910-DUAL
        case NL_SW_GPRS:         // AT&T/T-Mobile 2G Modem
        case NL_SW_EVDO_V:       // Verizon 3G Modem
        case NL_SW_EVDO_A:       // Aeris 3G Modem
        case NL_SW_HSPAP:        // AT&T/T-Mobile 3G Modem
        case NL_SW_HSPAPG:       // AT&T/T-Mobile 3G Modem w/ GPS
        case NL_SW_HSPAPE:       // GSM 3G Modem, EU
        case NL_SW_LTE_TSVG:     // Verizon 4G LTE Modem
        case NL_SW_LTE_TNAG:     // AT&T/T-Mobile 4G LTE Modem
        case NL_SW_LTE_TEUG:     // GSM 4G LTE Modem, EU
        case NL_SW_LTE_GELS3:    // VZW LTE CAT 1 Modem
        case NL_SW_LTE_S7588:    // VZW LTE CAT 4 Modem    
            ClearBuffer();
						print("AT+CGSN\n");
						cell.printf("AT+CGSN\r");
            do {
                while(!DataAvailable()) ;
                read_line();
            } while (rx_line[0] == 0);
            sscanf(rx_line, "%15s", IMEI);
						WaitForResponse("OK");
            ClearBuffer();
            break;
        default:
            //sprintf(IMEI, "%s", "nimbelinkisawesome");
				    print(IMEI, "%s", "nimbelinkisawesome");
            break;
    }
    return 0;
}
// Reads the signal strength received by the modem
int GetRSSI()
{
    int rssi;
    cell.printf("AT+CSQ\r");
    WaitForResponse("+CSQ:");
    sscanf(rx_line, "+CSQ: %i,%*i", &rssi);
	  print("RSSI: %s\n", rssi);
    ClearBuffer();
    return rssi;
}

// Function to print the delay time
void wait_print(int time)
{
    for (int i = time; i >= 0; i--)
    {
        //ClearDisplay();
        print("Countdown: %d\n", i);
        //UpdateDisplay();
        wait(1.0);
    }
    print("Delay finished!\n");
    //UpdateDisplay();
    return;
}


// Automatically sets APN based on SIM card type
bool AutoAPN()
{
    bool succeeded = false;

    cell.printf("AT#OTAUIDM=0\r");
    WaitForResponse("OK");
    do {
        while(!DataAvailable()) ;
        read_line();

        // 918 is the response code for successfully receiving the APN
        if (!strcmp(rx_line, "#OTAEV: #918"))
        {
            succeeded = true;
        }
        print("%s\n", rx_line);
        //UpdateDisplay();
    } while (strcmp(rx_line, "#OTAEV: #DREL"));
    
    return succeeded;
}

void PWP_SW_BG96_test(void) {
	    	
	 int rssi;
	//enter_DefaultMode_from_RESET();
  //toggleTicker.attach(&ledToggler, 0.2f);	

	 watchdog.attach(&watchdog_wakeup, 10.0);
	 led0_n = 1;
	 led1_n = 1;
	 led2_n = 0;

		// Setup serial comm for Skywire    


    //  GPA_EN_n (active low) for output pins GPO0_MCU - GPO2_MCU
	  //  GPB_EN_n (active low) for output pins GPO3_MCU, GREEN_LED, RED_LED, BLUE_LED
		//GPA_EN_n  = 0;
		//GPB_EN_n	= 0; 
    
    cell.attach(&Skywire_Rx_interrupt, Serial::RxIrq);
	
		wait_ms(200.0); /// 

	cell_reset = 0;  // BG96 Reset must be held high for a time between 150 and 460 ms.
	wait_ms(200.0);
	cell_reset = 1;
	

	

		// DTR pin must be driven low if unused and will wake up the BG96
		// RTS Pin must be driven low if unused! Pin 5 
		cell_wake = 0; // WAKE PIN ON CAT M1 and DTR on CAT3 LTE 
    cell_rts = 0;
    
    print("Starting Demo\n");
    print("PWP BG96 booting\n");
		    

	
   // Skywire ON_OFF = Pin 20 on Modem and connected to PF4	on EFM32WG
	 // Modem On/Off signal. Assert low for at least 1 second and then release to activate 
	 // start sequence. Drive with an open collector output. Internally pulled up I/O rail 
	 // with pull up. Do not use any external pull ups. Note: If you want modem to turn on
	 // automatically, permanently tie this signal to GND.
	 // the ON-OFF input to the Skywire is driven by the Open Drain of Q2 so the output pin (PF4) 
	 // of the EFM32 needs to be configured as a Push-Pull output to drive the gate of Q2.  You 
	 // did this yesterday when I hooked it up and toggled it for 5 seconds (ON-OFF input = low, EFM32 – PF4 = High).
	 //	 cell_on = 0;
	 // Delay for 5.1 Seconds since using NL_SW_LTE_TSVG 
	 // Wait time is different for each modem, 10s is enough for the LE910
	//   wait_print(6);  // Modem requires >5s pulse
		//   cell_on = 1;


	BG96_PWRKEY = 0; // Pulse for >100 mS
	wait_ms(200.0);  
	BG96_PWRKEY = 1;
	
		wait_print(10);  // Modem requires 4.9 seconds for initialization
		
	  // send "AT" until "OK" is received to ensure modem is on
    int ctr = 0;
    while (1) {
        
      print("count: %d\n", ctr);
			cell.printf("ATE0\r");
			//cell.printf("ATE0\r");
			
        bool ret1 = WaitForResponse("OK");
			  if (ret1){
					  print("AT ok\r\n");
            break;
				}
        wait(1.0);
        ctr++;
        ClearBuffer();
    }
		    wait(1.0);

    
    // Turn off echo
    // Helps with checking responses from Skywire
    //print("Turning off echo\n");
    //UpdateDisplay();

    //cell.printf("ATE0\r");
    //WaitForResponse("ATE0");
    
    wait(2.0);
   
    // Query the modem type from the Skywire and set MODEM
    GetSkywireModel();
    
		wait(2.0); 
    cell.printf("ATE0\r");  // Turn off echo to get correct IMEI
    WaitForResponse("ATE0");
     // read out MEID number to use as unique identifier
    GetMEID();
		print("Unique ID: %d \n", SYSTEM_GetUnique);				// 
		UniqueID = uint64_t (SYSTEM_GetUnique);
		
    print("MEID:%s\n", IMEI);
    
    wait(2.0);

    
    // Check signal quality
    print("Getting CSQ\n");
    //UpdateDisplay();
    cell.printf("AT+CSQ\r");
    WaitForResponse("+CSQ:");
		//WaitForResponse("OK");
		
    wait(1.0);
		    print("Getting Network Status\n");
    
    if (MODEM == NL_SW_LTE_TSVG || MODEM == NL_SW_LTE_TNAG) {
        print("Sending AT+CGREG\n");
        cell.printf("AT+CGREG?\r");
    } else {
        print("Sending AT+CREG\n");
        cell.printf("AT+CREG?\r");
    }
    WaitForResponse("OK");
    
    wait(1.0);
		    // Turn on DNS Response Caching (used on Telit-based Skywires)
    if (MODEM != NL_SW_LTE_GELS3) {
        print("Turning on DNS\nCaching to\nimprove speed\n");
        cell.printf("AT#CACHEDNS=1\r");
        WaitForResponse("OK");
        wait(1.0);      
    }
    
    print("Connecting\nto Network\n");
		
		    // Setup and activate PDP context based on Skywire model
    if (MODEM == NL_SW_LTE_TSVG) {
        // The last parameter in AT#SCFG sets the timeout if transmit buffer is not full
        // Time is in hundreds of ms: so, a value of 5 = 500 ms
        print("Context config\n");
        //UpdateDisplay();
        cell.printf("AT#SCFG=3,3,300,90,600,5\r");
        WaitForResponse("OK");

        wait(1.0);
        
        // Get APN automatically for NL-SW-LTE-TSVG Skywire (Verizon)
        print("Getting APN\n");

        while(!AutoAPN())
        {
            print("Failed to\nget APN\nRetrying...");
            wait(1.0);
        }
        wait(1.0);

        
        // Verify APN
        cell.printf("AT+CGDCONT?\r");
        WaitForResponse("OK");
        wait(4.0);
        // Manually set APN
        //display.printf("Configure APN:\n%s\n\n", APN);
        //UpdateDisplay();
        //cell.printf("AT+CGDCONT=3,\"IP\",\"%s\"\r", APN);
        //WaitForResponse("OK");
        //wait(1.0);
        //ClearDisplay();   
        cell.printf("AT#SGACT=3,1\r");
        
    } else if (MODEM == NL_SW_LTE_TNAG || MODEM == NL_SW_LTE_TEUG || MODEM == NL_SW_HSPAP || MODEM == NL_SW_HSPAPG || MODEM == NL_SW_HSPAPE || MODEM == NL_SW_GPRS) {
        print("Context config\n");
        //UpdateDisplay();
        cell.printf("AT#SCFG=1,1,300,90,600,5\r");
        WaitForResponse("OK");
        wait(1.0);
        //ClearDisplay();
        
        print("Configure APN:\n%s\n", APN);
        //UpdateDisplay();
        cell.printf("AT+CGDCONT=1,\"IP\",\"%s\"\r", APN);
        WaitForResponse("OK");
        wait(1.0);
        //ClearDisplay();
        cell.printf("AT#SGACT=1,1\r");
        WaitForResponse("#SGACT");
    } else {
        // The last parameter in AT#SCFG sets the timeout if transmit buffer is not full
        // Time is in hundreds of ms: so, a value of 5 = 500 ms
        cell.printf("AT#SCFG=1,1,300,90,600,5\r");
        WaitForResponse("OK");
        
        wait(1.0);
        //ClearDisplay();
        
        cell.printf("AT#SGACT=1,1\r");
        WaitForResponse("#SGACT");
        
        wait(1.0);
        //ClearDisplay();
    }
    WaitForResponse("OK");
    wait(1.0);
			
    
 //   while(1) {
 //       sleep();
 //   }
		    while(1) {
       //ClearDisplay();
        /*
        // Read temperature and humidity
        busChecked = false;
        rhtSensor.measure(si7021, respondedCallback);
        while(busChecked == false) sleep();
        
        if(rhtSensor.get_active()) {
            print("Temp: %d.%03d C\n", rhtSensor.get_temperature()/1000, rhtSensor.get_temperature()%1000);
            print("Hum:  %d.%03d %%\n", rhtSensor.get_humidity()/1000, rhtSensor.get_humidity()%1000);
            //UpdateDisplay();
        }
        else {  // sensor not available, skip sending data to dweet
            print("No sensor found\n");
            //UpdateDisplay();
            wait(1.0);
            continue;
        }
        */
        // get RSSI measurement from modem
        rssi = GetRSSI();
        print("RSSI: %d\n", rssi);
        wait(2.0);

        
        // connect to dweet.io
        if (MODEM == NL_SW_LTE_TSVG) {
            cell.printf("AT#SD=3,0,80,\"dweet.io\"\r");
            WaitForResponse("CONNECT");
        }
        else {
            cell.printf("AT#SD=1,0,80,\"dweet.io\"\r");
            WaitForResponse("CONNECT");
        }

        wait(1.0);
       
        // Report the sensor data to dweet.io
        //cell.printf("POST /dweet/for/%s?temp=%d&hum=%d&rssi=%d HTTP/1.0\r\n\r\n", IMEI, rhtSensor.get_temperature(), rhtSensor.get_humidity(), rssi);
        //WaitForResponse("NO CARRIER");
        //cell.printf("POST /dweet/for/%s?&rssi=%d HTTP/1.0\r\n\r\n", IMEI, rssi);
				cell.printf("POST /dweet/for/%s?&UniqueID=%d&rssi=%d HTTP/1.0\r\n\r\n\r\n", IMEI, UniqueID, rssi);
				
				WaitForResponse("NO CARRIER");
        
        ClearBuffer();
        wait(2.0);
								
    }

}

#endif
