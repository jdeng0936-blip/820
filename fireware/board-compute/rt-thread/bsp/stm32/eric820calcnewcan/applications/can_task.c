#include "main.h"

#include <rtconfig.h>
#include <rtthread.h>
#include <rthw.h>
#include <string.h>
#include <board.h>

#include <cjson.h>

#define CAN_TX_THREAD_STACK_SIZE	512
#define CAN_TX_THREAD_PRIORITY		30
#define CAN_TX_THREAD_SLICE_MS		10

#define CAN_RX_THREAD_STACK_SIZE	2048
#define CAN_RX_THREAD_PRIORITY		29
#define CAN_RX_THREAD_SLICE_MS		10


#define BC_ID				0x300
#define START_ID		0x400
#define iCanHead    0xCBFF

#define CAN_CMD_BC					0x01			//广播指令 回复继电器状态
#define CAN_CMD_GET_VAL			0x02			//获取电流值
#define CAN_CMD_GET_WAVE		0x03			//获取波形
#define CAN_CMD_SET_RELAY		0x04			//操作继电器
#define CAN_CMD_SET_ID			0x05			//设置ID
#define CAN_CMD_CALC				0x06			//校准

rt_list_t can_list = RT_LIST_OBJECT_INIT(can_list);

rt_timer_t bc_timer;
rt_timer_t pending_timer;

struct can_item ** can_items;
uint16_t can_item_cnt = 0;
uint16_t wave_id = 0;
extern CALC_STATUS calc_status;
extern TIM_HandleTypeDef htim12;
float max_value = 0.0f;

int8_t wave_replay_data[384];
uint8_t wave_replay_idx = 0;
volatile uint8_t wave_replay_evt = 0;

typedef struct{
	float rms_value;
	uint16_t id;
}RMS_SC;

//CAN_CLIENT * g_clients;//[1024]__attribute__((at(0xC0000000+256*32+768*9)));

//int16_t * g_wave_buf[3];// [3][256*20]__attribute__((at(0xC0000000+256*32+768*9+sizeof(CAN_CLIENT)*1024)));

RMS_SC rms_sc[3];

rt_tick_t time_start;
rt_tick_t time_end;
extern int8_t backup_buffer[];
int8_t rx_save_data[8];
extern volatile uint8_t backup_data_start;
volatile uint16_t error_can_id_1 = 0;
volatile uint16_t error_can_id_2 = 0;
volatile uint16_t error_can_id_3 = 0;
volatile uint8_t error_data_ready = 0;
typedef struct{
	CAN_RxHeaderTypeDef   RxHeader;
	uint8_t               RxData[8];
}RX_PACK;

typedef struct{
	CAN_TxHeaderTypeDef   TxHeader;
	uint8_t               TxData[8];
}TX_PACK;

RX_PACK can_rx_pack;
TX_PACK can_tx_pack;

extern CAN_HandleTypeDef hcan1;
uint32_t              TxMailbox;

rt_mq_t rx_data_mq;
rt_mq_t tx_data_mq;

uint8_t can_pack_buf[18];

int8_t error_buf[384*7];

volatile uint8_t bc_breath_req = 0;
volatile uint8_t pending_result_req = 0;
volatile uint8_t bc_pending = 0;
volatile uint8_t val_pending = 0;
volatile uint8_t wave_pending = 0;
volatile uint8_t bc_selstart_req = 0;
volatile uint8_t read_wave_req = 0;
volatile uint16_t total_cline_cnt = 0;
volatile uint16_t replay_cline_cnt = 0;
uint8_t wave_read_end = 0;
volatile uint16_t sel_time = 0;
volatile uint8_t bc_seling = 0;

const char * can_list_json = "{ \"payload\":[ \
{ \"lid\": 26,\"type\":1,\"child\":[ \
{ \"lid\": 37,\"type\":1,\"child\":[],\"cabinet\":\"AH18\",\"cid\":6}, \
{ \"lid\": 38,\"type\":1,\"child\":[],\"cabinet\":\"AH28\",\"cid\":14}, \
{ \"lid\": 39,\"type\":1,\"child\":[],\"cabinet\":\"AH38\",\"cid\":20} \
],\"cabinet\":\"AH8\",\"cid\":2}, \
{ \"lid\": 27,\"type\":1,\"child\":[],\"cabinet\":\"AH7\",\"cid\":4}, \
{ \"lid\": 28,\"type\":1,\"child\":[],\"cabinet\":\"AH8\",\"cid\":5} \
]}";

