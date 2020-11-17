/**
 * Baremetal device driver for DUMMY
 *
 * (point-to-point communication test)
 */

#include <stdio.h>
#ifndef __riscv
#include <stdlib.h>
#endif

#include <esp_accelerator.h>
#include <esp_probe.h>

typedef long long unsigned u64;
typedef unsigned u32;

typedef u64 token_t;
#define mask 0xFEED0BAC00000000LL

#define SLD_DUMMY   0x42
#define DEV_NAME "sld,dummy"

#define TOKENS 64
#define BATCH 4
const int32_t stride_size = 2;
const int32_t array_length = 8;
const int32_t req_type = 0;

static unsigned out_offset;
static unsigned size;

/* Size of the contiguous chunks for scatter/gather */
#define CHUNK_SHIFT 20
#define CHUNK_SIZE BIT(CHUNK_SHIFT)
#define NCHUNK ((size % CHUNK_SIZE == 0) ?			\
			(size / CHUNK_SIZE) :		\
			(size / CHUNK_SIZE) + 1)

// User defined registers
#define DUMMY_BASE_ADDR_REG 0x5c
#define DUMMY_OWNER_REG 0x58
#define DUMMY_OWNER_PRED_REG 0x54
#define DUMMY_STRIDE_SIZE_REG 0x50
#define DUMMY_COH_MSG_REG 0x4c
#define DUMMY_ARRAY_LENGTH_REG 0x48
#define DUMMY_REQ_TYPE_REG 0x44
#define DUMMY_ELEMENT_SIZE_REG 0x40


static int validate_dummy(token_t *mem)
{
	int i, j;
	int rtn = 0;
	for (j = 0; j < BATCH; j++)
		for (i = 0; i < TOKENS; i++)
			if (mem[i + j * TOKENS] != (mask | (token_t) i)) {
				printf("[%d, %d]: %llu\n", j, i, mem[i + j * TOKENS]);
				rtn++;
			}
	return rtn;
}

static void init_buf (token_t *mem)
{
	int i, j;
	for (j = 0; j < BATCH; j++)
		for (i = 0; i < TOKENS; i++)
			mem[i + j * TOKENS] = (mask | (token_t) i);

	for (i = 0; i < BATCH * TOKENS; i++)
		mem[i + BATCH * TOKENS] = 0xFFFFFFFFFFFFFFFFLL;
}


int main(int argc, char * argv[])
{
	int i;
	int n;
	int ndev;
	struct esp_device espdevs;
	struct esp_device *dev;
	struct esp_device *srcs[4];
	unsigned all_done;
	unsigned **ptable;
	token_t *mem;
	unsigned errors = 0;

	// out_offset = BATCH * TOKENS * sizeof(u64);
	out_offset = (stride_size) * array_length * sizeof(u64);
	size = 2 * out_offset;

	printf("Scanning device tree... \n");

	//ndev = probe(&espdevs, SLD_DUMMY, DEV_NAME);
	ndev = 1;
	espdevs.vendor = 235;
	espdevs.id = 88;
	espdevs.number = 2147626072;
	espdevs.irq = 6;
	espdevs.addr = 0x60011400;
	espdevs.compat = 1;
	printf("fast probe done\n");
	if (ndev < 1) {
		printf("This test requires a dummy device!\n");
		return 0;
	}

	// Check DMA capabilities
	dev = &espdevs;

	if (ioread32(dev, PT_NCHUNK_MAX_REG) == 0) {
		printf("  -> scatter-gather DMA is disabled. Abort.\n");
		return 0;
	}

	if (ioread32(dev, PT_NCHUNK_MAX_REG) < NCHUNK) {
		printf("  -> Not enough TLB entries available. Abort.\n");
		return 0;
	}

	// Allocate memory
	mem = aligned_malloc(size);
	printf("  memory buffer base-address = %p\n", mem);

	//Alocate and populate page table
	ptable = aligned_malloc(NCHUNK * sizeof(unsigned *));
	for (i = 0; i < NCHUNK; i++)
		ptable[i] = (unsigned *) &mem[i * (CHUNK_SIZE / sizeof(token_t))];
	printf("  ptable = %p\n", ptable);
	printf("  nchunk = %lu\n", NCHUNK);

	printf("  Generate random input...\n");
	init_buf(mem);

	iowrite32(dev, SELECT_REG, ioread32(dev, DEVID_REG));
	iowrite32(dev, COHERENCE_REG, ACC_COH_FULL);
	iowrite32(dev, PT_ADDRESS_REG, (unsigned long) ptable);
	iowrite32(dev, PT_NCHUNK_REG, NCHUNK);
	iowrite32(dev, PT_SHIFT_REG, CHUNK_SHIFT);
	iowrite32(dev, DUMMY_BASE_ADDR_REG, TOKENS);
	iowrite32(dev, DUMMY_OWNER_REG, BATCH);
	iowrite32(dev, DUMMY_STRIDE_SIZE_REG, stride_size);
	iowrite32(dev, DUMMY_ARRAY_LENGTH_REG, array_length);
	iowrite32(dev, DUMMY_REQ_TYPE_REG, req_type);
	iowrite32(dev, SRC_OFFSET_REG, 0x0);
	iowrite32(dev, DST_OFFSET_REG, out_offset);

	// Flush for non-coherent DMA
	// esp_flush(ACC_COH_NONE);

	// Start accelerators
	printf("  Start...\n");
	iowrite32(dev, CMD_REG, CMD_MASK_START);

	// Wait for completion
	all_done = 0;
	while (!all_done) {
		all_done = ioread32(dev, STATUS_REG);
		all_done &= STATUS_MASK_DONE;
	}

	iowrite32(dev, CMD_REG, 0x0);

	printf("  Done\n");

	/* Validation */
	printf("  validating...\n");
	errors = validate_dummy(&mem[BATCH * TOKENS]);

	if (errors)
		printf("  ... FAIL\n");
	else
		printf("  ... PASS\n");

	aligned_free(ptable);
	aligned_free(mem);

	return 0;
}

