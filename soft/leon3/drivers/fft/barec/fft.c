/* Copyright (c) 2011-2019 Columbia University, System Level Design Group */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <stdlib.h>

#include <esp_accelerator.h>
#include <esp_probe.h>
#include "test/fft_test.h"

// FFT
#if (FFT_FX_WIDTH == 64)
typedef long long token_t;
typedef double native_t;
#define fx2float fixed64_to_double
#define float2fx double_to_fixed64
#define FX_IL 42
#elif (FFT_FX_WIDTH == 32)
typedef int token_t;
typedef float native_t;
#define fx2float fixed32_to_float
#define float2fx float_to_fixed32
#define FX_IL 12
#endif /* FFT_FX_WIDTH */

const float ERR_TH = 0.05;

static unsigned DMA_WORD_PER_BEAT(unsigned _st)
{
        return (sizeof(void *) / _st);
}


#define SLD_FFT 0x059
#define DEV_NAME_FFT "sld,fft"

/* <<--params-->> */
const int32_t log_len = 3;
int32_t len;
int32_t do_bitrev = 1;

static unsigned in_words_adj;
static unsigned out_words_adj;
static unsigned in_len;
static unsigned out_len;
static unsigned in_size;
static unsigned out_size;
static unsigned out_offset;
static unsigned mem_size;

/* Size of the contiguous chunks for scatter/gather */
#define CHUNK_SHIFT_FFT 20
#define CHUNK_SIZE_FFT BIT(CHUNK_SHIFT_FFT)
#define NCHUNK_FFT(_sz) ((_sz % CHUNK_SIZE_FFT == 0) ?		\
			(_sz / CHUNK_SIZE_FFT) :		\
			(_sz / CHUNK_SIZE_FFT) + 1)

/* User defined registers */
/* <<--regs-->> */
#define FFT_DO_PEAK_REG 0x48
#define FFT_DO_BITREV_REG 0x44
#define FFT_LOG_LEN_REG 0x40


static int validate_buf(token_t *out, float *gold)
{
	int j;
	unsigned errors = 0;

	for (j = 0; j < 2 * len; j++) {
		native_t val = fx2float(out[j], FX_IL);
		if ((fabs(gold[j] - val) / fabs(gold[j])) > ERR_TH)
			errors++;
	}

	//printf("  + Relative error > %.02f for %d output values out of %ld\n", ERR_TH, errors, 2 * len);
	printf("  + Relative error > 0.05 for %d output values out of %ld\n", errors, 2 * len);
	return errors;
}


static void init_buf_fft(token_t *in, float *gold)
{
	int j;
	const float LO = -10.0;
	const float HI = 10.0;

	/* srand((unsigned int) time(NULL)); */

	for (j = 0; j < 2 * len; j++) {
		float scaling_factor = (float) rand_fft () / (float) RAND_MAX_FFT;
		gold[j] = LO + scaling_factor * (HI - LO);
	}

	// preprocess with bitreverse (fast in software anyway)
	if (!do_bitrev)
		fft_bit_reverse(gold, len, log_len);

	// convert input to fixed point
	for (j = 0; j < 2 * len; j++)
		in[j] = float2fx((native_t) gold[j], FX_IL);


	// Compute golden output
	fft_comp(gold, len, log_len, -1, do_bitrev);
}

//--------------------------------------------------------------
// SORT
#define SLD_SORT   0x0B
#define DEV_NAME_SORT "sld,sort"

#define SORT_LEN 64
#define SORT_BATCH 2

#define SORT_BUF_SIZE (SORT_LEN * SORT_BATCH * sizeof(unsigned))

/* Size of the contiguous chunks for scatter/gather */
#define CHUNK_SHIFT_SORT 7
#define CHUNK_SIZE_SORT BIT(CHUNK_SHIFT_SORT)
#define NCHUNK_SORT ((SORT_BUF_SIZE % CHUNK_SIZE_SORT == 0) ?			\
			(SORT_BUF_SIZE / CHUNK_SIZE_SORT) :			\
			(SORT_BUF_SIZE / CHUNK_SIZE_SORT) + 1)

// User defined registers
#define SORT_LEN_REG		0x40
#define SORT_BATCH_REG		0x44
#define SORT_LEN_MIN_REG	0x48
#define SORT_LEN_MAX_REG	0x4c
#define SORT_BATCH_MAX_REG	0x50


static int validate_sorted(float *array, int len)
{
	int i;
	int rtn = 0;
	for (i = 1; i < len; i++)
		if (array[i] < array[i-1])
			rtn++;
	return rtn;
}

