#include "mbed.h"
#include "pinmap.h"         // pinmap needed for hardware flow control
#include "InitDevice.h"
#include "em_system.h"

#define CMD_TIMEOUT 10000    // modem response timeout in milliseconds

DigitalOut myled0(LED0);
DigitalOut myled1(LED1);
DigitalOut myled2(LED2);
LowPowerTicker toggleTicker;
Serial debug_pc(PC2, PC3); // Serial connection to PC
Serial skywire(PE10, PE11);        // Serial comms to Skywire
Serial skygps(PC0, PC1);            // Serial comms to Skywire

// Skywire configuration
//DigitalOut GPI_EN_n(PA2);         // GPI_EN_n (active low)
//DigitalOut GPO_EN_n(PB12);        // GPO_EN_n (active low)
//DigitalOut skywire_power(PA4);	  // ?
DigitalOut skywire_reset(PF5);    // Skywire Enable  U5 to Reset Pchannel FET - When High Cell is Reset
DigitalOut skywire_rts(PE13);		// CTS and RTS are connected
DigitalOut skywire_on(PF4);    // Turn on skywire modem
		
// Digital I/O on board
DigitalIn button0(BTN0);
DigitalIn button1(BTN1);


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
    myled0 = !myled0;
//		myled1 = !myled1;
//		myled2 = !myled2;
}

void watchdog_wakeup() {
    wakeup = true;
}

// Function to blink LED0 for debugging
void blink_leds(int num)
{
    for (int i = 0; i < num; i++) {
        myled1 = 0;
        wait(0.5);
        myled1 = 1;
        wait(0.5);
    }
}


