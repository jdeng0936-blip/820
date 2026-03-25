/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-12-18     zylx         first version
 */
#include "main.h"
#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <drv_ext_io.h>
#include <dfs.h>
#include <dfs_fs.h>
#include <lvgl.h>
#include <lcd_port.h>
#include <cjson.h>
#include <dfs_posix.h>
#include <sys/time.h>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <webclient.h>
#include <ctype.h>
#include <string.h>
#include "mqtt_client.h"
#include "at24cxx.h"
#define DBG_ENABLE
#define DBG_SECTION_NAME    "main.mqtt"
#define DBG_LEVEL           DBG_LOG
#define DBG_COLOR
#include <rtdbg.h>

#define MQTT_URI                "tcp://36.33.241.51:8820"	//24 21 F1 33
//#define MQTT_URI                "tcp://47.106.152.155:1883"
#define MQTT_SUBTOPIC           "/ERIC18202522144329/sub"
#define MQTT_PUBTOPIC           "Server/ERIC0820"
#define MQTT_WILLMSG            "Goodbye!"

static mqtt_client client;
static int is_started = 0;
extern SETTINGS settings;
volatile uint8_t disp_settings_update = 0;
volatile uint8_t save_settings_req = 0;
volatile uint8_t save_request_reset = 0;
volatile uint8_t settings_upload_req = 0;
volatile uint8_t gnd_error_wave_upload = 0;
volatile uint8_t voltage_error_wave_upload = 0;
volatile uint8_t gnd_error_wave_save = 0;
volatile uint8_t over_error_wave_save = 0;
volatile uint8_t drop_error_wave_save = 0;
volatile uint8_t pt_error_wave_save = 0;
volatile uint8_t short_error_wave_save = 0;
volatile uint8_t gnd_error_wave_load = 0;
volatile uint8_t over_error_wave_load = 0;
volatile uint8_t drop_error_wave_load = 0;
volatile uint8_t pt_error_wave_load = 0;
volatile uint8_t short_error_wave_load = 0;
extern volatile uint8_t ui_loaded;
volatile uint8_t login = 0;
volatile uint8_t mqtt_online = 0;
volatile uint8_t mqtt_sub_ava = 0;
volatile uint8_t mqtt_start_req = 0;
volatile uint8_t update_start = 0;
volatile uint8_t download_file = 0;
volatile uint8_t download_cancfg = 0;
extern CALC_STATUS calc_status;
extern int8_t error_wavebuf[];
extern volatile uint8_t error_wave_load;
volatile uint8_t client_node_load = 0;
extern uint16_t client_nodes_cnt;
extern volatile uint8_t time_sync_req;
volatile uint8_t ui_loaded = 0;
volatile uint8_t mqtt_stop_req = 0;
volatile uint8_t disp_node_page_cnt = 0;
volatile uint8_t disp_node_page_idx = 0;
extern volatile uint8_t disp_node_page_update;
extern volatile uint8_t disp_node_page_idx;
extern volatile uint8_t disp_node_page_cnt;
extern void set_settings(void);
extern volatile uint8_t client_online_changed;
extern volatile uint8_t save_ptct_req;
extern volatile uint8_t save_vset_req;
char update_url[256] = "http://47.106.152.155/rtthread";
at24cxx_device_t i2c_dev = RT_NULL;
extern int cmux_sample(void);
extern int wiz_init(void);
extern volatile uint8_t over_error_wave_load;
extern volatile uint8_t save_nodelist_req;
volatile uint8_t i2c_check_ok = 0;
//const struct dfs_mount_tbl mount_table[] = {
//	{"W25Q256","/","elm",0,0},
//	{0}
//};

static int mqtt_start(void);

uint8_t mqtt_send_buf[12288]__attribute__((at(0x10000000)));
uint8_t mqtt_sub_buf[12288]__attribute__((at(0x10000000+sizeof(mqtt_send_buf))));
int8_t wave_ua[384]__attribute__((at(0x10000000+sizeof(mqtt_send_buf)+sizeof(mqtt_sub_buf))));
int8_t wave_ub[384]__attribute__((at(0x10000000+sizeof(mqtt_send_buf)+sizeof(mqtt_sub_buf)+sizeof(wave_ua))));
int8_t wave_uc[384]__attribute__((at(0x10000000+sizeof(mqtt_send_buf)+sizeof(mqtt_sub_buf)+sizeof(wave_ua)+sizeof(wave_ub))));
int8_t wave_u0[384]__attribute__((at(0x10000000+sizeof(mqtt_send_buf)+sizeof(mqtt_sub_buf)+sizeof(wave_ua)+sizeof(wave_ub)+sizeof(wave_uc))));
int8_t wave_ie_1[384]__attribute__((at(0x10000000+sizeof(mqtt_send_buf)+sizeof(mqtt_sub_buf)+sizeof(wave_ua)+sizeof(wave_ub)+sizeof(wave_uc)+sizeof(wave_u0))));
int8_t wave_ie_2[384]__attribute__((at(0x10000000+sizeof(mqtt_send_buf)+sizeof(mqtt_sub_buf)+sizeof(wave_ua)+sizeof(wave_ub)+sizeof(wave_uc)+sizeof(wave_u0)+sizeof(wave_ie_1))));
int8_t wave_ie_3[384]__attribute__((at(0x10000000+sizeof(mqtt_send_buf)+sizeof(mqtt_sub_buf)+sizeof(wave_ua)+sizeof(wave_ub)+sizeof(wave_uc)+sizeof(wave_u0)+sizeof(wave_ie_1)+sizeof(wave_ie_2))));
NODE client_nodes[1024]__attribute__((at(0x10000000+sizeof(mqtt_send_buf)+sizeof(mqtt_sub_buf)+sizeof(wave_ua)+sizeof(wave_ub)+sizeof(wave_uc)+sizeof(wave_u0)+sizeof(wave_ie_1)+sizeof(wave_ie_2)+sizeof(wave_ie_3))));

