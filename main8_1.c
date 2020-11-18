/*Pierre Borderie
  40312710
  18/11/2020
  Embedded Systems				   
  ELE10113
  Coursework
  V8.1			 																					
*/

#include <reg51.h>
#define ADC_DATA P2

/********************Definitions**********************/
////ADC Pins////
sbit ADC_READ=P1^0;
sbit ADC_WRITE=P1^1;
sbit ADC_INTR=P3^7;
////Valve and Pump drives////
sbit control_A=P0^0;
sbit control_B=P0^1;
sbit control_C=P0^2;
sbit drive_led1=P0^3;
sbit drive_led2=P0^4;
sbit drive_led3=P0^5;
sbit drive_led4=P0^6;
sbit v1_on=P1^2;
sbit v2_on=P1^3;
sbit v3_on=P1^4;
sbit v4_on=P1^5;
sbit v5_on=P1^6;
sbit v6_on=P1^7;
sbit v7_on=P0^7;
////Sesnsors////
sbit sens_1=P3^3;
sbit sens_2=P3^4;	//active low (have all push buttons pressed at start)
sbit sens_3=P3^5;
sbit sens_4=P3^6;

/************************FUNCTIONS*********************/
///Initialise////
void SerialInitialize(void);
void msec_delay (unsigned int);   
////Valves and Pump////
void drive_seq(void);
void rst_drive(void);
void valve_check(void);
void auto_valve(int);
void pressure_check(void);
////Flow Meter////
void external(void);
void flow_rate(void);
void flow_delay(int);
unsigned int adcConvert();
////Serial Communication////
void uart_msg(unsigned char *c);	//can send message by serial
void uart_tx(unsigned char);
void uart_nl(void);  
////Main Routines////
void system_run(void);
void filter_pump(void);
void tap_amount(void);
void backflush(void);

/************************Variables*************************/
volatile int count=0;  //volatile to work with interrupt
float flowrate, tank_factor=60000, minutes, req_delay_flow=0;
char flow;
int tank_amount1=500, tank_amount2=5000, tank_amount3=10000, end_process=0, end_tank_filter_fill=0, fill_flush=0, req_delay, valve_i;
int valve[7]={1, 1, 1, 1, 1, 1,1};	//change any of these values to a 0 to demonstrate valve closing safety measure
unsigned int adcData=0x00;


/*************************Main Program*********************/
int main (void){
	IE=0x90;  //enable serial interrupt
	IP=0x01;  // give external priority	(essential for flow meter to work)
	P1 = 0x00;
	P2 = 0xFF;
	P3 = 0xFF;
	P0 = 0x00;   
    SerialInitialize();  
    uart_msg("Welcome to Bacteria Concentration System");
	uart_nl(); //new line
	end_process=1;
	IT0=1; //sets external interrupt to pulse triggger

	while (1){
		if (end_process==1) {	  //return here after filter or backflush is complete
			uart_msg("Main Menu");
			uart_nl();
			uart_msg("Press 1 to start water stage, 2 to start backflush system");
			uart_nl();
			end_process=0;
			IE=0x91;	  //back to serial interrupt 
		}
		else {} //empty
		}          
}

/////////////////Serial Interrupt///////////////////////		
void serial_ISR (void) interrupt 4{  //serial interrupt	
	char chr;  			    
    if(RI==1)			  //if user types letter
	{
        chr = SBUF;		  //letter typed moved into variable for storage
        RI = 0;
		    }	                   
    switch(chr)				//based on what user typed, certain actions will be performed
    {
     case '1':						 //first stage
	 if (end_tank_filter_fill==0){
	 fill_flush=1; //1=fill  
	 system_run ();				   
	 }
	 else {
	 end_process=1;
	 IE=0x00;
	 uart_msg("Filter has sample");	   //cannot run filter stage if filter has been run but no backflush
	 uart_nl();	 
	 } 
	 break;

	 case '2':						//backflush stage
	 if (end_tank_filter_fill==1){
	 fill_flush=2; 
	 system_run();
	 }
	 else {
	 uart_msg("No sample yet");		 //cannot run in filter stage has not already run
	 uart_nl();
	 end_process=1;
	 IE=0x00;
	 } 
	 break;
	 default:     break;                        //do nothing
    }
}
//////////////////////////System Run Function////////////////
void system_run(void){
	IE=0x00;
	valve_check();
	if (fill_flush==1){
	tap_amount();
	}
	else{
	if (sens_2==0){
		uart_msg("Please put solution in tank");
		uart_nl();
		msec_delay(1000);
		end_process=1;
	}
	else{
	backflush();
	}		
	}
}

