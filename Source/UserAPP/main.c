/*

For contest

*/
#include <SN32F400.h>
#include <SN32F400_Def.h>
#include "..\Driver\GPIO.h"
#include "..\Driver\WDT.h"
#include "..\Driver\SPI.h"
#include "..\Driver\I2C.h"
#include "..\Driver\CT16B0.h"
#include "..\Driver\Utility.h"
#include "..\Module\sst_flash.h"
#include "..\Module\Segment.h"
#include "..\Module\KeyScan.h"
#include "..\Module\EEPROM.h"
#include "..\Module\Buzzer.h"
/*_____ D E C L A R A T I O N S ____________________________________________*/
void PFPA_Init(void);
void NotPinOut_GPIO_init(void);


/*_____ D E F I N I T I O N S ______________________________________________*/
#ifndef	SN32F407					//Do NOT Remove or Modify!!!
	#error Please install SONiX.SN32F4_DFP.0.0.18.pack or version >= 0.0.18
#endif
#define	PKG						SN32F407				//User SHALL modify the package on demand (SN32F407)

	
#define	EEPROM_WRITE_ADDR			0xa0
#define	EEPROM_READ_ADDR			0xa1	
#define EEPROM_ALARM_ADDR  0x00
/*_____ M A C R O S ________________________________________________________*/

/*_____ F U N C T I O N S __________________________________________________*/
uint32_t flash_addr = 0;
uint8_t write_flash_data[64];
uint8_t read_flash_data[64];
/*****************************************************************************
* Function		: main
* Description	: uart0 send the received data
* Input			: None
* Output		: None
* Return		: None
* Note			: Connect P3.1(UTXD0) and P3.2(URXD0)
*****************************************************************************/

typedef struct {
    uint8_t sec;
    uint8_t min;
    uint8_t hour;
} clock_time_t;

typedef enum {
    MODE_NORMAL,
    MODE_SET_HOUR,
		MODE_SET_HOUR_ALARM,
    MODE_SET_MIN,
		MODE_SET_MIN_ALARM
} system_mode_t; 		// State machine cho task 2, chinh gio, phut...

// Blink cho Task 2
uint16_t blink_counter = 0;
uint8_t blink_state = 1; // 1 = ON, 0 = OFF

typedef enum {
    BUZZER_IDLE,
    BUZZER_KEY_BEEP,
    BUZZER_ALARM
} buzzer_state_t;

buzzer_state_t buzzer_state = BUZZER_IDLE; // State machine for buzzer

// NORMAL ? SET_HOUR ? SET_MIN ? NORMAL

system_mode_t mode = MODE_NORMAL; 
clock_time_t SegmentTimer= {0, 0, 0};
clock_time_t TempTimer= {0, 0, 0};
clock_time_t AlarmTimer = {0,0,0};
uint16_t display_val = 0;
uint8_t alarm_buffer[2];

uint16_t buzzer_timer = 0;     // tổng thời gian
uint16_t buzzer_toggle = 0;    // dùng cho ON/OFF
uint8_t buzzer_output = 0;     // trạng thái hiện tại (ON/OFF)