uint8_t base64_tmp1[512];
uint8_t base64_tmp2[512];
uint8_t base64_tmp3[512];
uint8_t base64_tmp4[512];
uint8_t base64_tmp5[512];
uint8_t base64_tmp6[512];
uint8_t base64_tmp7[512];

uint32_t udid[3];
char fmd51[33];
char fmd52[33];
uint16_t base64_encode(const char * input, int length, char * output);
extern void get_settings(void);
void make_login_packet(void)
{
	char * cjson_str;
	char str[32];
	char ver_str[32];
	cJSON * packet = cJSON_CreateObject();
	udid[0] = HAL_GetUIDw0();
	udid[1] = HAL_GetUIDw1();
	udid[2] = HAL_GetUIDw2();
	//sprintf(ver_str, "%s", DISP_VER);
	sprintf(ver_str, "%s/%s", DISP_VER, calc_status.settings.ver);
	sprintf(str, "ERIC1820%08x", udid[2]^udid[1]^udid[0]);
	for(int i=0;i<strlen(str);i++)
	{
		if (str[i] >= 'a' && str[i] <= 'z')
			str[i] = toupper(str[i]);
	}
	cJSON_AddStringToObject(packet, "cid", str);
	cJSON_AddStringToObject(packet, "type", "login");
	cJSON * payload = cJSON_CreateObject();
	
	cJSON_AddNumberToObject(payload, "rs485_id", settings.rs485_id);
	cJSON_AddNumberToObject(payload, "rs485_baud", settings.rs485_baud);
	cJSON_AddStringToObject(payload, "server_addr", settings.server_addr);
	cJSON_AddNumberToObject(payload, "server_port", settings.server_port);
	cJSON_AddNumberToObject(payload, "pt", settings.pt);
	cJSON_AddNumberToObject(payload, "ct", settings.ct);
	cJSON_AddNumberToObject(payload, "i0_limt", settings.i0_limt);
	cJSON_AddNumberToObject(payload, "i0_ct", settings.i0_ct);
	cJSON_AddNumberToObject(payload, "v_over", settings.v_over);
	cJSON_AddNumberToObject(payload, "v_drop", settings.v_drop);
	cJSON_AddNumberToObject(payload, "u0_limt", settings.u0_limt);
	cJSON_AddStringToObject(payload, "pwd", settings.pass);
	cJSON_AddNumberToObject(payload, "screen_idle", settings.idle);
	cJSON_AddNumberToObject(payload, "step", 1);
	cJSON_AddBoolToObject(payload, "rewrite", false);
	cJSON_AddStringToObject(payload, "ver", ver_str);
	cJSON_AddNumberToObject(payload, "ts", 0);
	cJSON_AddItemToObject(packet, "payload", payload);
	
	cjson_str = cJSON_Print(packet);
	
	//rt_kprintf(cjson_str);
	
	memcpy(mqtt_send_buf, cjson_str, strlen(cjson_str));
	mqtt_send_buf[strlen(cjson_str)] = 0;
	cJSON_Delete(packet);
}

void make_settings_packet(void)
{
	char * cjson_str;
	char str[32];
	cJSON * packet = cJSON_CreateObject();
	udid[0] = HAL_GetUIDw0();
	udid[1] = HAL_GetUIDw1();
	udid[2] = HAL_GetUIDw2();
	sprintf(str, "ERIC1820%08x", udid[2]^udid[1]^udid[0]);
	for(int i=0;i<strlen(str);i++)
	{
		if (str[i] >= 'a' && str[i] <= 'z')
			str[i] = toupper(str[i]);
	}
	cJSON_AddStringToObject(packet, "cid", str);
	cJSON_AddStringToObject(packet, "type", "settings");
	cJSON * payload = cJSON_CreateObject();
	
	cJSON_AddNumberToObject(payload, "pt", settings.pt);
	cJSON_AddNumberToObject(payload, "ct", settings.ct);
	cJSON_AddNumberToObject(payload, "i0_limt", settings.i0_limt);
	cJSON_AddNumberToObject(payload, "i0_ct", settings.i0_ct);
	cJSON_AddNumberToObject(payload, "v_over", settings.v_over);
	cJSON_AddNumberToObject(payload, "v_drop", settings.v_drop);
	cJSON_AddNumberToObject(payload, "u0_limt", settings.u0_limt);
	cJSON_AddStringToObject(payload, "pwd", settings.pass);
	cJSON_AddItemToObject(packet, "payload", payload);
	
	cjson_str = cJSON_Print(packet);
	
	memcpy(mqtt_send_buf, cjson_str, strlen(cjson_str));
	mqtt_send_buf[strlen(cjson_str)] = 0;
	cJSON_Delete(packet);
}

