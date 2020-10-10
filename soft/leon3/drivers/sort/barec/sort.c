/**
 * Baremetal device driver for SORT
 *
 * Select Scatter-Gather in ESP configuration
 */

#include <stdio.h>
#include <stdlib.h>
#include <esp_accelerator.h>
#include <esp_probe.h>

#define SLD_SORT   0x0B
#define DEV_NAME "sld,sort"

#define SORT_LEN 64
#define SORT_BATCH 4

#define SORT_BUF_SIZE (SORT_LEN * SORT_BATCH * sizeof(unsigned))

/* Size of the contiguous chunks for scatter/gather */
#define CHUNK_SHIFT 8
#define CHUNK_SIZE BIT(CHUNK_SHIFT)
#define NCHUNK ((SORT_BUF_SIZE % CHUNK_SIZE == 0) ?			\
			(SORT_BUF_SIZE / CHUNK_SIZE) :			\
			(SORT_BUF_SIZE / CHUNK_SIZE) + 1)

// User defined registers
#define SORT_LEN_REG		0x40
#define SORT_BATCH_REG		0x44
#define SORT_LEN_MIN_REG	0x48
#define SORT_LEN_MAX_REG	0x4c
#define SORT_BATCH_MAX_REG	0x50

#define PTR_COUNTER 0xb0000000

static void sync()
{
	// return; // if MESI, uncomment this
	volatile int tmp;
	volatile int* d = (volatile int*)&tmp;

	asm volatile (
	" amoswap.w.aq t0, t0, (%1);"
	: "=&r"(d)
	: "0" (d)
	: "memory", "t0"
	);

}

static inline long long unsigned int get_counter(){
	return *(volatile long long unsigned int*)PTR_COUNTER;
}

static inline void print_counter(){
	long long int data = get_counter();
	printf("count: %x", (int) (data >> 32 & 0xffffffff));
	printf("%x\n", (int) (data & 0xffffffff));
}


static int validate_sorted(float *array, int len)
{
	int i;
	int rtn = 0;
	for (i = 1; i < len; i++)
		if (array[i] < array[i-1])
			rtn++;
	return rtn;
}