int	main(void)
{
	SystemInit();
	SystemCoreClockUpdate();				//Must call for SN32F400, Please do NOT remove!!!
	
	PFPA_Init();										//User shall set PFPA if used, do NOT remove!!!

	//1. User SHALL define PKG on demand.
	//2. User SHALL set the status of the GPIO which are NOT pin-out to input pull-up.
	NotPinOut_GPIO_init();
	
	GPIO_Init();								//initial gpio
	
	WDT_Init();									//Set WDT reset overflow time ~ 250ms
	
	I2C0_Init();								//Init I2C before any EEPROM read/write
	
	CT16B0_Init();
	// void eeprom_read(uint8_t addr,uint8_t reg,uint8_t *dat,uint16_t length);
	
	// EEPROM_Read(EEPROM_READ_ADDR, EEPROM_ALARM_ADDR, alarm_buffer, 2);
	// AlarmTimer.hour = alarm_buffer[0];
	// AlarmTimer.min = alarm_buffer[1];
	

	while (1)
	{
		__WDT_FEED_VALUE;
		
		
		//  Theo example 6 
		if(timer_1s_flag){
			
			timer_1s_flag = 0;
			
			SegmentTimer.sec+= 1;
			if (SegmentTimer.sec == 60) { SegmentTimer.sec = 0; SegmentTimer.min++; }
			if (SegmentTimer.min == 60) { SegmentTimer.min = 0; SegmentTimer.hour++; }
			if (SegmentTimer.hour == 24) SegmentTimer.hour = 0;
			
			// Convert HH.MM ? 4 digits
        display_val = SegmentTimer.hour * 100 + SegmentTimer.min;   
		}
		
		if(timer_1ms_flag)
    {
        timer_1ms_flag = 0;		
			 // blink logic
        blink_counter++;
        if(blink_counter >= 500)
        {
            blink_counter = 0;
            blink_state ^= 1;
        }
				
		Digital_DisplayDEC(display_val);  // set buffer
		segment_buff[1] |= 0x80; // SEG_H     
		
		if( (mode == MODE_SET_HOUR || mode == MODE_SET_HOUR_ALARM) && blink_state == 0)
		{
			// t?t HH (digit 0,1)
			segment_buff[0] = 0;
			segment_buff[1] = 0;
		}
		else if( (mode == MODE_SET_MIN || mode == MODE_SET_MIN_ALARM ) && blink_state == 0)
		{
			// t?t MM (digit 2,3)
			segment_buff[2] = 0;
			segment_buff[3] = 0;
		}
		
	switch(buzzer_state)
    {
        case BUZZER_IDLE:
            set_buzzer_pitch(0);
            break;

        case BUZZER_ALARM:
            buzzer_timer++;
            buzzer_toggle++;

            // toggle mỗi 500ms
            if(buzzer_toggle >= 500)
            {
                buzzer_toggle = 0;
                buzzer_output ^= 1;
            }

            if(buzzer_output)
                set_buzzer_pitch(4);
            else
                set_buzzer_pitch(0);

            // stop sau 5s
            if(buzzer_timer >= 5000)
            {
                buzzer_state = BUZZER_IDLE;
                set_buzzer_pitch(0);
            }
            break;

        case BUZZER_KEY_BEEP:
            buzzer_timer++;

        	set_buzzer_pitch(2);

			if(buzzer_timer >= 300)   // 300ms
			{
				buzzer_state = BUZZER_IDLE;
				set_buzzer_pitch(0);
			}

            break;	
	}	
	
	Digital_Scan();                   // multiplex	
    }
		
	uint16_t key = KeyScan();
		
		// SẼ BỔ SUNG TRANSITION CHO ALARM STATE MACHINE

		if(key == KEY_1)   // ?? SET TIME STATE MACHINE
		{
			buzzer_state = BUZZER_KEY_BEEP; // Bật buzzer khi nhấn phím
				switch(mode)
				{
						case MODE_NORMAL:
						case MODE_SET_HOUR_ALARM:
						case MODE_SET_MIN_ALARM:
								mode = MODE_SET_HOUR;
								break;

						case MODE_SET_HOUR:
								mode = MODE_SET_MIN;
								break;

						case MODE_SET_MIN:
								mode = MODE_NORMAL;
								break;
				}
		}

		if(key == KEY_13)   // ALARM STATE MACHINE
		{
			buzzer_state = BUZZER_KEY_BEEP; // Bật buzzer khi nhấn phím
				switch(mode)
				{
						case MODE_NORMAL:
						case MODE_SET_HOUR:
						case MODE_SET_MIN:
								TempTimer = SegmentTimer;
								mode = MODE_SET_HOUR_ALARM;
								break;

						case MODE_SET_HOUR_ALARM:
								
								mode = MODE_SET_MIN_ALARM;
								break;

						case MODE_SET_MIN_ALARM:
								
								AlarmTimer.hour = SegmentTimer.hour;
								AlarmTimer.min = SegmentTimer.min;
								alarm_buffer[0] = AlarmTimer.hour;
								alarm_buffer[1] = AlarmTimer.min;
						
								

								eeprom_write(EEPROM_WRITE_ADDR, EEPROM_ALARM_ADDR, alarm_buffer, 2);
								SegmentTimer = TempTimer;
								display_val = SegmentTimer.hour * 100 + SegmentTimer.min;
								mode = MODE_NORMAL;
								break;
				}
		}

		if (AlarmTimer.hour == SegmentTimer.hour && AlarmTimer.min == SegmentTimer.min && mode == MODE_NORMAL) {
			buzzer_state = BUZZER_ALARM; // Bật buzzer khi đến giờ báo thức
		}

		if(mode == MODE_SET_HOUR || mode == MODE_SET_HOUR_ALARM)
		{
			buzzer_state = BUZZER_KEY_BEEP; // Bật buzzer khi nhấn phím
				// ?? increase hour
				if(key == KEY_4) 
				{
						SegmentTimer.hour = (SegmentTimer.hour + 1) % 24; // increase hour, wrap around at 24
				}
				else if(key == KEY_8) 
				{
						SegmentTimer.hour = (SegmentTimer.hour + 23) % 24; // decrease hour, wrap around at 0
				}
		}
		else if(mode == MODE_SET_MIN || mode == MODE_SET_MIN_ALARM)
		{
			buzzer_state = BUZZER_KEY_BEEP; // Bật buzzer khi nhấn phím
				// ?? increase minute
				if(key == KEY_4) 
				{
						SegmentTimer.min = (SegmentTimer.min + 1) % 60; // increase minute, wrap around at 60
				}
				else if(key == KEY_8) 
				{
						SegmentTimer.min = (SegmentTimer.min + 59) % 60; // decrease minute, wrap around at 0
				}
		}
	}
}


/*****************************************************************************
* Function		: NotPinOut_GPIO_init
* Description	: Set the status of the GPIO which are NOT pin-out to input pull-up. 
* Input				: None
* Output			: None
* Return			: None
* Note				: 1. User SHALL define PKG on demand.
*****************************************************************************/
void NotPinOut_GPIO_init(void)
{
#if (PKG == SN32F405)
	//set P0.4, P0.6, P0.7 to input pull-up
	SN_GPIO0->CFG = 0x00A008AA;
	//set P1.4 ~ P1.12 to input pull-up
	SN_GPIO1->CFG = 0x000000AA;
	//set P3.8 ~ P3.11 to input pull-up
	SN_GPIO3->CFG = 0x0002AAAA;
#elif (PKG == SN32F403)
	//set P0.4 ~ P0.7 to input pull-up
	SN_GPIO0->CFG = 0x00A000AA;
	//set P1.4 ~ P1.12 to input pull-up
	SN_GPIO1->CFG = 0x000000AA;
	//set P2.5 ~ P2.6, P2.10 to input pull-up
	SN_GPIO2->CFG = 0x000A82AA;
	//set P3.0, P3.8 ~ P3.13 to input pull-up
	SN_GPIO3->CFG = 0x0000AAA8;
#endif
}

/*****************************************************************************
* Function		: HardFault_Handler
* Description	: ISR of Hard fault interrupt
* Input			: None
* Output		: None
* Return		: None
* Note			: None
*****************************************************************************/
void HardFault_Handler(void)
{
	NVIC_SystemReset();
}