void make_status_packet(void)
{
	char * cjson_str;
	char str[32];
	cJSON * packet = cJSON_CreateObject();
	udid[0] = HAL_GetUIDw0();
	udid[1] = HAL_GetUIDw1();
	udid[2] = HAL_GetUIDw2();
	sprintf(str, "ERIC1820%08x", udid[2]^udid[1]^udid[0]);
	for(int i=0;i<strlen(str);i++)
	{
		if (str[i] >= 'a' && str[i] <= 'z')
			str[i] = toupper(str[i]);
	}
	cJSON_AddStringToObject(packet, "cid", str);
	cJSON_AddStringToObject(packet, "type", "status");
	cJSON * payload = cJSON_CreateObject();
	
	cJSON * nodelist = cJSON_AddArrayToObject(payload, "nodelist");
	for(int i = 0; i<client_nodes_cnt; i++)
	{
		cJSON * item = cJSON_CreateObject();
		cJSON_AddNumberToObject(item, "SenserID", client_nodes[i].id);
		cJSON_AddNumberToObject(item, "online_status", client_nodes[i].status);
		cJSON_AddStringToObject(item, "position", client_nodes[i].pos);
		cJSON_AddItemToArray(nodelist, item);
	}
	
	cJSON_AddItemToObject(packet, "payload", payload);
	
	cjson_str = cJSON_Print(packet);
	
	memcpy(mqtt_send_buf, cjson_str, strlen(cjson_str));
	mqtt_send_buf[strlen(cjson_str)] = 0;
	cJSON_Delete(packet);
}

void make_voltage_packet(void)
{
	char * cjson_str;
	char str[32];
	cJSON * packet = cJSON_CreateObject();
	udid[0] = HAL_GetUIDw0();
	udid[1] = HAL_GetUIDw1();
	udid[2] = HAL_GetUIDw2();
	sprintf(str, "ERIC1820%08x", udid[2]^udid[1]^udid[0]);
	for(int i=0;i<strlen(str);i++)
	{
		if (str[i] >= 'a' && str[i] <= 'z')
			str[i] = toupper(str[i]);
	}
	cJSON_AddStringToObject(packet, "cid", str);
	cJSON_AddStringToObject(packet, "type", "voltage");
	cJSON * payload = cJSON_CreateObject();
	
	cJSON_AddNumberToObject(payload, "ua", calc_status.error_ua);
	if(calc_status.error_ua>calc_status.settings.v_over)
		cJSON_AddStringToObject(payload, "ua_st", "over");
	else if(calc_status.error_ua>calc_status.settings.v_drop)
		cJSON_AddStringToObject(payload, "ua_st", "nromal");
	else if(calc_status.error_ua>calc_status.settings.v_zero)
		cJSON_AddStringToObject(payload, "ua_st", "drop");
	else
		cJSON_AddStringToObject(payload, "ua_st", "zero");
	
	cJSON_AddNumberToObject(payload, "ub", calc_status.error_ub);
	if(calc_status.error_ub>calc_status.settings.v_over)
		cJSON_AddStringToObject(payload, "ub_st", "over");
	else if(calc_status.error_ub>calc_status.settings.v_drop)
		cJSON_AddStringToObject(payload, "ub_st", "nromal");
	else if(calc_status.error_ub>calc_status.settings.v_zero)
		cJSON_AddStringToObject(payload, "ub_st", "drop");
	else
		cJSON_AddStringToObject(payload, "ub_st", "zero");
	
	cJSON_AddNumberToObject(payload, "uc", calc_status.error_uc);
	if(calc_status.error_uc>calc_status.settings.v_over)
		cJSON_AddStringToObject(payload, "uc_st", "over");
	else if(calc_status.error_uc>calc_status.settings.v_drop)
		cJSON_AddStringToObject(payload, "uc_st", "nromal");
	else if(calc_status.error_uc>calc_status.settings.v_zero)
		cJSON_AddStringToObject(payload, "uc_st", "drop");
	else
		cJSON_AddStringToObject(payload, "uc_st", "zero");
	
	cJSON_AddNumberToObject(payload, "u0", calc_status.error_u0);
	if(calc_status.error_u0>calc_status.settings.v_over)
		cJSON_AddStringToObject(payload, "u0_st", "over");
	else if(calc_status.error_u0>calc_status.settings.v_drop)
		cJSON_AddStringToObject(payload, "u0_st", "nromal");
	else if(calc_status.error_u0>calc_status.settings.v_zero)
		cJSON_AddStringToObject(payload, "u0_st", "drop");
	else
		cJSON_AddStringToObject(payload, "u0_st", "zero");
	
	cJSON_AddNumberToObject(payload, "ts", calc_status.error_ts);
	
	if(calc_status.error_slow == VOLTAGE_DROP)
		cJSON_AddNumberToObject(payload, "type", 1);
	else if(calc_status.error_slow == VOLTAGE_OVER)
		cJSON_AddNumberToObject(payload, "type", 2);
	else if(calc_status.error_slow == SHORT_ERROR)
		cJSON_AddNumberToObject(payload, "type", 3);
	else if(calc_status.error_slow == PT_ERROR)
		cJSON_AddNumberToObject(payload, "type", 4);
	
	for(int i=0;i<384;i++)
	{
		wave_ua[i] = error_wavebuf[i*7];
		wave_ub[i] = error_wavebuf[i*7+1];
		wave_uc[i] = error_wavebuf[i*7+2];
		wave_u0[i] = error_wavebuf[i*7+3];
	}
	base64_encode((char *)wave_ua, 384, (char *)base64_tmp1);
	cJSON_AddStringToObject(payload, "ua_wave", (char *)base64_tmp1);
	
	base64_encode((char *)wave_ub, 384, (char *)base64_tmp2);
	cJSON_AddStringToObject(payload, "ub_wave", (char *)base64_tmp2);
	
	base64_encode((char *)wave_uc, 384, (char *)base64_tmp3);
	cJSON_AddStringToObject(payload, "uc_wave", (char *)base64_tmp3);
	
	base64_encode((char *)wave_u0, 384, (char *)base64_tmp4);
	cJSON_AddStringToObject(payload, "u0_wave", (char *)base64_tmp4);
	
	cJSON_AddItemToObject(packet, "payload", payload);
	
	cjson_str = cJSON_Print(packet);
	
	memcpy(mqtt_send_buf, cjson_str, strlen(cjson_str));
	mqtt_send_buf[strlen(cjson_str)] = 0;
	//mqtt_send_buf[512] = 0;
	cJSON_Delete(packet);
}

