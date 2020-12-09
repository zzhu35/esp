/* Copyright (c) 2011-2019 Columbia University, System Level Design Group */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#ifndef __riscv
#include <stdlib.h>
#endif

#include <esp_accelerator.h>
#include <esp_probe.h>

typedef int32_t token_t;

static unsigned DMA_WORD_PER_BEAT(unsigned _st)
{
        return (sizeof(void *) / _st);
}


#define SLD_SPANDEXDEMO 0x058
#define DEV_NAME "sld,spandexdemo"

/* <<--params-->> */
const int32_t base_addr = 0x1000;
const int32_t owner = 0;
const int32_t owner_pred = 0;
const int32_t stride_size = 0;
const int32_t coh_msg = 0;
const int32_t array_length = 1;
const int32_t req_type = 0;
const int32_t element_size = 1;

static unsigned in_words_adj;
static unsigned out_words_adj;
static unsigned in_len;
static unsigned out_len;
static unsigned in_size;
static unsigned out_size;
static unsigned out_offset;
static unsigned mem_size;

/* Size of the contiguous chunks for scatter/gather */
#define CHUNK_SHIFT 20
#define CHUNK_SIZE BIT(CHUNK_SHIFT)
#define NCHUNK(_sz) ((_sz % CHUNK_SIZE == 0) ?		\
			(_sz / CHUNK_SIZE) :		\
			(_sz / CHUNK_SIZE) + 1)

/* User defined registers */
/* <<--regs-->> */
#define SPANDEXDEMO_BASE_ADDR_REG 0x5c
#define SPANDEXDEMO_OWNER_REG 0x58
#define SPANDEXDEMO_OWNER_PRED_REG 0x54
#define SPANDEXDEMO_STRIDE_SIZE_REG 0x50
#define SPANDEXDEMO_COH_MSG_REG 0x4c
#define SPANDEXDEMO_ARRAY_LENGTH_REG 0x48
#define SPANDEXDEMO_REQ_TYPE_REG 0x44
#define SPANDEXDEMO_ELEMENT_SIZE_REG 0x40


static int validate_buf(token_t *out, token_t *gold)
{
	int i;
	int j;
	unsigned errors = 0;

	for (i = 0; i < 1; i++)
		for (j = 0; j < element_size; j++)
			if (gold[i * out_words_adj + j] != out[i * out_words_adj + j])
				errors++;

	return errors;
}


static void init_buf (token_t *in, token_t * gold)
{
	int i;
	int j;

	for (i = 0; i < 1; i++)
		for (j = 0; j < element_size; j++)
			in[i * in_words_adj + j] = (token_t) j;

	for (i = 0; i < 1; i++)
		for (j = 0; j < element_size; j++)
			gold[i * out_words_adj + j] = (token_t) j;
}


int main(int argc, char * argv[])
{
	int i;
	int n;
	int ndev;
	struct esp_device espdevs;
	struct esp_device *dev;
	unsigned done;
	unsigned **ptable;
	token_t *mem;
	token_t *gold;
	unsigned errors = 0;

	if (DMA_WORD_PER_BEAT(sizeof(token_t)) == 0) {
		in_words_adj = element_size;
		out_words_adj = element_size;
	} else {
		in_words_adj = round_up(element_size, DMA_WORD_PER_BEAT(sizeof(token_t)));
		out_words_adj = round_up(element_size, DMA_WORD_PER_BEAT(sizeof(token_t)));
	}
	in_len = in_words_adj * (1);
	out_len = out_words_adj * (1);
	in_size = in_len * sizeof(token_t);
	out_size = out_len * sizeof(token_t);
	out_offset  = in_len;
	mem_size = (out_offset * sizeof(token_t)) + out_size;


	// Search for the device
	printf("Scanning device tree... \n");

	ndev = 1;
	espdevs.vendor = 235;
	espdevs.id = 88;
	espdevs.number = 2147626072;
	espdevs.irq = 6;
	espdevs.addr = 0x60011400;
	espdevs.compat = 1;
	printf("fast probe done\n");
	if (ndev == 0) {
		printf("spandexdemo not found\n");
		return 0;
	}

	for (n = 0; n < ndev; n++) {

		dev = &espdevs;

		// Check DMA capabilities
		if (ioread32(dev, PT_NCHUNK_MAX_REG) == 0) {
			printf("  -> scatter-gather DMA is disabled. Abort.\n");
			return 0;
		}

		if (ioread32(dev, PT_NCHUNK_MAX_REG) < NCHUNK(mem_size)) {
			printf("  -> Not enough TLB entries available. Abort.\n");
			return 0;
		}

		// Allocate memory
		gold = aligned_malloc(out_size);
		mem = aligned_malloc(mem_size);
		printf("  memory buffer base-address = %p\n", mem);

		// Alocate and populate page table
		ptable = aligned_malloc(NCHUNK(mem_size) * sizeof(unsigned *));
		for (i = 0; i < NCHUNK(mem_size); i++)
			ptable[i] = (unsigned *) &mem[i * (CHUNK_SIZE / sizeof(token_t))];

		printf("  ptable = %p\n", ptable);
		printf("  nchunk = %lu\n", NCHUNK(mem_size));

		printf("  Generate input...\n");
		init_buf(mem, gold);

		// Pass common configuration parameters

		iowrite32(dev, SELECT_REG, ioread32(dev, DEVID_REG));
		iowrite32(dev, COHERENCE_REG, ACC_COH_FULL);

#ifndef __sparc
		iowrite32(dev, PT_ADDRESS_REG, (unsigned long long) ptable);
#else
		iowrite32(dev, PT_ADDRESS_REG, (unsigned) ptable);
#endif
		iowrite32(dev, PT_NCHUNK_REG, NCHUNK(mem_size));
		iowrite32(dev, PT_SHIFT_REG, CHUNK_SHIFT);

		// Use the following if input and output data are not allocated at the default offsets
		iowrite32(dev, SRC_OFFSET_REG, 0);
		iowrite32(dev, DST_OFFSET_REG, 0);

		// Pass accelerator-specific configuration parameters
		/* <<--regs-config-->> */
		iowrite32(dev, SPANDEXDEMO_BASE_ADDR_REG, base_addr);
		iowrite32(dev, SPANDEXDEMO_OWNER_REG, owner);
		iowrite32(dev, SPANDEXDEMO_OWNER_PRED_REG, owner_pred);
		iowrite32(dev, SPANDEXDEMO_STRIDE_SIZE_REG, stride_size);
		iowrite32(dev, SPANDEXDEMO_COH_MSG_REG, coh_msg);
		iowrite32(dev, SPANDEXDEMO_ARRAY_LENGTH_REG, array_length);
		iowrite32(dev, SPANDEXDEMO_REQ_TYPE_REG, req_type);
		iowrite32(dev, SPANDEXDEMO_ELEMENT_SIZE_REG, element_size);

		// Flush (customize coherence model here)
		// esp_flush(ACC_COH_FULL);

		// Start accelerators
		printf("  Start...\n");
		iowrite32(dev, CMD_REG, CMD_MASK_START);

		// Wait for completion
		done = 0;
		while (!done) {
			done = ioread32(dev, STATUS_REG);
			done &= STATUS_MASK_DONE;
		}
		iowrite32(dev, CMD_REG, 0x0);

		printf("  Done\n");
		printf("  validating...\n");

		/* Validation */
		errors = validate_buf(&mem[out_offset], gold);
		if (errors)
			printf("  ... FAIL\n");
		else
			printf("  ... PASS\n");

		aligned_free(ptable);
		aligned_free(mem);
		aligned_free(gold);
	}

	return 0;
}
