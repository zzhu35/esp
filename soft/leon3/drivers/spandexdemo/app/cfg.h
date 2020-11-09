#ifndef __ESP_CFG_000_H__
#define __ESP_CFG_000_H__

#include "libesp.h"

typedef int32_t token_t;

/* <<--params-def-->> */
#define BASE_ADDR 0
#define OWNER 0
#define OWNER_PRED 0
#define STRIDE_SIZE 0
#define COH_MSG 0
#define ARRAY_LENGTH 1
#define REQ_TYPE 0
#define ELEMENT_SIZE 1

/* <<--params-->> */
const int32_t base_addr = BASE_ADDR;
const int32_t owner = OWNER;
const int32_t owner_pred = OWNER_PRED;
const int32_t stride_size = STRIDE_SIZE;
const int32_t coh_msg = COH_MSG;
const int32_t array_length = ARRAY_LENGTH;
const int32_t req_type = REQ_TYPE;
const int32_t element_size = ELEMENT_SIZE;

#define NACC 1

esp_thread_info_t cfg_000[] = {
	{
		.run = true,
		.devname = "spandexdemo.0",
		.type = spandexdemo,
		/* <<--descriptor-->> */
		.desc.spandexdemo_desc.base_addr = BASE_ADDR,
		.desc.spandexdemo_desc.owner = OWNER,
		.desc.spandexdemo_desc.owner_pred = OWNER_PRED,
		.desc.spandexdemo_desc.stride_size = STRIDE_SIZE,
		.desc.spandexdemo_desc.coh_msg = COH_MSG,
		.desc.spandexdemo_desc.array_length = ARRAY_LENGTH,
		.desc.spandexdemo_desc.req_type = REQ_TYPE,
		.desc.spandexdemo_desc.element_size = ELEMENT_SIZE,
		.desc.spandexdemo_desc.src_offset = 0,
		.desc.spandexdemo_desc.dst_offset = 0,
		.desc.spandexdemo_desc.esp.coherence = ACC_COH_NONE,
		.desc.spandexdemo_desc.esp.p2p_store = 0,
		.desc.spandexdemo_desc.esp.p2p_nsrcs = 0,
		.desc.spandexdemo_desc.esp.p2p_srcs = {"", "", "", ""},
	}
};

#endif /* __ESP_CFG_000_H__ */