void make_gnd_packet(void)
{
	char * cjson_str;
	char str[32];
	cJSON * packet = cJSON_CreateObject();
	udid[0] = HAL_GetUIDw0();
	udid[1] = HAL_GetUIDw1();
	udid[2] = HAL_GetUIDw2();
	sprintf(str, "ERIC1820%08x", udid[2]^udid[1]^udid[0]);
	for(int i=0;i<strlen(str);i++)
	{
		if (str[i] >= 'a' && str[i] <= 'z')
			str[i] = toupper(str[i]);
	}
	cJSON_AddStringToObject(packet, "cid", str);
	cJSON_AddStringToObject(packet, "type", "gnd");
	cJSON * payload = cJSON_CreateObject();
	if(calc_status.error_type == 2)
		cJSON_AddNumberToObject(payload, "type", 1);
	else
		cJSON_AddNumberToObject(payload, "type", 0);
	if(calc_status.error_line == 1)
		cJSON_AddStringToObject(payload, "error_line", "A");
	else if(calc_status.error_line == 2)
		cJSON_AddStringToObject(payload, "error_line", "B");
	else if(calc_status.error_line == 3)
		cJSON_AddStringToObject(payload, "error_line", "C");
	
	cJSON_AddNumberToObject(payload, "error_sensor_1", calc_status.error_id_1);
	cJSON_AddNumberToObject(payload, "error_sensor_2", calc_status.error_id_2);
	cJSON_AddNumberToObject(payload, "error_sensor_3", calc_status.error_id_3);
	cJSON_AddNumberToObject(payload, "error_rms_1", calc_status.rms_1*settings.i0_ct);
	cJSON_AddNumberToObject(payload, "error_rms_2", calc_status.rms_2*settings.i0_ct);
	cJSON_AddNumberToObject(payload, "error_rms_3", calc_status.rms_3*settings.i0_ct);
	
	for(int i=0;i<384;i++)
	{
		wave_ua[i] = error_wavebuf[i*7];
		wave_ub[i] = error_wavebuf[i*7+1];
		wave_uc[i] = error_wavebuf[i*7+2];
		wave_u0[i] = error_wavebuf[i*7+3];
		wave_ie_1[i] = error_wavebuf[i*7+4];
		wave_ie_2[i] = error_wavebuf[i*7+5];
		wave_ie_3[i] = error_wavebuf[i*7+6];
	}
	base64_encode((char *)wave_ua, 384, (char *)base64_tmp1);
	cJSON_AddStringToObject(payload, "ua_wave", (char *)base64_tmp1);
	
	base64_encode((char *)wave_ub, 384, (char *)base64_tmp2);
	cJSON_AddStringToObject(payload, "ub_wave", (char *)base64_tmp2);
	
	base64_encode((char *)wave_uc, 384, (char *)base64_tmp3);
	cJSON_AddStringToObject(payload, "uc_wave", (char *)base64_tmp3);
	
	base64_encode((char *)wave_u0, 384, (char *)base64_tmp4);
	cJSON_AddStringToObject(payload, "u0_wave", (char *)base64_tmp4);
	
	base64_encode((char *)wave_ie_1, 384, (char *)base64_tmp5);
	cJSON_AddStringToObject(payload, "ie_wave_1", (char *)base64_tmp5);
	
	base64_encode((char *)wave_ie_2, 384, (char *)base64_tmp6);
	cJSON_AddStringToObject(payload, "ie_wave_2", (char *)base64_tmp6);
	
	base64_encode((char *)wave_ie_3, 384, (char *)base64_tmp7);
	cJSON_AddStringToObject(payload, "ie_wave_3", (char *)base64_tmp7);
	
	cJSON_AddNumberToObject(payload, "ts", calc_status.error_ts);
	
	cJSON_AddItemToObject(packet, "payload", payload);
	
	cjson_str = cJSON_Print(packet);
	
	memcpy(mqtt_send_buf, cjson_str, strlen(cjson_str));
	mqtt_send_buf[strlen(cjson_str)] = 0;
	cJSON_Delete(packet);
}
int result[4];
void ipstr2num(char* str)
{
	int i = 0;
	int j = 0;
	char new_str[3];
	int num = 0;
	while(*str != '\0')
	{
		while((*str != '.') && (*str != '\0'))
		{
			new_str[i] = *str;
			num = num * 10 + new_str[i] - '0';
			str += 1;
			i += 1;
		}
		result[j] = num;
		num = 0;
		printf("%d \t",result[j]);
		if (*str == '\0')
		{
			break;
		}
		else
		{
			str += 1;
			i = 0;
			j += 1;	
		}	
	}
}