static uint16_t GetRevCrc_16(uint8_t * pData, uint16_t nLength,uint16_t init, const uint16_t *ptable)
{
  unsigned short cRc_16 = init;
  unsigned char temp;

  while(nLength-- > 0)
  {
    temp = cRc_16 & 0xFF; 
    cRc_16 = (cRc_16 >> 8) ^ ptable[(temp ^ *pData++) & 0xFF];
  }

  return cRc_16;
}
//MODBUS CRC-16
const unsigned short g_McRctable_16[256] =
{
0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};

uint16_t can_online_client_cnt(void)
{
	uint16_t cnt = 0;
	uint8_t mask_bit = 0;
	uint8_t mask_idx = 0;
	for(int i=0;i<can_item_cnt;i++)
	{
		if((can_items[i]->online == 1)&&(can_items[i]->pending == 0))
		{
			calc_status.clients_mask[mask_idx] |= (1<<mask_bit);
			cnt++;
		}
		else
		{
			calc_status.clients_mask[mask_idx] &=~ (1<<mask_bit);
		}
		mask_bit++;
		if(mask_bit == 32)
		{
			mask_bit = 0;
			mask_idx++;
		}
	}
	calc_status.clients_cnt = cnt;
	rt_kprintf("online cnt %d\r\n", cnt);
	return cnt;
}

uint16_t CRC_GetModbus16(uint8_t *pdata, uint16_t len)
{
  return GetRevCrc_16(pdata, len, 0xFFFF, g_McRctable_16);
}

void can_send_buf(uint16_t rid, uint8_t * data, uint16_t len)
{
	uint16_t pcnt = len/8;
	uint16_t ptail = len%8;

	can_tx_pack.TxHeader.TransmitGlobalTime = DISABLE;
	can_tx_pack.TxHeader.StdId = rid;
	can_tx_pack.TxHeader.DLC = 8;
	can_tx_pack.TxHeader.IDE = CAN_ID_STD;
	can_tx_pack.TxHeader.RTR = CAN_RTR_DATA;
	can_tx_pack.TxHeader.ExtId = 1;
	for(int i=0;i<pcnt;i++)
	{
		memcpy(can_tx_pack.TxData, &data[i*8], 8);
		rt_mq_send(tx_data_mq,&can_tx_pack,sizeof(TX_PACK));
	}
	if(ptail != 0)
	{
		can_tx_pack.TxHeader.DLC = ptail;
		memcpy(can_tx_pack.TxData, &data[pcnt*8], ptail);
		rt_mq_send(tx_data_mq,&can_tx_pack,sizeof(TX_PACK));
	}
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
  /* Get RX message */
  if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &can_rx_pack.RxHeader, can_rx_pack.RxData) == HAL_OK)
  {
		if(can_rx_pack.RxHeader.StdId > 0x400)
			rt_mq_send(rx_data_mq, &can_rx_pack, sizeof(RX_PACK));
  }
}
void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
	rt_kprintf("HAL_CAN_ErrorCallback\r\n");
}
void HAL_CAN_SleepCallback(CAN_HandleTypeDef *hcan)
{
	rt_kprintf("HAL_CAN_SleepCallback\r\n");
}
void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan)
{
	rt_kprintf("HAL_CAN_TxMailbox0CompleteCallback\r\n");
}
void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *hcan)
{
	rt_kprintf("HAL_CAN_TxMailbox1CompleteCallback\r\n");
}
void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *hcan)
{
	rt_kprintf("HAL_CAN_TxMailbox2CompleteCallback\r\n");
}

/**
  * @brief This function handles CAN1 TX interrupts.
  */
void CAN1_TX_IRQHandler(void)
{
  /* USER CODE BEGIN CAN1_TX_IRQn 0 */

  /* USER CODE END CAN1_TX_IRQn 0 */
  HAL_CAN_IRQHandler(&hcan1);
  /* USER CODE BEGIN CAN1_TX_IRQn 1 */

  /* USER CODE END CAN1_TX_IRQn 1 */
}

/**
  * @brief This function handles CAN1 RX0 interrupts.
  */
void CAN1_RX0_IRQHandler(void)
{
  /* USER CODE BEGIN CAN1_RX0_IRQn 0 */

  /* USER CODE END CAN1_RX0_IRQn 0 */
  HAL_CAN_IRQHandler(&hcan1);
  /* USER CODE BEGIN CAN1_RX0_IRQn 1 */

  /* USER CODE END CAN1_RX0_IRQn 1 */
}