///////////////////Backflush///////////////////////////////////
void backflush(void){
	if (end_tank_filter_fill==1){	  //has first stage been run yet
	auto_valve(2);
	auto_valve(4);
	auto_valve(6);
	uart_msg("PUMP ON");
	uart_nl();
	while (sens_2==1){
     auto_valve(8);
	 pressure_check();
	 }
	uart_msg("Backflush Complete");
	uart_nl();
	auto_valve(2);
	auto_valve(4);
	auto_valve(6);
	}
	if (sens_4==0){
		uart_msg("No concentrate detected, run backflush again");
		uart_nl();
		end_tank_filter_fill=1;
		fill_flush=2;			  //ensures backflush can be run again
		end_process=1;
		}
	else{
	end_process=1;
	end_tank_filter_fill=0;
	}

}
///////////////////////////User Defines Amount of Tap Water/////////////////
void tap_amount(void){
	float  p_flow, q_flow,i=0;
	char flow_holder;
		uart_msg("In litres, select 1 for 0.5, 2 for 5 or 3 for 10"); 
		uart_nl();                                          
   		while (RI==0);			//wait for input
		if (RI==1)
			{
        	flow_holder = SBUF;
        	RI = 0;
			}
			uart_msg("Open tap");
			uart_nl();
			auto_valve(7);
			IE=0x81; //enables ext interrupt0 no serial 
			msec_delay(1000);
			IE=0x00;	//disables ext interrupt, leaves serial off
			flow_rate();
			switch (flow_holder){
			case '1':
					flow_delay(1);							
					break;
			case '2':
					flow_delay(2);
					break;
			case '3':
					flow_delay(3);
					break;
											   
         	default:
					break;
			}
																
		for (p_flow = 0; p_flow < req_delay_flow; p_flow = p_flow +1);
        for (q_flow = 0; q_flow < 1275; q_flow = q_flow +1);		
		count=0;
		req_delay_flow=0;
		uart_msg("Close tap"); //tells user to close tap, not essential as valves stop flow but good for safety.
	    uart_nl();
		auto_valve(7); 
		filter_pump();	//move to next stage
}
//////////////////////Pump Water to filter and monitor pressure///////////////////
void filter_pump(void){
	if (sens_1==1&&sens_3==1) {	 //check water is in tank
	}
	else {
		uart_msg("Error no water in tank");
		uart_nl();
		tap_amount();
		}
	uart_nl();
	auto_valve(3);
	auto_valve(1);
	//valve_check();
	uart_msg("PUMP ON");  //for safety user is told when pump is on
	uart_nl();
	while (sens_1==1){
	auto_valve(8);
	pressure_check();
	}
	if (sens_3==1&&sens_1==0){ //if high level sensor detects water but low level does not
	 uart_msg("Warning: Tank Sensors Conflict");  
	 uart_nl();
	}
	uart_msg("Finished, ready for backflush");
	uart_nl();
	auto_valve(3);
	auto_valve(1);
	end_process=1;
	end_tank_filter_fill=1;	 //return to main menu
}
/////////////Pressure Check////////////////////////
void pressure_check(void){
	adcConvert();	 //0.66bar max=4.71v 5v/255 steps=19.61mv = 1 step 4.17/19.61mv=212 steps max
	if (adcData>220){
		uart_msg("Excess pressure, valve open until pressure drops");
		uart_nl();
		auto_valve(5);
		msec_delay(1000);
		while (adcData>190){ //190 so that pressure is not released then immediatly too high again
		adcConvert();
		}
		auto_valve(5);
		uart_msg("Pressure low, pump on.");
		uart_nl();
		}
	else {}
}
////////////ADC Function///////////////////////////
unsigned int adcConvert(){	 //fetches adc data
	ADC_INTR = 1;
	ADC_WRITE = 1;
	ADC_WRITE = 0;
	msec_delay(100);
	ADC_WRITE = 1;
	ADC_READ = 1;
	while(ADC_INTR==1);
	msec_delay(50);
	ADC_READ = 0;
	adcData = ADC_DATA;
	return(adcData);
 }
//////////////////Calculates Flow rate//////////////////////////////////
void flow_rate(void){
	flowrate=count*0.704;	 //counted pules multipled by 0.704ml/pulse		 //for count=300
	flowrate=flowrate*60;  //ml/min										 flowrate=12672
	}
////////////////Calculates Time for tap to be open based on flow rate/////////////
void flow_delay(int flow_num){
	if (flow_num==1){
		 minutes=tank_amount1/flowrate; 	 //tank_amount1==500		   minutes=0.0394570
		 req_delay_flow=minutes*tank_factor;  //tank_factor=60,000(minutes*60*1000 to give millisecond delay)	   req_delay_flow=2367.47 (2 seconds)
	}
	else if(flow_num==2){
		minutes=tank_amount2/flowrate; 			//tank_amount2==5000		req_delay_flow=23674.24 (23 seconds)
		req_delay_flow=minutes*tank_factor;
		}
	else if(flow_num==3){
		minutes=tank_amount3/flowrate; 			//tank_amount3==10000	   	req_delay_flow=47348.48 (47 seconds)
		req_delay_flow=minutes*tank_factor;
		}
	else{
	req_delay_flow=0;	//for simulation purposes you can skip time delay
	}
}