const char * can_list_json = "{ \"payload\":[ \
{ \"lid\": 26,\"type\":1,\"child\":[ \
{ \"lid\": 37,\"type\":1,\"child\":[],\"cabinet\":\"AH18\",\"cid\":6}, \
{ \"lid\": 38,\"type\":1,\"child\":[],\"cabinet\":\"AH28\",\"cid\":14}, \
{ \"lid\": 39,\"type\":1,\"child\":[],\"cabinet\":\"AH38\",\"cid\":20} \
],\"cabinet\":\"AH8\",\"cid\":2}, \
{ \"lid\": 27,\"type\":1,\"child\":[],\"cabinet\":\"AH7\",\"cid\":4}, \
{ \"lid\": 28,\"type\":1,\"child\":[],\"cabinet\":\"AH8\",\"cid\":5} \
]}";

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
		client_nodes[it->index].id = it->id;
		strcpy(client_nodes[it->index].pos, it->cab);
		if(it->child_node.next)
			print_tree(&it->child_node);
	}
}

void deal_substr(void)
{
	int nodes = 0;
	cJSON * json = cJSON_ParseWithLength((char *)mqtt_sub_buf, strlen((char *)mqtt_sub_buf));
	cJSON * type = cJSON_GetObjectItem(json, "type");
	cJSON * resp_ts = cJSON_GetObjectItem(json, "ts");
	
	if((type != NULL)&&(0 == strcasecmp("login", type->valuestring)))	//settings packet
	{
		cJSON * payload = cJSON_GetObjectItem(json, "payload");
		cJSON * step = cJSON_GetObjectItem(payload, "step");
		cJSON * ts = cJSON_GetObjectItem(payload, "ts");
		//if((step->valueint > settings.step)||(ts->valueint > settings.ts))
//		{
//			cJSON * nodelist = cJSON_GetObjectItem(payload, "nodelist");
//			nodes = cJSON_GetArraySize(nodelist);
//			rt_kprintf("nodlist size %d\n", nodes);
//			client_nodes_cnt = 0;
//			for(int i = 0; i < nodes; i++)
//			{
//				cJSON * node = cJSON_GetArrayItem(nodelist, i);
//				cJSON * sid = cJSON_GetObjectItem(node, "SenserID");
//				cJSON * pos = cJSON_GetObjectItem(node, "position");
//				client_nodes[i].id = atoi(sid->valuestring);
//				strcpy(client_nodes[i].pos, pos->valuestring);
//				client_nodes_cnt++;
//			}
//			disp_node_page_idx = 1;
//			disp_node_page_cnt = (client_nodes_cnt+7)/8;
//			disp_node_page_update = 1;
//			disp_settings_update = 1;
//		}
	}
	else if((type != NULL)&&(0 == strcasecmp("settings", type->valuestring)))	//settings packet
	{
		cJSON * payload = cJSON_GetObjectItem(json, "payload");
		cJSON * pt = cJSON_GetObjectItem(payload, "pt");
		settings.pt = pt->valueint;
		cJSON * ct = cJSON_GetObjectItem(payload, "ct");
		settings.ct = ct->valueint;
		cJSON * i0limt = cJSON_GetObjectItem(payload, "i0_limt");
		settings.i0_limt = i0limt->valueint;
		cJSON * i0ct = cJSON_GetObjectItem(payload, "i0_ct");
		settings.i0_ct = i0ct->valueint;
		cJSON * vover = cJSON_GetObjectItem(payload, "v_over");
		settings.v_over = vover->valueint;
		cJSON * vdrop = cJSON_GetObjectItem(payload, "v_drop");
		settings.v_drop = vdrop->valueint;
		cJSON * u0limt = cJSON_GetObjectItem(payload, "u0_limt");
		settings.u0_limt = u0limt->valueint;
		cJSON * pwd = cJSON_GetObjectItem(payload, "pwd");
		sprintf(settings.pass, "%s", pwd->valuestring);
		disp_settings_update = 1;
		save_settings_req = 1;
		save_ptct_req = 1;
		save_vset_req = 1;
	}
	else if((type != NULL)&&(0 == strcasecmp("update", type->valuestring)))	//settings packet
	{
		cJSON * payload = cJSON_GetObjectItem(json, "payload");
		cJSON * url = cJSON_GetObjectItem(payload, "url");
		cJSON * md51 = cJSON_GetObjectItem(payload, "md51");
		cJSON * md52 = cJSON_GetObjectItem(payload, "md52");
		rt_kprintf("%s\n", url->valuestring);
		rt_kprintf("%s\n", md51->valuestring);
		rt_kprintf("%s\n", md52->valuestring);
		sprintf(fmd51, "%s", md51->valuestring);
		fmd51[32] = 0;
		sprintf(fmd52, "%s", md52->valuestring);
		fmd52[32] = 0;
		
		memset(update_url, 0x00, sizeof(update_url));
		rt_strcpy(update_url, url->valuestring);
		download_file = 1;
	}
	else if((type != NULL)&&(0 == strcasecmp("canlist", type->valuestring)))	//can cfg packet
	{
		cJSON * payload = cJSON_GetObjectItem(json, "payload");
		{
			rt_list_t can_list = RT_LIST_OBJECT_INIT(can_list);
			rt_list_init(&can_list);
			cJSON * nodelist = cJSON_GetObjectItem(payload, "nodelist");
			int index = 0;
			index = parse_child(nodelist, &can_list, index);
			char * cjson_str = cJSON_Print(payload);
			//write cancfg msg to flash file
			int fd = open("/cancfg.msg", O_WRONLY | O_CREAT | O_TRUNC);
			write(fd, cjson_str, strlen(cjson_str));
			close(fd);
			
			print_tree(&can_list);
			client_nodes_cnt = index;
			disp_node_page_idx = 1;
			disp_node_page_cnt = (client_nodes_cnt+7)/8;
			disp_node_page_update = 1;
			disp_settings_update = 1;
			save_nodelist_req = 1;
		}
	}
	if((type != NULL)&&(0 == strcasecmp("reject", type->valuestring)))	//not register
	{
		mqtt_stop_req = 1;
	}
	
	set_timestamp(resp_ts->valueint);
	time_sync_req = 1;
	cJSON_Delete(json);
}

