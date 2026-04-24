/*
Thực hiện bài tập trên MCU SONiX SN32F407 với mô phỏng đồng hồ
*/
#include <SN32F400.h>
#include <SN32F400_Def.h>
#include "..\Driver\GPIO.h"
#include "..\Driver\WDT.h"
#include "..\Driver\SPI.h"
#include "..\Driver\I2C.h"
#include "..\Driver\CT16B0.h"
#include "..\Driver\CT16B1.h"
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

#define BUZZER_ALARM_VALUE 2
#define BUZZER_BEEP_VALUE 2
#define BUZZER_OFF_VALUE 0xFF

/*_____ S T R U C T U R E S ______________________________________________*/


/*
clock_time_t : cấu trúc lưu trữ thời gian

AlarmTimer: lưu thời gian báo thức, được đọc từ EEPROM khi khởi động và cập nhật khi người dùng thiết lập báo thức mới
SegementTimer: lưu thời gian hiện tại của đồng hồ, được cập nhật mỗi giây và cũng là thời gian hiển thị lên 7-segment nếu có người dùng tương tác
TempTimer: biến tạm để lưu thời gian để lưu thời gian cũ trước khi mà người dùng thiết lập báo thức mới
*/

typedef struct {
    uint8_t sec;
    uint8_t min;
    uint8_t hour;
} clock_time_t;

clock_time_t SegmentTimer= {0, 0, 0};
clock_time_t TempTimer= {0, 0, 0};
clock_time_t AlarmTimer = {0,0,0};

/* 
State machine cho system mode gồm có các state:
 * - MODE_NORMAL: Chế độ bình thường, hiển thị thời gian
 * - MODE_SET_HOUR: Thiết lập giờ
 * - MODE_SET_HOUR_ALARM: Thiết lập giờ cho báo thức
 * - MODE_SET_MIN: Thiết lập phút
 * - MODE_SET_MIN_ALARM: Thiết lập phút cho báo thức
*/

typedef enum {
    MODE_NORMAL,
    MODE_SET_HOUR,
		MODE_SET_HOUR_ALARM,
    MODE_SET_MIN,
		MODE_SET_MIN_ALARM
} system_mode_t; 	

system_mode_t mode = MODE_NORMAL; 


/*
State machine cho buzzer gồm có các state:
 * - BUZZER_IDLE: Buzzer không hoạt động
 * - BUZZER_KEY_BEEP: Buzzer phát âm thanh khi nhấm phím hoặc thoát chế độ chỉnh / hẹn giờ
 * - BUZZER_ALARM: Buzzer phát âm thanh báo thức
*/

typedef enum {
    BUZZER_IDLE,
    BUZZER_KEY_BEEP,
    BUZZER_ALARM
} buzzer_state_t;

buzzer_state_t buzzer_state = BUZZER_IDLE; // State machine for buzzer

/* Các biến hỗ trợ cho việc blink khi chỉnh giờ và hẹn giờ, cũng như hiển thị giá trị lên 7-segment */
uint16_t blink_counter = 0; // đếm giây để điều khiển blink, reset sau mỗi 500ms
uint8_t blink_state = 1; // 1 = ON, 0 = OFF

// Biến lưu giá trị hiển thị trên 7-segment
uint16_t display_val = 0;

// Buffer lưu giờ và phút của báo thức để ghi vào EEPROM
uint8_t alarm_buffer[2];

/* Biến hỗ trợ cho buzzer */
uint16_t buzzer_timer = 0;     // tổng thời gian buzzer đã hoạt động
uint16_t buzzer_toggle = 0;    // điều khiển duty cycle bên trong alarm (50%)
uint8_t buzzer_output = 0;     // trạng thái hiện tại (ON/OFF)

uint16_t key_code; // Lưu trữ key được bấm
uint16_t edit_time_out_sec = 0; // Điều khiển timeout khi đồng hồ ở chế độ chỉnh giờ hoặc hẹn giờ, reset sau mỗi lần có thao tác, timeout sau 30s không thao tác

/*_____ F U N C T I O N S ______________________________________________*/

/*
Lưu thời gian báo thức vào EEPROM và khôi phục lại thời gian hiển thị ban đầu sau khi thiết lập báo thức mới.
AlarmTimer được cập nhật và lưu vào EEPROM, khi nào SegmentTimer chạm đến AlarmTimer thì buzzer sẽ kêu. 
TempTimer được dùng để lưu thời gian hiển thị ban đầu trước khi người dùng thiết lập báo thức mới, sau khi lưu xong sẽ khôi phục lại SegmentTimer về thời gian ban đầu.
*/
void save_time_to_eeprom(clock_time_t* TempTimer, clock_time_t* SegmentTimer, clock_time_t* AlarmTimer, uint8_t* alarm_buffer)
{
	AlarmTimer->hour = SegmentTimer->hour;
	AlarmTimer->min = SegmentTimer->min;
	alarm_buffer[0] = AlarmTimer->hour;
	alarm_buffer[1] = AlarmTimer->min;

	eeprom_write(EEPROM_WRITE_ADDR, EEPROM_ALARM_ADDR, alarm_buffer, 2);
	*SegmentTimer = *TempTimer;
}