static inline void init_buf (float *buf, unsigned sort_size, unsigned sort_batch)
{
	int i, j;

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
	// struct esp_device *espdevs = NULL;
	struct esp_device espdevs;
	unsigned coherence;
	long long unsigned int timestamps[6]; // get three intervals


	// ndev = probe(&espdevs, SLD_SORT, DEV_NAME);
	// printf("vendor:%u\nid:%u\nnumber:%u\nirq:%u\naddr:%lu\ncompat:%u\n",espdevs->vendor, espdevs->id,espdevs->number,espdevs->irq,espdevs->addr,espdevs->compat);
	ndev = 1;
	espdevs.vendor = 235;
	espdevs.id = 11;
	espdevs.number = 0;
	espdevs.irq = 6;
	espdevs.addr = 0x60011400;
	espdevs.compat = 1;
	printf("fast probe done\n");
	if (!ndev) {
		printf("Error: %s device not found!\n", DEV_NAME);
		exit(EXIT_FAILURE);
	}

	long long unsigned int a, b, c;
	a = 0; b = 0; c = 0;


	printf("Test parameters: [LEN, BATCH] = [%d, %d]\n\n", SORT_LEN, SORT_BATCH);
	n = 0;
	for (int iii = 0; iii < 1; iii++) {
		/* TODO: Restore full test once ESP caches are integrated */
		coherence = ACC_COH_LLC;
		// struct esp_device *dev = &espdevs[n];
		struct esp_device *dev = &espdevs;
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

		//printf("******************** %s.%d ********************\n", DEV_NAME, n);
		// Check access ok
		if (SORT_LEN < sort_len_min ||
			SORT_LEN > sort_len_max ||
			SORT_BATCH < 1 ||
			SORT_BATCH > sort_batch_max) {
			//printf("  Error: unsopported configuration parameters for %s.%d\n", DEV_NAME, n);
			//printf("         device can sort up to %d fp-vectors of size [%d, %d]\n",
			//	sort_batch_max, sort_len_min, sort_len_max);
			break;
		}

		// Check if scatter-gather DMA is disabled
		if (ioread32(dev, PT_NCHUNK_MAX_REG) == 0) {
			//printf("  -> scatter-gather DMA is disabled. Abort.\n");
			scatter_gather = 0;
		}

		if (scatter_gather)
			if (ioread32(dev, PT_NCHUNK_MAX_REG) < NCHUNK) {
				//printf("  -> Not enough TLB entries available. Abort.\n");
				break;
			}

		// Allocate memory (will be contigous anyway in baremetal)
		mem = aligned_malloc(SORT_BUF_SIZE);

		printf("  memory buffer base-address = %p\n", mem);

		if (scatter_gather) {
			//Alocate and populate page table
			ptable = aligned_malloc(NCHUNK * sizeof(unsigned *));
			for (i = 0; i < NCHUNK; i++)
				ptable[i] = (unsigned *) &mem[i * (CHUNK_SIZE / sizeof(unsigned))];

			//printf("  ptable = %p\n", ptable);
			//printf("  nchunk = %lu\n", NCHUNK);
		}

		// Initialize input: write floating point hex values (simpler to debug)
		timestamps[0] = get_counter();
		init_buf((float *) mem, SORT_LEN, SORT_BATCH);
		timestamps[1] = get_counter();

		// Configure device
		iowrite32(dev, SELECT_REG, ioread32(dev, DEVID_REG));
		iowrite32(dev, COHERENCE_REG, coherence);

		if (scatter_gather) {
			iowrite32(dev, PT_ADDRESS_REG, (unsigned long) ptable);
			iowrite32(dev, PT_NCHUNK_REG, NCHUNK);
			iowrite32(dev, PT_SHIFT_REG, CHUNK_SHIFT);
			iowrite32(dev, SRC_OFFSET_REG, 0);
			iowrite32(dev, DST_OFFSET_REG, 0); // Sort runs in place
		} else {
			iowrite32(dev, SRC_OFFSET_REG, (unsigned long) mem);
			iowrite32(dev, DST_OFFSET_REG, (unsigned long) mem); // Sort runs in place
		}
		iowrite32(dev, SORT_LEN_REG, SORT_LEN);
		iowrite32(dev, SORT_BATCH_REG, SORT_BATCH);

		// Flush for non-coherent DMA
		//////////////////esp_flush(coherence);

		// Start accelerator
		printf("  Start..\n");
		iowrite32(dev, CMD_REG, CMD_MASK_START);

		timestamps[2] = get_counter();

		done = 0;
		while (!done) {
			done = ioread32(dev, STATUS_REG);
			done &= STATUS_MASK_DONE;
		}
		timestamps[3] = get_counter();

		iowrite32(dev, CMD_REG, 0x0);
		printf("  Done\n");

		/* /\* Print output *\/ */
		/* printf("  output:\n"); */
		/* for (j = 0; j < SORT_BATCH; j++) */
		/* 	for (i = 0; i < SORT_LEN; i++) */
		/* 		printf("    mem[%d][%d] = %08x\n", j, i, mem[j*SORT_LEN + i]); */

		/* Validation */
		printf("  validating...\n");
		sync();

		timestamps[4] = get_counter();

		for (j = 0; j < SORT_BATCH; j++) {
			int err = validate_sorted((float *) &mem[j * SORT_LEN], SORT_LEN);
			/* if (err != 0) */
			/* 	printf("  Error: %s.%d mismatch on batch %d\n", DEV_NAME, n, j); */
			errors += err;
		}
		timestamps[5] = get_counter();
		if (errors)
			printf("  ... FAIL\n");
		else
			printf("  ... PASS\n");
		//printf("**************************************************\n\n");
		if (scatter_gather)
			aligned_free(ptable);
		aligned_free(mem);

		a += timestamps[1] - timestamps[0];
		b += timestamps[3] - timestamps[2];
		c += timestamps[5] - timestamps[4];

	}
	//long long unsigned int a, b, c;
	// a = timestamps[1] - timestamps[0];
	// b = timestamps[3] - timestamps[2];
	// c = timestamps[5] - timestamps[4];
	printf("Spandex perf:\nBufgen\t%lu\nSort\t%lu\nValid\t%lu\nTot\t%lu\n", a, b, c, a+b+c);
	printf("%lu\n",get_counter());
	return 0;
}