int main(void)
{
	udid[0] = HAL_GetUIDw0();
	udid[1] = HAL_GetUIDw1();
	udid[2] = HAL_GetUIDw2();
	
	i2c_dev = at24cxx_init("i2c1", 0);
	
	while(RT_EOK != at24cxx_check(i2c_dev))
		rt_thread_mdelay(100);
	
	i2c_check_ok = 1;
	
	if(dfs_mount("W25Q256", "/", "elm", 0, 0) != 0)
	{
		rt_kprintf("spi flash mount failed\n");
		if(dfs_mkfs("elm", "W25Q256") == 0)
		{
			rt_kprintf("spi flash formated\n");
			dfs_mount("W25Q256", "/", "elm", 0, 0);
		}
	}
	
	//load settings here
	memset(client_nodes, 0x00, sizeof(client_nodes));
	
	get_settings();
	if(settings.use4g == 0)
	{
		wiz_init();
	}
	
	disp_settings_update = 1;
	
	//make_settings_packet();
	
	//make_voltage_packet();
	
//	make_gnd_packet();
//	
//	
//	while(mqtt_online == 0)
//	{
//		rt_thread_mdelay(1000);
//	}
//	rt_thread_mdelay(2000);
//	make_settings_packet();
//	paho_mqtt_publish(&client, QOS1, MQTT_PUBTOPIC, (char *)mqtt_send_buf, strlen((char *)mqtt_send_buf));
//	
////	rt_thread_mdelay(2000);
////	make_voltage_packet();
////	paho_mqtt_publish(&client, QOS1, MQTT_PUBTOPIC, (char *)mqtt_send_buf, strlen((char *)mqtt_send_buf));
////	
////	rt_thread_mdelay(2000);
////	make_gnd_packet();
////	paho_mqtt_publish(&client, QOS1, MQTT_PUBTOPIC, (char *)mqtt_send_buf, strlen((char *)mqtt_send_buf));
	
	rt_thread_mdelay(4000);
	//cmux_sample();
	//error_wave_load = 1;
	over_error_wave_load = 1;
	client_node_load = 1;
	rt_thread_mdelay(1000);
	if(settings.use4g == 1)
	{
		ppp_sample_start();
	}
	
//	rt_list_t can_list = RT_LIST_OBJECT_INIT(can_list);
//	rt_list_init(&can_list);
//	cJSON* can_list_json_obj = cJSON_Parse(can_list_json);
//	cJSON * nodelist = cJSON_GetObjectItem(can_list_json_obj, "can_list");
//	int index = parse_child(nodelist, &can_list, index);
//	char * cjson_str = cJSON_Print(nodelist);
//	//write cancfg msg to flash file
//	print_tree(&can_list);
//	client_nodes_cnt = index;
//	disp_node_page_idx = 1;
//	disp_node_page_cnt = (client_nodes_cnt+7)/8;
//	disp_node_page_update = 1;
//	disp_settings_update = 1;
//	save_nodelist_req = 1;
	
	mb_slave_sample();
	//mqtt_start();
	while (1)
	{
		rt_thread_mdelay(500);
		if((ui_loaded == 1)&&(mqtt_online == 1))
		{
			if(login == 0)
			{
				rt_kprintf("upload login packet \n");
				make_login_packet();
				rt_kprintf("pkt len %d\n", strlen((char *)mqtt_send_buf));
				paho_mqtt_publish(&client, QOS1, MQTT_PUBTOPIC, (char *)mqtt_send_buf, strlen((char *)mqtt_send_buf));
				rt_thread_mdelay(5000);
			}
			else if(gnd_error_wave_upload == 1)
			{
				gnd_error_wave_upload = 0;
				rt_kprintf("upload gnd packet \n");
				make_gnd_packet();
				rt_kprintf("pkt len %d\n", strlen((char *)mqtt_send_buf));
				if(PAHO_SUCCESS == paho_mqtt_publish(&client, QOS1, MQTT_PUBTOPIC, (char *)mqtt_send_buf, strlen((char *)mqtt_send_buf)))
				{
					rt_kprintf("pub sended \n");
				}
				else
				{
					rt_kprintf("pub failed \n");
				}
			}
			else if(voltage_error_wave_upload == 1)
			{
				voltage_error_wave_upload = 0;
				rt_kprintf("upload voltage packet \n");
				make_voltage_packet();
				rt_kprintf("pkt len %d\n", strlen((char *)mqtt_send_buf));
				if(PAHO_SUCCESS == paho_mqtt_publish(&client, QOS1, MQTT_PUBTOPIC, (char *)mqtt_send_buf, strlen((char *)mqtt_send_buf)))
				{
					rt_kprintf("pub sended \n");
				}
				else
				{
					rt_kprintf("pub failed \n");
				}
			}
			else if(client_online_changed == 1)
			{
				client_online_changed = 0;
				rt_kprintf("upload status packet \n");
				make_status_packet();
				rt_kprintf("pkt len %d\n", strlen((char *)mqtt_send_buf));
				if(PAHO_SUCCESS == paho_mqtt_publish(&client, QOS1, MQTT_PUBTOPIC, (char *)mqtt_send_buf, strlen((char *)mqtt_send_buf)))
				{
					rt_kprintf("pub status sended \n");
				}
				else
				{
					rt_kprintf("pub status failed \n");
				}
			}
			else if(settings_upload_req == 1)
			{
				settings_upload_req = 0;
				rt_kprintf("upload settings packet \n");
				make_settings_packet();
				rt_kprintf("pkt len %d\n", strlen((char *)mqtt_send_buf));
				if(PAHO_SUCCESS == paho_mqtt_publish(&client, QOS1, MQTT_PUBTOPIC, (char *)mqtt_send_buf, strlen((char *)mqtt_send_buf)))
				{
					rt_kprintf("pub sended \n");
				}
				else
				{
					rt_kprintf("pub failed \n");
				}
			}
		}
		if(mqtt_sub_ava == 1)
		{
			mqtt_sub_ava = 0;
			login = 1;
			deal_substr();
		}
		if((ui_loaded == 1)&&(mqtt_start_req == 1))
		{
			mqtt_start_req = 0;
			mqtt_start();
		}
		if((ui_loaded == 1)&&(mqtt_stop_req == 1))
		{
			mqtt_stop_req = 0;
			paho_mqtt_stop(&client);
		}
	}
}