/**
  * @brief This function handles CAN1 RX1 interrupt.
  */
void CAN1_RX1_IRQHandler(void)
{
  /* USER CODE BEGIN CAN1_RX1_IRQn 0 */

  /* USER CODE END CAN1_RX1_IRQn 0 */
  HAL_CAN_IRQHandler(&hcan1);
  /* USER CODE BEGIN CAN1_RX1_IRQn 1 */

  /* USER CODE END CAN1_RX1_IRQn 1 */
}

/**
  * @brief This function handles CAN1 SCE interrupt.
  */
void CAN1_SCE_IRQHandler(void)
{
  /* USER CODE BEGIN CAN1_SCE_IRQn 0 */

  /* USER CODE END CAN1_SCE_IRQn 0 */
  HAL_CAN_IRQHandler(&hcan1);
  /* USER CODE BEGIN CAN1_SCE_IRQn 1 */

  /* USER CODE END CAN1_SCE_IRQn 1 */
}

int parse_child(cJSON * child_node, rt_list_t* tree_node, int index)
{
	int nodes = cJSON_GetArraySize(child_node);
	for(int i=0;i<nodes;i++)
	{
		cJSON* node_item = cJSON_GetArrayItem(child_node, i);
		cJSON* lid = cJSON_GetObjectItem(node_item, "lid");
		cJSON* child = cJSON_GetObjectItem(node_item, "child");
		cJSON* cabinet = cJSON_GetObjectItem(node_item, "cabinet");
		cJSON* cid = cJSON_GetObjectItem(node_item, "cid");
		struct can_item *item = malloc(sizeof(struct can_item));
		memset(item, 0x00, sizeof(struct can_item));
		sprintf(item->cab, "%s", cabinet->valuestring);
		item->index = index++;
		item->id = cid->valueint;
		rt_list_init(&item->node);
		rt_list_insert_before(tree_node, &item->node);
		if(cJSON_GetArraySize(child)>0)
		{
			rt_list_init(&item->child_node);
			index = parse_child(child, &item->child_node, index);
		}
	}
	return index;
}

void print_tree(rt_list_t* tree_node)
{
	struct can_item *it;
	rt_list_for_each_entry(it, tree_node, node)
	{
		rt_kprintf("index: %d\n", it->index);
		rt_kprintf("id: %d\n", it->id);
		rt_kprintf("cab: %s\n", it->cab);
		can_items[it->index] = it;
		if(it->child_node.next)
			print_tree(&it->child_node);
	}
}

void bc_timer_callback(void *parameter)
{
	bc_breath_req = 1;
}

void pending_timer_callback(void *parameter)
{
	pending_result_req = 1;
}