static void init_buf_sort (float *buf, unsigned sort_size, unsigned sort_batch)
{
	int i, j;
	printf("  Generate random input...\n");

	/* srand(time(NULL)); */
	for (j = 0; j < sort_batch; j++)
		for (i = 0; i < sort_size; i++) {
			/* TAV rand between 0 and 1 */
#ifndef __riscv
			buf[sort_size * j + i] = ((float) rand () / (float) RAND_MAX);
#else
			buf[sort_size * j + i] = 1.0 / ((float) i + 1);
#endif
			/* /\* More general testbench *\/ */
			/* float M = 100000.0; */
			/* buf[sort_size * j + i] =  M * ((float) rand() / (float) RAND_MAX) - M/2; */
			/* /\* Easyto debug...! *\/ */
			/* buf[sort_size * j + i] = (float) (sort_size - i);; */
		}
}


int main(int argc, char * argv[])
{
	int n;
	int ndev;
	struct esp_device *espdevs = NULL;
	unsigned coherence;

	ndev = probe(&espdevs, SLD_SORT, DEV_NAME_SORT);
	if (!ndev) {
		printf("Error: %s device not found!\n", DEV_NAME_SORT);
		exit(EXIT_FAILURE);
	}

	printf("Test parameters: [LEN, BATCH] = [%d, %d]\n\n", SORT_LEN, SORT_BATCH);
	/* TODO: Restore full test once ESP caches are integrated */
	coherence = ACC_COH_FULL;
	struct esp_device *dev = &espdevs[0];
	unsigned sort_batch_max;
	unsigned sort_len_max;
	unsigned sort_len_min;
	unsigned done;
	int i, j;
	unsigned **ptable;
	unsigned *mem;
	unsigned errors = 0;
	int scatter_gather = 1;

	sort_batch_max = ioread32(dev, SORT_BATCH_MAX_REG);
	sort_len_min = ioread32(dev, SORT_LEN_MIN_REG);
	sort_len_max = ioread32(dev, SORT_LEN_MAX_REG);

	printf("******************** %s.%d ********************\n", DEV_NAME_SORT, n);
	// Check access ok
	if (SORT_LEN < sort_len_min ||
		SORT_LEN > sort_len_max ||
		SORT_BATCH < 1 ||
		SORT_BATCH > sort_batch_max) {
		printf("  Error: unsopported configuration parameters for %s.%d\n", DEV_NAME_SORT, n);
		printf("         device can sort up to %d fp-vectors of size [%d, %d]\n",
			sort_batch_max, sort_len_min, sort_len_max);
		return 0;
	}

	// Check if scatter-gather DMA is disabled
	if (ioread32(dev, PT_NCHUNK_MAX_REG) == 0) {
		printf("  -> scatter-gather DMA is disabled. Abort.\n");
		scatter_gather = 0;
	}

	if (scatter_gather)
		if (ioread32(dev, PT_NCHUNK_MAX_REG) < NCHUNK_SORT) {
			printf("  -> Not enough TLB entries available. Abort.\n");
			return 0;
		}

	// Allocate memory (will be contigous anyway in baremetal)
	mem = aligned_malloc(SORT_BUF_SIZE);

	printf("  memory buffer base-address = %p\n", mem);

	if (scatter_gather) {
		//Alocate and populate page table
		ptable = aligned_malloc(NCHUNK_SORT * sizeof(unsigned *));
		for (i = 0; i < NCHUNK_SORT; i++)
			ptable[i] = (unsigned *) &mem[i * (CHUNK_SIZE_SORT / sizeof(unsigned))];

		printf("  ptable = %p\n", ptable);
		printf("  nchunk = %lu\n", NCHUNK_SORT);
	}

	// Initialize input: write floating point hex values (simpler to debug)
	init_buf_sort((float *) mem, SORT_LEN, SORT_BATCH);

	// Configure device
	iowrite32(dev, SELECT_REG, ioread32(dev, DEVID_REG));
	iowrite32(dev, COHERENCE_REG, coherence);

	if (scatter_gather) {
		iowrite32(dev, PT_ADDRESS_REG, (unsigned long) ptable);
		iowrite32(dev, PT_NCHUNK_REG, NCHUNK_SORT);
		iowrite32(dev, PT_SHIFT_REG, CHUNK_SHIFT_SORT);
		iowrite32(dev, SRC_OFFSET_REG, 0);
		iowrite32(dev, DST_OFFSET_REG, 0); // Sort runs in place
	} else {
		iowrite32(dev, SRC_OFFSET_REG, (unsigned long) mem);
		iowrite32(dev, DST_OFFSET_REG, (unsigned long) mem); // Sort runs in place
	}
	iowrite32(dev, SORT_LEN_REG, SORT_LEN);
	iowrite32(dev, SORT_BATCH_REG, SORT_BATCH);

	// ****************************************************************************************************
	// FFT

	int ndev_fft;
	unsigned done_fft;
	token_t *mem_fft;
	float *gold;
    const int ERROR_COUNT_TH = 0.001;

	len = 1 << log_len;

	if (DMA_WORD_PER_BEAT(sizeof(token_t)) == 0) {
		in_words_adj = 2 * len;
		out_words_adj = 2 * len;
	} else {
		in_words_adj = round_up(2 * len, DMA_WORD_PER_BEAT(sizeof(token_t)));
		out_words_adj = round_up(2 * len, DMA_WORD_PER_BEAT(sizeof(token_t)));
	}
	in_len = in_words_adj;
	out_len = out_words_adj;
	in_size = in_len * sizeof(token_t);
	out_size = out_len * sizeof(token_t);
	out_offset  = 0;
	mem_size = (out_offset * sizeof(token_t)) + out_size;


	// Search for the device
	printf("Scanning device tree... \n");

	ndev_fft = probe(&espdevs, SLD_FFT, DEV_NAME_FFT);
	if (ndev_fft == 0) {
		printf("fft not found\n");
		return 0;
	}

	struct esp_device *dev_fft = &espdevs[0];

	// Check DMA capabilities
	if (ioread32(dev_fft, PT_NCHUNK_MAX_REG) == 0) {
		printf("  -> scatter-gather DMA is disabled. Abort.\n");
		return 0;
	}

	if (ioread32(dev_fft, PT_NCHUNK_MAX_REG) < NCHUNK_FFT(mem_size)) {
		printf("  -> Not enough TLB entries available. Abort.\n");
		return 0;
	}

	// Allocate memory
	gold = aligned_malloc(out_len * sizeof(float));
	mem_fft = aligned_malloc(mem_size);
	printf("  memory buffer base-address = %p\n", mem_fft);

	// Alocate and populate page table
	ptable = aligned_malloc(NCHUNK_FFT(mem_size) * sizeof(unsigned *));
	for (i = 0; i < NCHUNK_FFT(mem_size); i++)
		ptable[i] = (unsigned *) &mem_fft[i * (CHUNK_SIZE_FFT / sizeof(token_t))];

	printf("  ptable = %p\n", ptable);
	printf("  nchunk = %lu\n", NCHUNK_FFT(mem_size));

	printf("  Generate input...\n");
	init_buf_fft(mem_fft, gold);

	// Pass common configuration parameters

	iowrite32(dev_fft, SELECT_REG, ioread32(dev_fft, DEVID_REG));
	iowrite32(dev_fft, COHERENCE_REG, ACC_COH_FULL);

	iowrite32(dev_fft, PT_ADDRESS_REG, (unsigned long long) ptable);

	iowrite32(dev_fft, PT_NCHUNK_REG, NCHUNK_FFT(mem_size));
	iowrite32(dev_fft, PT_SHIFT_REG, CHUNK_SHIFT_FFT);

	// Use the following if input and output data are not allocated at the default offsets
	iowrite32(dev_fft, SRC_OFFSET_REG, 0x0);
	iowrite32(dev_fft, DST_OFFSET_REG, 0x0);

	// Pass accelerator-specific configuration parameters
	/* <<--regs-config-->> */
	iowrite32(dev_fft, FFT_DO_PEAK_REG, 0);
	iowrite32(dev_fft, FFT_DO_BITREV_REG, do_bitrev);
	iowrite32(dev_fft, FFT_LOG_LEN_REG, log_len);

	// Start accelerator
	printf("  Sort Start..\n");
	iowrite32(dev, CMD_REG, CMD_MASK_START);

	// Start accelerators
	printf("  FFT Start...\n");
	iowrite32(dev_fft, CMD_REG, CMD_MASK_START);

	// Wait for completion
	done_fft = 0;
	while (!done_fft) {
		done_fft = ioread32(dev_fft, STATUS_REG);
		done_fft &= STATUS_MASK_DONE;
	}
	iowrite32(dev_fft, CMD_REG, 0x0);
	printf("  FFT Done\n");

	done = 0;
	while (!done) {
		done = ioread32(dev, STATUS_REG);
		done &= STATUS_MASK_DONE;
	}
	iowrite32(dev, CMD_REG, 0x0);
	printf("  Both Done\n");

	printf("  validating FFT...\n");

	/* Validation */
	errors = validate_buf(&mem_fft[out_offset], gold);
	if ((errors / len) > ERROR_COUNT_TH)
		printf("  ... FAIL: exceeding error count threshold\n");
	else
		printf("  ... PASS: not exceeding error count threshold\n");

	aligned_free(mem_fft);
	aligned_free(gold);


	// **************************************************************************************
	
	printf("  validating Sort...\n");
	errors = 0;
	for (j = 0; j < SORT_BATCH; j++) {
		int err = validate_sorted((float *) &mem[j * SORT_LEN], SORT_LEN);
		/* if (err != 0) */
		/* 	printf("  Error: %s.%d mismatch on batch %d\n", DEV_NAME_SORT, n, j); */
		errors += err;
	}
	if (errors)
		printf("  ... FAIL\n");
	else
		printf("  ... PASS\n");
	printf("**************************************************\n\n");

	if (scatter_gather)
		aligned_free(ptable);
	aligned_free(mem);

	return 0;
}