int	main(void)
{
	/* Khởi tạo hệ thống */
	SystemInit();
	SystemCoreClockUpdate();				
	
	PFPA_Init();										

	// Chỉnh GPIO không pin-out về input pull-up để tránh trạng thái floating
	NotPinOut_GPIO_init();
	
	/* 	Khởi tạo các ngoại vi cần thiết: GPIO, WDT, I2C, CT16B0 (buzzer), CT16B1 (timer) */
	GPIO_Init();								// GPIO cho LED, 7-segment, keyscan
	
	WDT_Init();									//Watchdog timer
	
	I2C0_Init();								// I2C0 dùng để giao tiếp với EEPROM
	
	CT16B0_Init();						// PWM cho buzzer
	
	// P.30 là pin PWM0 của CT16B0, dùng để điều khiển buzzer, set pin này về chức năng PWM
	SN_PFPA->CT16B0_b.PWM0 = 1;

	// CT16B1 là timer chính để cập nhật thời gian, cho các flag 1ms và flag 1s
	CT16B1_Init();						
	//void eeprom_read(uint8_t addr,uint8_t reg,uint8_t *dat,uint16_t length);
	
	// Khi khởi động, đọc thời gian báo thức đã lưu từ EEPROM vào AlarmTimer 
	eeprom_read(EEPROM_READ_ADDR, EEPROM_ALARM_ADDR, alarm_buffer, 2);
	AlarmTimer.hour = alarm_buffer[0];
	AlarmTimer.min = alarm_buffer[1];
	

	while (1)
	{
		__WDT_FEED_VALUE;
		uint16_t key = KeyScan();

		if(timer_1s_flag){
			if(mode == MODE_SET_HOUR || mode == MODE_SET_MIN )
			{
				edit_time_out_sec++; 
				if(edit_time_out_sec >= 30) // timeout sau 30s không thao tác ở chế độ chỉnh giờ
				{
					edit_time_out_sec = 0;
					mode = MODE_NORMAL; // Trở về chế độ bình thường
					buzzer_state = BUZZER_KEY_BEEP; // beep khi timeout
				}
			}
			else if(mode == MODE_SET_HOUR_ALARM || mode == MODE_SET_MIN_ALARM)
			{
				edit_time_out_sec++;
				if(edit_time_out_sec >= 30) // timeout sau 30s không thao tác ở chế độ chỉnh báo thức
				{
					save_time_to_eeprom(&TempTimer, &SegmentTimer, &AlarmTimer, alarm_buffer); // Lưu báo thức khi thoát
					edit_time_out_sec = 0;
					mode = MODE_NORMAL; // Trở về chế độ bình thường
					buzzer_state = BUZZER_KEY_BEEP; // beep khi timeout
				}
			}

			timer_1s_flag = 0;  // reset flag sau khi xử lý
			
			// Cập nhật thời gian hiện tại mỗi giây
			SegmentTimer.sec+= 1;
			if (SegmentTimer.sec == 60) { SegmentTimer.sec = 0; SegmentTimer.min++; }
			if (SegmentTimer.min == 60) { SegmentTimer.min = 0; SegmentTimer.hour++; }
			if (SegmentTimer.hour == 24) SegmentTimer.hour = 0;
			
			// Đổi thời gian sang giá trị hiển thị trên 7-segment, chỉ hiển thị giờ và phút
        display_val = SegmentTimer.hour * 100 + SegmentTimer.min;   
		}
		

		if(timer_1ms_flag)
    	{
			// Điều khiển blink khi ở chế độ chỉnh giờ hoặc hẹn giờ, blink mỗi 500ms
			timer_1ms_flag = 0;		
			blink_counter++;
			if(blink_counter >= 500)
			{
				blink_counter = 0;
				blink_state ^= 1;
			}
					
			Digital_DisplayDEC(display_val);  // Hiển thị giá trị lên 7-segment
			segment_buff[1] |= 0x80; // Dấu chấm sau số giờ luôn bật
			
			if( (mode == MODE_SET_HOUR || mode == MODE_SET_HOUR_ALARM))
			{
				if (blink_state){
					// LED D6 nhấp nháy khi chỉnh giờ 
					SET_LED0_ON;
				}
				else{
					SET_LED0_OFF;
					segment_buff[0] = 0;
					segment_buff[1] = 0;
					segment_buff[1] |= 0x80; 
				}
				
			}
			else if( (mode == MODE_SET_MIN || mode == MODE_SET_MIN_ALARM ))
			{
				if (blink_state){
					// LED D6 nhấp nháy khi chỉnh phút
					SET_LED0_ON;
				}
				else{
					SET_LED0_OFF;
					segment_buff[2] = 0;
					segment_buff[3] = 0;
				}
				
			}
		
		switch(buzzer_state)
		{
			case BUZZER_IDLE:
			// 	Mặc định khi không có sự kiện nào, buzzer tắt
				set_buzzer_pitch(0xFF);
				break;

			case BUZZER_ALARM:
			/*
			Báo thức diễn ra trong 5s: buzzer sẽ toggle giữa BUZZER_ALARM_VALUE và BUZZER_OFF_VALUE mỗi 500ms để tạo hiệu ứng âm thanh báo thức, sau đó tắt hoàn toàn.
			*/
				buzzer_timer++;
				buzzer_toggle++;

				// toggle mỗi 500ms
				if(buzzer_toggle >= 500)
				{
					buzzer_toggle = 0;
					buzzer_output ^= 1;
				}

				if(buzzer_output)
					set_buzzer_pitch(BUZZER_ALARM_VALUE);
				else
					set_buzzer_pitch(BUZZER_OFF_VALUE);

				// stop sau 5s
				if(buzzer_timer >= 5000)
				{
					buzzer_state = BUZZER_IDLE;
					set_buzzer_pitch(BUZZER_OFF_VALUE);
					buzzer_timer = 0;
					buzzer_output = 0;
				}
				break;

			case BUZZER_KEY_BEEP:
			/* Beep ngắn với thời lượng 300ms */
				buzzer_timer++;

				set_buzzer_pitch(BUZZER_BEEP_VALUE);

				if(buzzer_timer >= 300)   
				{
					buzzer_state = BUZZER_IDLE;
					set_buzzer_pitch(BUZZER_OFF_VALUE);
					buzzer_timer = 0;
					buzzer_output = 0;
				}

				break;	
		}	
			Digital_Scan();                   // Cập nhật trạng thái keyScan	
    	}
		

		if(key == KEY_1)   // Transition cho state machine của system mode khi chỉnh giờ
		{
			buzzer_state = BUZZER_KEY_BEEP;
				switch(mode)
				{
						case MODE_NORMAL:
						case MODE_SET_HOUR_ALARM:
						case MODE_SET_MIN_ALARM:
								mode = MODE_SET_HOUR;
								break;

						case MODE_SET_HOUR:
								mode = MODE_SET_MIN;
								edit_time_out_sec = 0; // reset timeout khi chuyển state
								break;

						case MODE_SET_MIN:
								mode = MODE_NORMAL;
								edit_time_out_sec = 0; // reset timeout khi chuyển state
								break;
				}
		}

		if(key == KEY_13)   // Transition cho state machine của system mmode khi thiết lập báo thức
		{
				buzzer_state = BUZZER_KEY_BEEP;

				switch(mode)
				{
						case MODE_NORMAL:
						case MODE_SET_HOUR:
						case MODE_SET_MIN:
								TempTimer = SegmentTimer;
								mode = MODE_SET_HOUR_ALARM;
								break;

						case MODE_SET_HOUR_ALARM:
								edit_time_out_sec = 0; // reset timeout khi chuyển state
								mode = MODE_SET_MIN_ALARM;
								break;

						case MODE_SET_MIN_ALARM:
								edit_time_out_sec = 0; // reset timeout khi chuyển state
								
								// Lưu giờ hẹn vào EEPROM
								save_time_to_eeprom(&TempTimer, &SegmentTimer, &AlarmTimer, alarm_buffer);
								display_val = SegmentTimer.hour * 100 + SegmentTimer.min;
								mode = MODE_NORMAL;
								break;
				}
		}

		if (AlarmTimer.hour == SegmentTimer.hour && AlarmTimer.min == SegmentTimer.min && SegmentTimer.sec == 0 && mode == MODE_NORMAL) {
			buzzer_state = BUZZER_ALARM; // Bật buzzer khi đến giờ báo thức
		}

		// Nhấn key 4 để tăng giá trị giờ hoặc phút và key 8 để giảm giá trị giờ hoặc phút, tùy vào state hiện tại của system mode
		if(mode == MODE_SET_HOUR || mode == MODE_SET_HOUR_ALARM)
		{
				
				if(key == KEY_4) 
				{
						edit_time_out_sec = 0; // reset timeout khi có thao tác
						buzzer_state = BUZZER_KEY_BEEP;
						
						SegmentTimer.hour = (SegmentTimer.hour + 1) % 24; // tăng giờ, nếu vượt quá 23 về 0
				}
				else if(key == KEY_8) 
				{
						edit_time_out_sec = 0; // reset timeout khi có thao tác
						buzzer_state = BUZZER_KEY_BEEP;
					
						SegmentTimer.hour = (SegmentTimer.hour + 23) % 24; // giảm giờ, nếu giảm từ 0 sẽ về 23
				}
		}
		else if(mode == MODE_SET_MIN || mode == MODE_SET_MIN_ALARM)
		{
				if(key == KEY_4) 
				{
						edit_time_out_sec = 0; // reset timeout khi có thao tác
						buzzer_state = BUZZER_KEY_BEEP;
						SegmentTimer.min = (SegmentTimer.min + 1) % 60; // tăng phút, nếu vượt quá 59 về 0
				}
				else if(key == KEY_8) 
				{
						edit_time_out_sec = 0; // reset timeout khi có thao tác
						buzzer_state = BUZZER_KEY_BEEP;
						SegmentTimer.min = (SegmentTimer.min + 59) % 60; // giảm phút, nếu giảm từ 0 sẽ về 59
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
