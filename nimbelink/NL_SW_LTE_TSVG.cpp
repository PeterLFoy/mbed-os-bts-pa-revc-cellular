#include "mbed.h"
#include "pinmap.h"         // pinmap needed for hardware flow control
#include "InitDevice.h"
#include "em_system.h"
#include "NL_SW_LTE_TSVG.h"


int NL_SW_LTE_TSVG_main() {
	    	
	 int rssi;
	//enter_DefaultMode_from_RESET();
  //toggleTicker.attach(&ledToggler, 0.2f);	

	 watchdog.attach(&watchdog_wakeup, 10.0);
	 led0 = 1;
	 led1 = 1;
	 led2 = 0;
	
		// Setup serial comm for Skywire    


    //  GPA_EN_n (active low) for output pins GPO0_MCU - GPO2_MCU
	  //  GPB_EN_n (active low) for output pins GPO3_MCU, GREEN_LED, RED_LED, BLUE_LED
		GPA_EN_n  = 0;
		GPB_EN_n	= 0; 
    
    cell.attach(&Skywire_Rx_interrupt, Serial::RxIrq);

		// DTR pin must be driven low if unused!
		// skywire_dtr = 0; // Pin 9 (DTR) of Skywire is tied to GND!
		// RTS Pin must be driven low if unused! Pin 5 
    cell_rts = 0;
    
    debug_pc.printf("Starting Demo\n");
    debug_pc.printf("Skywire booting\n");
		    
	// Skywire Reset_nIN = Pin 9 on Modem and connected to PE14 on EFM32WG
  // Controls HW_SHUTDOWN input on LE910, tie low for 200ms and release to activate. 
	// Internally pulled up to Vcc. Drive with open collector output. Assert only in an 
	// emerency as the module will not gracefully exit the cellular network when asserted
	cell_reset = 1;
	wait_ms(201.0);
	cell_reset = 0;
	
   // Skywire ON_OFF = Pin 20 on Modem and connected to PF4	on EFM32WG
	 // Modem On/Off signal. Assert low for at least 1 second and then release to activate 
	 // start sequence. Drive with an open collector output. Internally pulled up I/O rail 
	 // with pull up. Do not use any external pull ups. Note: If you want modem to turn on
	 // automatically, permanently tie this signal to GND.
	 // the ON-OFF input to the Skywire is driven by the Open Drain of Q2 so the output pin (PF4) 
	 // of the EFM32 needs to be configured as a Push-Pull output to drive the gate of Q2.  You 
	 // did this yesterday when I hooked it up and toggled it for 5 seconds (ON-OFF input = low, EFM32 – PF4 = High).
	 cell_on = 1;
	 // Delay for 5.1 Seconds since using NL_SW_LTE_TSVG 
	 // Wait time is different for each modem, 10s is enough for the LE910
   wait_print(6);  // Modem requires >5s pulse
   cell_on = 0;

		wait_print(16);  // Modem requires 15 seconds for initialization
		
	  // send "AT" until "OK" is received to ensure modem is on
    int ctr = 0;
    while (1) {
        
      debug_pc.printf("count: %d\n", ctr);
			cell.printf("AT\r");
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
		debug_pc.printf("Unique ID: %d \n", SYSTEM_GetUnique);				// 
		UniqueID = uint64_t (SYSTEM_GetUnique);
		
    debug_pc.printf("MEID:%s\n", IMEI);
    
    wait(2.0);

    
    // Check signal quality
    debug_pc.printf("Getting CSQ\n");
    //UpdateDisplay();
    cell.printf("AT+CSQ\r");
    WaitForResponse("+CSQ:");
		//WaitForResponse("OK");
		
    wait(1.0);
		    debug_pc.printf("Getting Network Status\n");
    
    if (MODEM == NL_SW_LTE_TSVG || MODEM == NL_SW_LTE_TNAG) {
        debug_pc.printf("Sending AT+CGREG\n");
        cell.printf("AT+CGREG?\r");
    } else {
        debug_pc.printf("Sending AT+CREG\n");
        cell.printf("AT+CREG?\r");
    }
    WaitForResponse("OK");
    
    wait(1.0);
		    // Turn on DNS Response Caching (used on Telit-based Skywires)
    if (MODEM != NL_SW_LTE_GELS3) {
        debug_pc.printf("Turning on DNS\nCaching to\nimprove speed\n");
        cell.printf("AT#CACHEDNS=1\r");
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
        cell.printf("AT#SCFG=3,3,300,90,600,5\r");
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
        debug_pc.printf("Context config\n");
        //UpdateDisplay();
        cell.printf("AT#SCFG=1,1,300,90,600,5\r");
        WaitForResponse("OK");
        wait(1.0);
        //ClearDisplay();
        
        debug_pc.printf("Configure APN:\n%s\n", APN);
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
				return 1; 
}