/////////////Flow Rate External Interrupt////////////////////////////////////
void external(void) interrupt 0{	 //k factor=1420 therefore 0.704ml per pulse
count++;
}
/////////Drive Pump and Valves/////////////////////
void drive_seq(void)   //full drive for max. torque
{ int i;
	for (i=1; i<10; i++){
	drive_led1=0;
	drive_led2=0;
	drive_led3=1;
	drive_led4=1;
    msec_delay(50);
	drive_led1=0;
	drive_led2=1;
	drive_led3=1;
	drive_led4=0;
    msec_delay(50);
	drive_led1=1;
	drive_led2=1;
	drive_led3=0;
	drive_led4=0;
    msec_delay(50);
	drive_led1=1;
	drive_led2=0;
	drive_led3=0;
	drive_led4=1;
    msec_delay(50);
	}
}

/////////////Initialize Serial Port////////////////////////////
void SerialInitialize(void){
    TMOD = 0x20;                   //Timer 1 In Mode 2 baud rate determined by overflow
    SCON = 0x50;                   //Serial Mode 1, REN Enabled
    TH1 = 0xFD;                    //Gives baud rate of 9600
    TR1 = 1;                              
}
////////////////////Transmits serial data////////////////////////
void uart_tx(unsigned char serialdata){
    SBUF = serialdata;                        //load data to serial buffer
    while(TI == 0);                           //wait until transmission finished
    TI = 0;                                   //clear transmission flag
}
////////////////////NEW LINE SERIAL//////////////////////////////
void uart_nl(void){	  	 //sepcific instance of uart_tx
	SBUF=0x0d;		  //0x0d is new line command
	while(TI==0);
	TI=0;
}
///////////Message for Serial///////////////////////////////
void uart_msg(unsigned char *c){	  //use pointer to send message by serial
	while(*c != 0)					   //until done send next character
    	{
         uart_tx(*c++);
         }
}
///////////////////Reset Drive LEDS so motor doesn't overheat///////////////////
void rst_drive(void){
	 drive_led1=0;
	 drive_led2=0;
	 drive_led3=0;
	 drive_led4=0;
	 }
//////////////////////Valve Check///////////////////////////
void valve_check(void){
	valve_i=0;
	for (valve_i=0;valve_i<7;valve_i++){	//closes any open valves
		if (valve[valve_i]==0){
			auto_valve(valve_i);
			}
		else {}
		}
}	
/////////////////Auto Valve Function////////////////////////////////////////////////
void auto_valve(int number){
	int valve_ctrl_int=0;
	if (number==1)   
		{
			control_A=0;  //0b000=v1
			control_B=0;
			control_C=0; 
			valve_ctrl_int=0;
			v1_on=~v1_on;  //toggle valve staus led
		 }
	else if (number==2)
		{
			control_A=1;   //0b001=v2 etc
			control_B=0;
			control_C=0;
			valve_ctrl_int=1;
			v2_on=~v2_on;
		}
   	else if (number==3)
		{
			control_A=0;
			control_B=1;
			control_C=0;
			valve_ctrl_int=2;
			v3_on=~v3_on;
		}	
	else if (number==4)
		{
			control_A=1;
			control_B=1;
			control_C=0;
			valve_ctrl_int=3;
			v4_on=~v4_on; 
		}
		
	else if (number==5)
		{
			control_A=0;
			control_B=0;
			control_C=1;
			valve_ctrl_int=4;
			v5_on=~v5_on;
		}

	else if (number==6)
		{
			control_A=1;
			control_B=0;
			control_C=1;
			valve_ctrl_int=5;
			v6_on=~v6_on;
		}

	else if (number==7)
		{
			control_A=0;
			control_B=1;
			control_C=1;
			valve_ctrl_int=6;
			v7_on=~v7_on;
		  }
	else if (number==8)
		{
			control_A=1;
			control_B=1;
			control_C=1;
			valve_ctrl_int=7; 
		}
			
	else {}	  //do nothing
	drive_seq(); //run led sequence

	 rst_drive();  //motor does not overheat
	 control_A=0;
	 control_B=0;	//reset control bits
	 control_C=0;
	 if (valve[valve_ctrl_int]== 1){  //toggles valve array so program "knows" which valves are open/closed
	 	valve[valve_ctrl_int]=0;
		}
	 else{ valve[valve_ctrl_int]=1;
	 }
}
 
//////////////msec_delay Function////////////
void msec_delay(unsigned int  req_delay)	//msec delay
    {
      unsigned int  p, q;
      for (p = 0; p < req_delay; p = p +1)
        for (q = 0; q < 1275; q = q +1);  //1275 from controller cycle
		
     }