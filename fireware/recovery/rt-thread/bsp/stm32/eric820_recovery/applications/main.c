/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-12-18     zylx         first version
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <dfs.h>
#include <dfs_fs.h>
#include <dfs_file.h>
#include <sys/unistd.h>

#define	RECOVERY_ADDR			0x081A0000
#define	APP_ADDR					0x08004000
#define APP_START_SECTOR	1
#define APP_NOF_SECTORS		20

extern RTC_HandleTypeDef RTC_Handler;
const struct dfs_mount_tbl mount_table[] = {
	{"W25Q256","/","elm",0,0},
	{0}
};

int fd;
int filelen;
struct stat filestat;

uint32_t file_buf[1024];
int readed = 0;

static FLASH_EraseInitTypeDef EraseInitStruct;
uint32_t SectorError = 0, Address = 0;
int main(void)
{
	rt_base_t level;
	struct timeval tv = { 0 };
	gettimeofday(&tv, RT_NULL);
	while (1)
	{
		rt_thread_mdelay(500);
		//check if rtthread.bin exist
		if(stat("rtthread.bin",&filestat)==0)
		{
			filelen = filestat.st_size;
			fd = open("rtthread.bin", O_RDONLY);
			if(fd > 0)
			{
				//unlock flash
				HAL_FLASH_Unlock();
				//erase flash sector 2-20
				EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
				EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
				EraseInitStruct.Sector = APP_START_SECTOR;
				EraseInitStruct.NbSectors = APP_NOF_SECTORS;
				//level = rt_hw_interrupt_disable();
				FLASH_WaitForLastOperation(50000U);
				if(HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK)
				{
					rt_kprintf("erase error!!!\n");
				}
				//rt_hw_interrupt_enable(level);
				Address = APP_ADDR;
				while(filelen>0)
				{
					//read
					readed = read(fd, (uint8_t *)file_buf, 4096);
					//program
					uint32_t * buf = file_buf;
					while(readed)
					{
						HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, Address, *buf);
						buf++;
						Address+=4;
						readed-=4;
					}
					filelen -= 4096;
				}
				//lock flash
				HAL_FLASH_Lock(); 
			}
		}
		HAL_RTCEx_BKUPWrite(&RTC_Handler, RTC_BKP_DR2, 0x00000000);
		//reboot
		rt_hw_cpu_reset();
	}
}

