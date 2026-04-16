/******************** (C) COPYRIGHT 2013 SONiX *******************************
* COMPANY:			SONiX
* DATE:					2016/03
* AUTHOR:				SA1
* IC:						SN32F240/230/220 
* DESCRIPTION:	LED_Function.c
*____________________________________________________________________________
* REVISION	Date				User		Description
* 1.0				2018/12/21	SA1			1. First release
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

#include "sst_flash.h"

#include "..\Driver\SPI.h"
#include "..\Driver\WDT.h"
/*_____ D E C L A R A T I O N S ____________________________________________*/

/*_____ D E F I N I T I O N S ______________________________________________*/
#define	__IO_SEL_LOW	SN_GPIO1->BCLR = 1 << 8;
#define	__IO_SEL_HIGH	SN_GPIO1->BSET = 1 << 8;

/*_____ M A C R O S ________________________________________________________*/

/*_____ F U N C T I O N S __________________________________________________*/
/*****************************************************************************
* Function		: flash_enable_write_status
* Description	: Enable Write  Status Register
* Input			: None
* Output		: None
* Return		: None
* Note			: None
*****************************************************************************/
void flash_enable_write_status(void)
{
	__IO_SEL_LOW
	SPI0_RW(FLASH_EN_WRITE_STATUS_CMD);				// EWSR COMMAND
	__IO_SEL_HIGH
}
/*****************************************************************************
* Function		: flash_write_status
* Description	:  Write  Status Register ,status for BP3~BP0 Status
* Input			: None
* Output		: None
* Return		: None
* Note			: None
*****************************************************************************/
void flash_write_status(uint8_t status)
{
	__IO_SEL_LOW
	SPI0_RW(FLASH_WRITE_STATUS_CMD);				// wrsr command
	SPI0_RW(status);	
	__IO_SEL_HIGH
}
/*****************************************************************************
* Function		: flash_write_enable
* Description	: 
* Input			: None
* Output		: None
* Return		: None
* Note			: None
*****************************************************************************/
void flash_write_enable(void)
{
	__IO_SEL_LOW
	SPI0_RW(FLASH_EN_WRITE_CMD);		// Write Enable Command
	__IO_SEL_HIGH
}
/*****************************************************************************
* Function		: flash_write_disable
* Description	: 
* Input			: None
* Output		: None
* Return		: None
* Note			: None
*****************************************************************************/
void flash_write_disable(void)
{
	__IO_SEL_LOW
	SPI0_RW(FLASH_DIS_WRITE_CMD);			// Write Disable Command
	__IO_SEL_HIGH
}
/*****************************************************************************
* Function		: SPIFlash_WaitReady
* Description	:  
* Input			: 
* Output		: 
* Return		: None
* Note			: 
*****************************************************************************/
void flash_wait_ready(void)
{
	__IO_SEL_LOW;	  
	SPI0_RW(FLASH_READ_STATUS_CMD);
	while(SPI0_RW(0x00) & 0x01)
	{
		__WDT_FEED_VALUE;
	}
	__IO_SEL_HIGH;		
}
/*****************************************************************************
* Function		: flash_read_id
* Descriptio
* Input			: None
* Output		: None
* Return		: None
* Note			: None
*****************************************************************************/
uint16_t flash_read_id(void)
{
	uint16_t id;
	__IO_SEL_LOW;
	SPI0_RW(FLASH_READ_ID_CMD);
	SPI0_RW(0x00);
	SPI0_RW(0x00);
	SPI0_RW(0x00);				// address
	id = SPI0_RW(0)<<8;		// 
	id |= SPI0_RW(0);			// 
	__IO_SEL_HIGH;
	
	return id;
}
/*****************************************************************************
* Function		: flash_erase_sector
* Description	: 
* Input			: None
* Output		: None
* Return		: None
* Note			: None
*****************************************************************************/
void flash_erase_sector(uint32_t adr)
{
	flash_wait_ready();
	
	flash_enable_write_status();
	flash_write_status(0);
	
	flash_write_enable();
	
	adr &= ~(4*1024-1);
	__IO_SEL_LOW
	SPI0_RW(FLASH_ERASE_4K_CMD);
	SPI0_RW(adr>>16);
	SPI0_RW(adr>>8);
	SPI0_RW(adr&0xFF);	
	
	__IO_SEL_HIGH;
}

/*****************************************************************************
* Function		: flash_write
* Description	: auto address increment write
								each cycle write 2 byte
* Input			: None
* Output		: None
* Return		: None
* Note			: None
*****************************************************************************/
void flash_write(uint32_t adr,uint16_t len,uint8_t *buff)
{
	flash_wait_ready();
	flash_enable_write_status();	//enable write status
	flash_write_status(0);				//clear status
	
	flash_write_enable();
	
	//address must be aligent with 2byte
	adr = adr & ~(0x01);
	
	//set write cmd and write address
	__IO_SEL_LOW
	SPI0_RW(ADDR_AUTO_INC_WRITE_CMD);
	
	SPI0_RW(adr>>16);
	SPI0_RW(adr>>	8);
	SPI0_RW(adr&0xFF);
	
	SPI0_RW(*buff++);
	SPI0_RW(*buff++);	

	__IO_SEL_HIGH
	
	
	len = (len+1)>>1;		//avoid len bit0 = 1
	
	len--;							//have been write 2byte
	while(len--)
	{
		flash_wait_ready();
		__IO_SEL_LOW
		SPI0_RW(0xad);
		SPI0_RW(*buff++);
		SPI0_RW(*buff++);
		__IO_SEL_HIGH
		
	}
	
	flash_wait_ready();
	flash_write_disable();
	__IO_SEL_HIGH;
	
}

/*****************************************************************************
* Function		: flash_read
* Description	: 
* Input			: None
* Output		: None
* Return		: None
* Note			: None
*****************************************************************************/
void flash_read(uint32_t adr,uint16_t len,uint8_t *buff)
{
	uint16_t i;
	flash_wait_ready();
	__IO_SEL_LOW
	SPI0_RW(FLASH_READ_CMD);
	SPI0_RW(adr>>16);
	SPI0_RW(adr>>8);
	SPI0_RW(adr&0xFF);
	
	for(i=0;i<len;i++)
		buff[i] = SPI0_RW(0);

	__IO_SEL_HIGH
}