static void mqtt_new_sub_callback(mqtt_client *client, message_data *msg_data)
{
    *((char *)msg_data->message->payload + msg_data->message->payloadlen) = '\0';
    LOG_D("mqtt new subscribe callback: %.*s %.*s",
               msg_data->topic_name->lenstring.len,
               msg_data->topic_name->lenstring.data,
               msg_data->message->payloadlen,
               (char *)msg_data->message->payload);
}

static void mqtt_sub_callback(mqtt_client *c, message_data *msg_data)
{
    *((char *)msg_data->message->payload + msg_data->message->payloadlen) = '\0';
		memcpy(mqtt_sub_buf, msg_data->message->payload, msg_data->message->payloadlen);
		mqtt_sub_buf[msg_data->message->payloadlen] = 0;
		mqtt_sub_ava = 1;
    LOG_D("mqtt sub callback: %.*s %.*s",
               msg_data->topic_name->lenstring.len,
               msg_data->topic_name->lenstring.data,
               msg_data->message->payloadlen,
               (char *)msg_data->message->payload);
}

static void mqtt_sub_default_callback(mqtt_client *c, message_data *msg_data)
{
    *((char *)msg_data->message->payload + msg_data->message->payloadlen) = '\0';
    LOG_D("mqtt sub default callback: %.*s %.*s",
               msg_data->topic_name->lenstring.len,
               msg_data->topic_name->lenstring.data,
               msg_data->message->payloadlen,
               (char *)msg_data->message->payload);
}

static void mqtt_connect_callback(mqtt_client *c)
{
    LOG_D("inter mqtt_connect_callback!");
}