volatile rt_ssize_t msg_size;
static void can_rx_thread(void * param)
{
	char str[64];
	RX_PACK rp;
	uint16_t index = 0;
	uint8_t founded = 0;
	uint8_t duplicate = 0;
	
	
	rt_list_init(&can_list);
	
	rt_thread_mdelay(3000);
	
	struct stat filestat;
	stat("cancfg.msg", &filestat);
	FILE * cancfg_file = fopen("cancfg.msg", "rb");
	if(cancfg_file>0)
	{
		char * cancfg_str = malloc(filestat.st_size);
		fread(cancfg_str, 1, filestat.st_size, cancfg_file);
		fclose(cancfg_file);
		cJSON* can_list_json_obj = cJSON_Parse(cancfg_str);
		
		cJSON* can_list_obj = cJSON_GetObjectItem(can_list_json_obj, "nodelist");
		index = 0;
		index = parse_child(can_list_obj, &can_list, index);
		cJSON_Delete(can_list_json_obj);
		free(cancfg_str);
	}
	else
	{
		cJSON* can_list_json_obj = cJSON_Parse(can_list_json);
		
		cJSON* can_list_obj = cJSON_GetObjectItem(can_list_json_obj, "payload");
		index = 0;
		index = parse_child(can_list_obj, &can_list, index);
		cJSON_Delete(can_list_json_obj);
	}
	
	can_items = (struct can_item **)malloc(sizeof(void *)*index);
	can_item_cnt = index;
	print_tree(&can_list);
	
	rt_kprintf("can item cnt %d\r\n", can_item_cnt);
	
	bc_timer = rt_timer_create("bc_t", bc_timer_callback, RT_NULL, 5000, RT_TIMER_FLAG_PERIODIC | RT_TIMER_FLAG_SOFT_TIMER);
	rt_timer_start(bc_timer);
	
	pending_timer = rt_timer_create("pend_t", pending_timer_callback, RT_NULL, 4000, RT_TIMER_FLAG_ONE_SHOT | RT_TIMER_FLAG_SOFT_TIMER);
	bc_breath_req = 1;
	while(1)
	{
		if(rt_mq_recv(rx_data_mq, &rp, sizeof(RX_PACK), 1)>0)
		{
			if(rp.RxHeader.StdId == 0x7E9)
			{
				//pkt for wave data
				memcpy(&wave_replay_data[wave_replay_idx*8], rp.RxData, 8);
				wave_replay_idx++;
				if(wave_replay_idx == 48)
					wave_replay_evt = 1;
			}
			else
			{
				founded = 0;
				duplicate = 0;
				for(int i=0;i<can_item_cnt;i++)
				{
					if(can_items[i]->id == rp.RxHeader.StdId - START_ID)
					{
						if(can_items[i]->pending == 1)
						{
							can_items[i]->online = 1;
							can_items[i]->pending = 0;
							founded = 1;
						}
						else
						{
							duplicate = 1;
						}
						break;
					}
				}
				switch(rp.RxData[0]&0x7f)
				{
					case CAN_CMD_BC:
					{
						rt_kprintf("bc replay from %d\r\n", rp.RxHeader.StdId - START_ID);
						//if(founded == 0)
						//	rt_kprintf("bc replay from %d\r\n", rp.RxHeader.StdId - START_ID);
						//if(duplicate == 1)
						//	rt_kprintf("duplicate bc replay from %d\r\n", rp.RxHeader.StdId - START_ID);
					}
					break;
					case CAN_CMD_GET_VAL:
					{
						float val;
						memcpy(&val, &rp.RxData[1], 4);
						sprintf(str, "val replay from %d, val is %.2f\r\n", rp.RxHeader.StdId - START_ID, val);
						rt_kprintf(str);
						for(int i=0;i<can_item_cnt;i++)
						{
							if(can_items[i]->id == rp.RxHeader.StdId - START_ID)
							{
								can_items[i]->rms = val;
								replay_cline_cnt++;
								break;
							}
						}
					}
					break;
					case CAN_CMD_GET_WAVE:
					{
						wave_replay_idx = 0;
						rt_kprintf("get wave replay from %d\r\n", rp.RxHeader.StdId - START_ID);
					}
					break;
					case CAN_CMD_SET_RELAY:
					{
						rt_kprintf("set relay replay from %d\r\n", rp.RxHeader.StdId - START_ID);
					}
					break;
				}
			}
		}
		
		if(pending_result_req == 1)
		{
			pending_result_req = 0;
			if(wave_pending == 1)
			{
				wave_pending = 0;
			}
			if(val_pending == 1)
			{
				val_pending = 0;
			}
			if(bc_pending == 1)
			{
				bc_pending = 0;
				for(int i=0;i<can_item_cnt;i++)
				{
					if(can_items[i]->pending == 1)
					{
						can_items[i]->online = 0;
					}
				}
				total_cline_cnt = can_online_client_cnt();
			}
		}
		if((bc_breath_req == 1)&&((bc_pending == 0)&&(val_pending == 0)&&(wave_pending == 0)))
		{
			bc_breath_req = 0;
			bc_pending = 1;
			for(int i=0;i<can_item_cnt;i++)
			{
				can_items[i]->pending = 1;
			}
			can_pack_buf[0] = CAN_CMD_BC;
			can_send_buf(BC_ID, can_pack_buf, 1);
			rt_timer_stop(pending_timer);
			rt_timer_start(pending_timer);
		}
		if(bc_selstart_req == 1)
		{
			bc_selstart_req = 0;
			bc_pending = 0;
			val_pending = 1;
			wave_pending = 0;
			for(int i=0;i<can_item_cnt;i++)
			{
				can_items[i]->pending = 1;
			}
			can_pack_buf[0] = CAN_CMD_GET_VAL;
			can_send_buf(BC_ID, can_pack_buf, 1);
			bc_seling = 1;
			replay_cline_cnt = 0;
			rt_timer_stop(pending_timer);
			rt_timer_start(pending_timer);
		}
		if(read_wave_req == 1)
		{
			read_wave_req = 0;
			bc_pending = 0;
			val_pending = 0;
			wave_pending = 1;
			can_pack_buf[0] = CAN_CMD_SET_RELAY;
			can_pack_buf[1] = wave_id;
			can_pack_buf[2] = wave_id>>8;
			can_pack_buf[3] = 0x00;
			can_pack_buf[4] = 10;
			can_send_buf(BC_ID, can_pack_buf, 5);
			can_pack_buf[0] = CAN_CMD_GET_WAVE;
			can_pack_buf[1] = wave_id;
			can_pack_buf[2] = wave_id>>8;
			can_send_buf(BC_ID, can_pack_buf, 3);
			rt_timer_stop(pending_timer);
			rt_timer_start(pending_timer);
		}
		if((bc_seling == 1)&&((replay_cline_cnt == total_cline_cnt)||(val_pending = 0)))
		{
			bc_seling = 0;
			//sel line
			//first find max value
			for(int i=0;i<can_item_cnt;i++)
			{
				if(can_items[i]->rms > max_value)
				{
					max_value = can_items[i]->rms;
				}
			}
			//second find most far item around max value
			for(int i=(can_item_cnt-1);i>=0;i--)
			{
				if(can_items[i]->rms>=(max_value*0.8f))	//value is bigger than 80% of max value
				{
					max_value = can_items[i]->rms;
					wave_id = 0x400 + can_items[i]->id;
					read_wave_req = 1;
					break;
				}
			}
		}
		if(wave_replay_evt == 1)
		{
			wave_replay_evt = 0;
			for(int i=0;i<384;i++)
			{
				error_buf[i*7] = backup_buffer[8*i];
				error_buf[i*7+1] = backup_buffer[8*i+1];
				error_buf[i*7+2] = backup_buffer[8*i+2];
				error_buf[i*7+3] = backup_buffer[8*i+3];
				error_buf[i*7+4] = wave_replay_data[i]/8;
				error_buf[i*7+5] = 0;
				error_buf[i*7+6] = 0;
			}
			error_can_id_1 = wave_id - 0x400;
			error_can_id_2 = 0;
			error_can_id_3 = 0;
			calc_status.rms_1 = max_value;
			calc_status.rms_2 = 0.0f;
			calc_status.rms_3 = 0.0f;
			error_data_ready = 1;
		}
	}
}
static void can_tx_thread(void * param)
{
	TX_PACK tp;
	rt_thread_mdelay(3000);
	while(1)
	{
		if(HAL_CAN_GetTxMailboxesFreeLevel(&hcan1))
		{
			if(rt_mq_recv(tx_data_mq, &tp, sizeof(TX_PACK), 1) > 0)
			{
				HAL_CAN_AddTxMessage(&hcan1, &tp.TxHeader, tp.TxData, &TxMailbox);
			}
		}
		else
		{
			//rt_thread_yield();
			rt_thread_mdelay(1);
		}
	}
}
int can_comm_init(void)
{
	rt_thread_t canrx_thread;
	rt_thread_t cantx_thread;
	
	rx_data_mq = rt_mq_create("canr_mq", sizeof(RX_PACK), 1024, RT_IPC_FLAG_FIFO);
	tx_data_mq = rt_mq_create("cant_mq", sizeof(TX_PACK), 1024, RT_IPC_FLAG_FIFO);

	cantx_thread = rt_thread_create("cant_th", can_tx_thread, RT_NULL,
														CAN_TX_THREAD_STACK_SIZE,
														CAN_TX_THREAD_PRIORITY,
														CAN_TX_THREAD_SLICE_MS);
	rt_thread_startup(cantx_thread);
	
	canrx_thread = rt_thread_create("canr_th", can_rx_thread, RT_NULL,
														CAN_RX_THREAD_STACK_SIZE,
														CAN_RX_THREAD_PRIORITY,
														CAN_RX_THREAD_SLICE_MS);
	rt_thread_startup(canrx_thread);
	
	return 0;
}
INIT_DEVICE_EXPORT(can_comm_init);

static int get_online(int argc, char **argv)
{
	for(int i=0;i<32;i++)
	{
		rt_kprintf("0x%08x\r\n", calc_status.clients_mask[i]);
	}
	return RT_EOK;
}

/* 导出到 msh 命令列表中 */
MSH_CMD_EXPORT(get_online, get online list );


