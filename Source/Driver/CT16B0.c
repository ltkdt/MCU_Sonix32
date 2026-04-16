/******************** (C) COPYRIGHT 2023 SONiX *******************************
* COMPANY:		SONiX
* DATE:				2023/11
* AUTHOR:			SA1
* IC:					SN32F400
* DESCRIPTION:	CT16B0 related functions.
*____________________________________________________________________________
*	REVISION	Date				User		Description
*	1.0				2023/11/06	SA1			First release
*
*____________________________________________________________________________
* THE PRESENT SOFTWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
* WITH CODING INFORMATION REGARDING THEIR PRODUCTS TIME TO MARKET.
* SONiX SHALL NOT BE HELD LIABLE FOR ANY DIRECT, INDIRECT OR CONSEQUENTIAL 
* DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE CONTENT OF SUCH SOFTWARE
* AND/OR THE USE MADE BY CUSTOMERS OF THE CODING INFORMATION CONTAINED HEREIN 
* IN CONNECTION WITH THEIR PRODUCTS.
*****************************************************************************/

/*_____ I N C L U D E S ____________________________________________________*/
#include <SN32F400.h>
#include "CT16.h"
#include "CT16B0.h"

/*_____ D E C L A R A T I O N S ____________________________________________*/
void CT16B0_Init(void);
void CT16B0_NvicEnable(void);
void CT16B0_NvicDisable(void);

/*_____ D E F I N I T I O N S ______________________________________________*/

/*_____ M A C R O S ________________________________________________________*/

/*_____ F U N C T I O N S __________________________________________________*/
uint16_t timer_repeat = 0;
uint8_t timer_1s_flag = 0;
uint8_t timer_1ms_flag = 0;
/*****************************************************************************
* Function		: CT16B0_IRQHandler
* Description	: ISR of CT16B0 interrupt
* Input			: None
* Output		: None
* Return		: None
* Note			: None
*****************************************************************************/
void CT16B0_IRQHandler(void)
{
	uint32_t ris = SN_CT16B0->RIS;	//read interrupt request
	if(ris & (1 << 5))		//interrupt flag for match MR9
	{
		timer_repeat++;
		if(timer_repeat >= 1000)
		{
			timer_repeat = 0;
			timer_1s_flag = 1;
		}
		timer_1ms_flag = 1;
	}
	
	//clear ct16b0 group interrupt flag
	SN_CT16B0->IC = 0xffffffff;
}

/*****************************************************************************
* Function		: CT16B0_Init
* Description	: Initialization of CT16B0 timer
* Input			: None
* Output		: None
* Return		: None
* Note			: None
*****************************************************************************/
void CT16B0_Init(void)
{
	//Enable P_CLOCK for CT16B0.
	__CT16B0_ENABLE;
	
	SN_CT16B0->MR9 = 12 * 1000 -1;	//HCLK=12MHz.timer 1ms
	
	SN_CT16B0->MCTRL = 1<<30|			//when TC == MR9,reset TC = 0;
										 1<<29;				//when TC == MR9
	
	SN_CT16B0->TMRCTRL = 1 << 1;		//reset timer count
	while(SN_CT16B0->TMRCTRL & (1<<1));
	
	SN_CT16B0->TMRCTRL = 1;					//START TIMER
	
	NVIC_EnableIRQ(CT16B0_IRQn);		//ENABLE systerm CT16B0 interrput
}

/*****************************************************************************
* Function		: CT16B0_NvicEnable
* Description	: Enable CT16B0 timer interrupt
* Input			: None
* Output		: None
* Return		: None
* Note			: None
*****************************************************************************/
void CT16B0_NvicEnable(void)
{
	NVIC_ClearPendingIRQ(CT16B0_IRQn);
	NVIC_EnableIRQ(CT16B0_IRQn);
}

/*****************************************************************************
* Function		: CT16B0_NvicEnable
* Description	: Disable CT16B0 timer interrupt
* Input			: None
* Output		: None
* Return		: None
* Note			: None
*****************************************************************************/
void CT16B0_NvicDisable(void)
{
	NVIC_DisableIRQ(CT16B0_IRQn);
}