static void mqtt_online_callback(mqtt_client *c)
{
	LOG_D("inter mqtt_online_callback!");
	mqtt_online = 1;
	//paho_mqtt_publish(&client, QOS1, MQTT_PUBTOPIC, (char *)mqtt_send_buf, strlen((char *)mqtt_send_buf));
	//paho_mqtt_subscribe(&client, QOS1, MQTT_SUBTOPIC, mqtt_new_sub_callback);
}

static void mqtt_offline_callback(mqtt_client *c)
{
    LOG_D("inter mqtt_offline_callback!");
	mqtt_online = 0;
	login = 0;
}

static int mqtt_start(void)
{
    /* init condata param by using MQTTPacket_connectData_initializer */
    MQTTPacket_connectData condata = MQTTPacket_connectData_initializer;
    static char cid[20] = { 0 };
		static char str[32] = {0};
		static char substr[64] = {0};

    if (is_started)
    {
        LOG_E("mqtt client is already connected.");
        return -1;
    }
    /* config MQTT context param */
    {
        client.isconnected = 0;
				sprintf(str, "tcp://%s:%d", settings.server_addr, settings.server_port);
        client.uri = str;

        /* generate the random client ID */
        //rt_snprintf(cid, sizeof(cid), "rtthread%d", rt_tick_get());
        /* config connect param */
        memcpy(&client.condata, &condata, sizeof(condata));
				udid[0] = HAL_GetUIDw0();
				udid[1] = HAL_GetUIDw1();
				udid[2] = HAL_GetUIDw2();
				sprintf(cid, "ERIC1820%08x", udid[2]^udid[1]^udid[0]);
				for(int i=0;i<strlen(cid);i++)
				{
					if (cid[i] >= 'a' && cid[i] <= 'z')
						cid[i] = toupper(cid[i]);
				}
				sprintf(substr, "/%s/sub", cid);
        client.condata.clientID.cstring = cid;
        client.condata.username.cstring = "admin";
        client.condata.password.cstring = "123456";
        client.condata.keepAliveInterval = 30;
        client.condata.cleansession = 1;

        /* config MQTT will param. */
        client.condata.willFlag = 1;
        client.condata.will.qos = 1;
        client.condata.will.retained = 0;
        client.condata.will.topicName.cstring = MQTT_PUBTOPIC;
        client.condata.will.message.cstring = MQTT_WILLMSG;

        /* malloc buffer. */
        client.buf_size = client.readbuf_size = 1024*1024;
        client.buf = rt_calloc(1, client.buf_size);
        client.readbuf = rt_calloc(1, client.readbuf_size);
        if (!(client.buf && client.readbuf))
        {
            LOG_E("no memory for MQTT client buffer!");
            return -1;
        }

        /* set event callback function */
        client.connect_callback = mqtt_connect_callback;
        client.online_callback = mqtt_online_callback;
        client.offline_callback = mqtt_offline_callback;

        /* set subscribe table and event callback */
        client.message_handlers[0].topicFilter = rt_strdup(substr);
        client.message_handlers[0].callback = mqtt_sub_callback;
        client.message_handlers[0].qos = QOS1;

        /* set default subscribe event callback */
        client.default_message_handlers = mqtt_sub_default_callback;
    }
    
    {
      int value;
      uint16_t u16Value;
      value = 5;
      paho_mqtt_control(&client, MQTT_CTRL_SET_CONN_TIMEO, &value);
      value = 5;
      paho_mqtt_control(&client, MQTT_CTRL_SET_MSG_TIMEO, &value);
      value = 5;
      paho_mqtt_control(&client, MQTT_CTRL_SET_RECONN_INTERVAL, &value);
      value = 30;
      paho_mqtt_control(&client, MQTT_CTRL_SET_KEEPALIVE_INTERVAL, &value);
      u16Value = 3;
      paho_mqtt_control(&client, MQTT_CTRL_SET_KEEPALIVE_COUNT, &u16Value);
    }

    /* run mqtt client */
    paho_mqtt_start(&client, 8196, 21);
    is_started = 1;

    return 0;
}
#define BASE64_TABLE "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
uint16_t base64_encode(const char * input, int length, char * output) {
    int encoded_len = 4 * ((length + 2) / 3);
    int i, j = 0;
 
    for (i = 0; i < length; i += 3) {
        int val = (input[i] << 16);
        if (i + 1 < length) {
            val |= (input[i + 1] << 8);
        }
        if (i + 2 < length) {
            val |= input[i + 2];
        }
				
        output[j++] = BASE64_TABLE[(val >> 18) & 0x3F];
        output[j++] = BASE64_TABLE[(val >> 12) & 0x3F];
        output[j++] = (i + 1 < length) ? BASE64_TABLE[(val >> 6) & 0x3F] : '=';
        output[j++] = (i + 2 < length) ? BASE64_TABLE[val & 0x3F] : '=';
    }
    output[j] = '\0';
    return encoded_len;
}
void ADC_GPIO_EXTI_Callback(void)
{
}
void SPI_GPIO_EXTI_Callback(void)
{
}
void RST_GPIO_EXTI_Callback(void)
{
}
