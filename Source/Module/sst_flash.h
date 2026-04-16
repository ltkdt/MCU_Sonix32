#ifndef __SST_FLASH_H
#define __SST_FLASH_H

/*_____ I N C L U D E S ____________________________________________________*/
#include <stdint.h>
/*_____ D E F I N I T I O N S ______________________________________________*/
#define	FLASH_READ_CMD								0x03
#define	FLASH_HIGN_SPEED_READ_CMD			0x0B

#define	FLASH_ERASE_4K_CMD						0x20
#define	FLASH_ERASE_32K_CMD						0x52
#define	FLASH_ERASE_64K_CMD						0xd8
#define	FLASH_ERASE_ALL_CMD						0x60



#define	WRITE_BYTE_CMD								0x02
#define	ADDR_AUTO_INC_WRITE_CMD				0xAD

#define	FLASH_READ_STATUS_CMD					0x05
#define	FLASH_EN_WRITE_STATUS_CMD			0x50
#define	FLASH_WRITE_STATUS_CMD				0x01

#define	FLASH_EN_WRITE_CMD						0x06
#define	FLASH_DIS_WRITE_CMD						0x04

#define	FLASH_READ_ID_CMD							0x90



/*_____ M A C R O S ________________________________________________________*/

/*_____ D E C L A R A T I O N S ____________________________________________*/
uint16_t flash_read_id(void);
void flash_erase_sector(uint32_t adr);
void flash_write(uint32_t adr,uint16_t len,uint8_t *buff);
void flash_read(uint32_t adr,uint16_t len,uint8_t *buff);
#endif	/*__SST_FLASH_H*/