// Interrupt for the Skywire
void Skywire_Rx_interrupt()
{
// Loop just in case more than one character is in UART's receive FIFO buffer
// Stop if buffer full
    while ((skywire.readable()) && (((rx_in + 1) % buffer_size) != rx_out)) {
        rx_buffer[rx_in] = skywire.getc();
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
    debug_pc.printf("Waiting for:\n%s\n", response);
    debug_pc.printf("Received:\n");
    //UpdateDisplay();
    t.reset();
    t.start();
    do {
        t.reset();
        while(!DataAvailable())
        {
            if (t.read_ms() > CMD_TIMEOUT)
            {
                debug_pc.printf("Command timeout\n");
                //UpdateDisplay();
                t.stop();
                return false;
            }
        }
        read_line();
        // don't write to the LCD if the line is >= 100 characters
        if(strlen(rx_line) < 1000 && strlen(rx_line) > 1)
        {
            debug_pc.printf("%s\n", rx_line);
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
        skywire.printf(message);
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
        debug_pc.printf("Waiting for: %s\nReceived: %s\n", "OK", rx_line);
        //UpdateDisplay();
        if (!strncmp(rx_line, "GE910-QUAD-V3", 13)) {
            MODEM = NL_SW_GPRS;
            debug_pc.printf("Mdm: NL-SW-GPRS\n");
            //UpdateDisplay();
            //ret_val = 1;
            return 1;
        }
        if (!strncmp(rx_line, "CE910-DUAL", 10)) {
            MODEM = NL_SW_1xRTT_V;
            debug_pc.printf("Mdm: NL-SW-1xRTT\n");
            //UpdateDisplay();
            //ret_val = 1;
            return 1;
        }
        if (!strncmp(rx_line, "DE910-DUAL", 10)) {
            MODEM = NL_SW_EVDO_V;
            debug_pc.printf("Mdm: NL-SW-EVDO\n");
            //UpdateDisplay();
            //ret_val = 1;
            return 1;
        }
        if (!strncmp(rx_line, "HE910-NAD", 9)) {
            MODEM = NL_SW_HSPAP;
            debug_pc.printf("Mdm: NL-SW-HSPAP\n");
            //UpdateDisplay();
            //ret_val = 1;
            return 1;
        }
        if (!strncmp(rx_line, "HE910-D", 7)) {
            MODEM = NL_SW_HSPAPG;
            debug_pc.printf("Mdm: NL-SW-HSPAPG\n");
            //UpdateDisplay();
            //ret_val = 1;
            return 1;
        }
        if (!strncmp(rx_line, "LE910-SVG", 9)) {
            MODEM = NL_SW_LTE_TSVG;
            debug_pc.printf("Mdm: NL-SW-LTE-TSVG\n");
            //UpdateDisplay();
            //ret_val = 1;
            return 1;
        }
        if (!strncmp(rx_line, "LE910-NAG", 9)) {
            MODEM = NL_SW_LTE_TNAG;
            debug_pc.printf("Mdm: NL-SW-LTE-TNAG\n");
            //UpdateDisplay();
            //ret_val = 1;
            return 1;
        }
        if (!strncmp(rx_line, "HL7588", 6)) {
            MODEM = NL_SW_LTE_S7588;
            debug_pc.printf("Mdm: NL-SW-LTE-S7588\n");
            //UpdateDisplay();
            //ret_val = 1;
            return 1;
        }
        if (!strncmp(rx_line, "ELS31-V", 7)) {
            MODEM = NL_SW_LTE_GELS3;
            debug_pc.printf("Mdm: NL-SW-LTE-GELS3\n");
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
    debug_pc.printf("Sending AT+CGMM\n");
    //UpdateDisplay();
    skywire.printf("AT+CGMM\r");
    if (GetGMMResponse()) {
        return 0;
    }
    // Otherwise, we have an error (or no modem), so sit here and blink
    else {
        debug_pc.printf("ERROR: Unable to detect modem\n");
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
            skywire.printf("AT#MEID?\r");
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
						debug_pc.printf("AT+CGSN\n");
						skywire.printf("AT+CGSN\r");
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
				    debug_pc.printf(IMEI, "%s", "nimbelinkisawesome");
            break;
    }
    return 0;
}
// Reads the signal strength received by the modem
int GetRSSI()
{
    int rssi;
    skywire.printf("AT+CSQ\r");
    WaitForResponse("+CSQ:");
    sscanf(rx_line, "+CSQ: %i,%*i", &rssi);
	  debug_pc.printf("RSSI: %s\n", rssi);
    ClearBuffer();
    return rssi;
}

// Function to print the delay time
void wait_print(int time)
{
    for (int i = time; i >= 0; i--)
    {
        //ClearDisplay();
        debug_pc.printf("Countdown: %d\n", i);
        //UpdateDisplay();
        wait(1.0);
    }
    debug_pc.printf("Delay finished!\n");
    //UpdateDisplay();
    return;
}


// Automatically sets APN based on SIM card type
bool AutoAPN()
{
    bool succeeded = false;

    skywire.printf("AT#OTAUIDM=0\r");
    WaitForResponse("OK");
    do {
        while(!DataAvailable()) ;
        read_line();

        // 918 is the response code for successfully receiving the APN
        if (!strcmp(rx_line, "#OTAEV: #918"))
        {
            succeeded = true;
        }
        debug_pc.printf("%s\n", rx_line);
        //UpdateDisplay();
    } while (strcmp(rx_line, "#OTAEV: #DREL"));
    
    return succeeded;
}

int main() {
	    	
	 int rssi;
	//enter_DefaultMode_from_RESET();

	HFXO_enter_DefaultMode_from_RESET();
	CMU_enter_DefaultMode_from_RESET();
	USART0_enter_DefaultMode_from_RESET(); 
	USART1_enter_DefaultMode_from_RESET();
	USART2_enter_DefaultMode_from_RESET();
	PORTIO_enter_DefaultMode_from_RESET();
  GPIO_PinModeSet((GPIO_Port_TypeDef) AF_USART1_TX_PORT(gpioPortC), AF_USART1_TX_PIN (USART0_TX_PIN), gpioModePushPull, 1);
  GPIO_PinModeSet((GPIO_Port_TypeDef) AF_USART1_RX_PORT(gpioPortC), AF_USART1_RX_PIN (USART0_RX_PIN), gpioModeInput, 0);

  GPIO_PinModeSet((GPIO_Port_TypeDef)AF_USART0_TX_PORT(gpioPortE), AF_USART0_TX_PIN(USART1_TX_PIN), gpioModePushPull, 1);
  GPIO_PinModeSet((GPIO_Port_TypeDef)AF_USART0_RX_PORT(gpioPortE), AF_USART0_RX_PIN(USART1_RX_PIN), gpioModeInput, 0);

	
	 //toggleTicker.attach(&ledToggler, 0.2f);	

	 watchdog.attach(&watchdog_wakeup, 10.0);
	 myled0 = 1;
	 myled1 = 1;
	 myled2 = 0;
	
		// Setup serial comm for Skywire    


    //  GPI_EN_n (active low) for input pins PA3-PA6
	  //  GPO_EN_n (active low) for out pins PA9,10,15 and PB11
    //GPI_EN_n = 0;
    //GPO_EN_n = 0;
	
    debug_pc.printf("System Clock: %d Hz\n", SystemCoreClock);				// 48_000_000
/*debug_pc.printf("Unique ID: %d \n", SYSTEM_GetUnique);				// 
		debug_pc.printf("HF Peripheral Clock: %d Hz\n", cmuClock_HFPER); // 295_232 Hz
		debug_pc.printf("USART0 rx/tx Clock:  %d Hz\n", cmuClock_USART0); // 262_656 Hz
		debug_pc.printf("USART1 rx/tx Clock:  %d Hz\n", cmuClock_USART1); // 266_752 Hz
		debug_pc.printf("USART2 rx/tx Clock:  %d Hz\n", cmuClock_USART2); // 270_848 Hz
		debug_pc.printf("USART2 RX Pin: %d \n", USART0_RX_PIN);
		debug_pc.printf("USART2 TX Pin: %d \n", USART0_TX_PIN);		
*/
    //debug_pc.printf("Prov Flag: %d\n", prov_flag);
    //UpdateDisplay();
    
    skywire.attach(&Skywire_Rx_interrupt, Serial::RxIrq);

		// DTR pin must be driven low if unused!
		// skywire_dtr = 0; // Pin 9 (DTR) of Skywire is tied to GND!
		// RTS Pin must be driven low if unused! Pin 5 
    skywire_rts = 0;
    
    debug_pc.printf("Starting Demo\n");
    debug_pc.printf("Skywire booting\n");
		    
	// Skywire Reset_nIN = Pin 9 on Modem and connected to PE14 on EFM32WG
  // Controls HW_SHUTDOWN input on LE910, tie low for 200ms and release to activate. 
	// Internally pulled up to Vcc. Drive with open collector output. Assert only in an 
	// emerency as the module will not gracefully exit the cellular network when asserted
	//skywire_reset.output();
	//skywire_reset.mode(OpenDrain);
	skywire_reset = 1;
	wait_ms(201.0);
	skywire_reset = 0;
	
   // Skywire ON_OFF = Pin 20 on Modem and connected to PF4	on EFM32WG
	 // Modem On/Off signal. Assert low for at least 1 second and then release to activate 
	 // start sequence. Drive with an open collector output. Internally pulled up I/O rail 
	 // with pull up. Do not use any external pull ups. Note: If you want modem to turn on
	 // automatically, permanently tie this signal to GND.
	 // the ON-OFF input to the Skywire is driven by the Open Drain of Q2 so the output pin (PF4) 
	 // of the EFM32 needs to be configured as a Push-Pull output to drive the gate of Q2.  You 
	 // did this yesterday when I hooked it up and toggled it for 5 seconds (ON-OFF input = low, EFM32 – PF4 = High).
	 skywire_on = 1;
	 // Delay for 5.1 Seconds since using NL_SW_LTE_TSVG 
	 // Wait time is different for each modem, 10s is enough for the LE910
   wait_print(6);  // Modem requires >5s pulse
   skywire_on = 0;

		wait_print(16);  // Modem requires 15 seconds for initialization
		
	  // send "AT" until "OK" is received to ensure modem is on
    int ctr = 0;
    while (1) {
        
      debug_pc.printf("count: %d\n", ctr);
			skywire.printf("AT\r");
        bool ret1 = WaitForResponse("OK");
        if (ret1)
            break;
        wait(1.0);
        ctr++;
        ClearBuffer();
    }
		    wait(1.0);

    
    // Turn off echo
    // Helps with checking responses from Skywire
    //debug_pc.printf("Turning off echo\n");
    //UpdateDisplay();

    //skywire.printf("ATE0\r");
    //WaitForResponse("ATE0");
    
    wait(2.0);
   
    // Query the modem type from the Skywire and set MODEM
    GetSkywireModel();
    
		wait(2.0); 
    skywire.printf("ATE0\r");  // Turn off echo to get correct IMEI
    WaitForResponse("ATE0");
     // read out MEID number to use as unique identifier
    GetMEID();
		debug_pc.printf("Unique ID: %d \n", SYSTEM_GetUnique);				// 
		UniqueID = uint64_t (SYSTEM_GetUnique);
		
    debug_pc.printf("MEID:%s\n", IMEI);
    
    wait(2.0);

    
    // Check signal quality
    debug_pc.printf("Getting CSQ\n");
    //UpdateDisplay();
    skywire.printf("AT+CSQ\r");
    WaitForResponse("+CSQ:");
		//WaitForResponse("OK");
		
    wait(1.0);
		    debug_pc.printf("Getting Network Status\n");
    
    if (MODEM == NL_SW_LTE_TSVG || MODEM == NL_SW_LTE_TNAG) {
        debug_pc.printf("Sending AT+CGREG\n");
        skywire.printf("AT+CGREG?\r");
    } else {
        debug_pc.printf("Sending AT+CREG\n");
        skywire.printf("AT+CREG?\r");
    }
    WaitForResponse("OK");
    
    wait(1.0);
		    // Turn on DNS Response Caching (used on Telit-based Skywires)
    if (MODEM != NL_SW_LTE_GELS3) {
        debug_pc.printf("Turning on DNS\nCaching to\nimprove speed\n");
        skywire.printf("AT#CACHEDNS=1\r");
        WaitForResponse("OK");
        wait(1.0);      
    }
    
    debug_pc.printf("Connecting\nto Network\n");
		
		    // Setup and activate PDP context based on Skywire model
    if (MODEM == NL_SW_LTE_TSVG) {
        // The last parameter in AT#SCFG sets the timeout if transmit buffer is not full
        // Time is in hundreds of ms: so, a value of 5 = 500 ms
        debug_pc.printf("Context config\n");
        //UpdateDisplay();
        skywire.printf("AT#SCFG=3,3,300,90,600,5\r");
        WaitForResponse("OK");

        wait(1.0);
        
        // Get APN automatically for NL-SW-LTE-TSVG Skywire (Verizon)
        debug_pc.printf("Getting APN\n");

        while(!AutoAPN())
        {
            debug_pc.printf("Failed to\nget APN\nRetrying...");
            wait(1.0);
        }
        wait(1.0);

        
        // Verify APN
        skywire.printf("AT+CGDCONT?\r");
        WaitForResponse("OK");
        wait(4.0);
        // Manually set APN
        //display.printf("Configure APN:\n%s\n\n", APN);
        //UpdateDisplay();
        //skywire.printf("AT+CGDCONT=3,\"IP\",\"%s\"\r", APN);
        //WaitForResponse("OK");
        //wait(1.0);
        //ClearDisplay();   
        skywire.printf("AT#SGACT=3,1\r");
        
    } else if (MODEM == NL_SW_LTE_TNAG || MODEM == NL_SW_LTE_TEUG || MODEM == NL_SW_HSPAP || MODEM == NL_SW_HSPAPG || MODEM == NL_SW_HSPAPE || MODEM == NL_SW_GPRS) {
        debug_pc.printf("Context config\n");
        //UpdateDisplay();
        skywire.printf("AT#SCFG=1,1,300,90,600,5\r");
        WaitForResponse("OK");
        wait(1.0);
        //ClearDisplay();
        
        debug_pc.printf("Configure APN:\n%s\n", APN);
        //UpdateDisplay();
        skywire.printf("AT+CGDCONT=1,\"IP\",\"%s\"\r", APN);
        WaitForResponse("OK");
        wait(1.0);
        //ClearDisplay();
        skywire.printf("AT#SGACT=1,1\r");
        WaitForResponse("#SGACT");
    } else {
        // The last parameter in AT#SCFG sets the timeout if transmit buffer is not full
        // Time is in hundreds of ms: so, a value of 5 = 500 ms
        skywire.printf("AT#SCFG=1,1,300,90,600,5\r");
        WaitForResponse("OK");
        
        wait(1.0);
        //ClearDisplay();
        
        skywire.printf("AT#SGACT=1,1\r");
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
            debug_pc.printf("Temp: %d.%03d C\n", rhtSensor.get_temperature()/1000, rhtSensor.get_temperature()%1000);
            debug_pc.printf("Hum:  %d.%03d %%\n", rhtSensor.get_humidity()/1000, rhtSensor.get_humidity()%1000);
            //UpdateDisplay();
        }
        else {  // sensor not available, skip sending data to dweet
            debug_pc.printf("No sensor found\n");
            //UpdateDisplay();
            wait(1.0);
            continue;
        }
        */
        // get RSSI measurement from modem
        rssi = GetRSSI();
        debug_pc.printf("RSSI: %d\n", rssi);
        wait(2.0);

        
        // connect to dweet.io
        if (MODEM == NL_SW_LTE_TSVG) {
            skywire.printf("AT#SD=3,0,80,\"dweet.io\"\r");
            WaitForResponse("CONNECT");
        }
        else {
            skywire.printf("AT#SD=1,0,80,\"dweet.io\"\r");
            WaitForResponse("CONNECT");
        }

        wait(1.0);
       
        // Report the sensor data to dweet.io
        //skywire.printf("POST /dweet/for/%s?temp=%d&hum=%d&rssi=%d HTTP/1.0\r\n\r\n", IMEI, rhtSensor.get_temperature(), rhtSensor.get_humidity(), rssi);
        //WaitForResponse("NO CARRIER");
        //skywire.printf("POST /dweet/for/%s?&rssi=%d HTTP/1.0\r\n\r\n", IMEI, rssi);
				skywire.printf("POST /dweet/for/%s?&UniqueID=%d&rssi=%d HTTP/1.0\r\n\r\n\r\n", IMEI, UniqueID, rssi);
				
				WaitForResponse("NO CARRIER");
        
        ClearBuffer();
        wait(2.0);
    }
}