#include "uart.h"
#include <stdlib.h>


#define START_TEST   0
#define STOP_TEST    1
#define TEST_FAIL    2
#define REGFILE      3
#define MUL_TEST     4
#define DIV_TEST     5
#define CACHE_TEST   6
#define MP_TEST      7
#define FPU_TEST     8
#define ITAG_TEST    9
#define DTAG_TEST    10
#define IDAT_TEST    11
#define DDAT_TEST    12
#define GRFPU_TEST   13
#define MMU_TEST     14
#define CASA_TEST    15
#define PRIV_TEST    16
#define REX_TEST     17

#define APBUART_TEST 7 
#define FTSRCTRL     8 
#define GPIO         9
#define CMEM_TEST    10
#define IRQ_TEST     11
#define SPW_TEST     12

#define MCTRL_BYTE   3
#define MCTRL_EDAC   4
#define MCTRL_WPROT  5

#define SPW_SNOOP_TEST  1
#define SPW_NOSNOOP_TEST  2
#define SPW_RMAP_TEST  3
#define SPW_TIME_TEST  4

/* Cache parameters */
#define SETS 32
#define L2_WAYS 2
// #define N_CPU 4
#define MAX_N_CPU 4
//#define LLC_WAYS CPUS*L2_WAYS
// #define LLC_WAYS (MAX_N_CPU * L2_WAYS)
#define LINE_SIZE 16
#define BYTES_PER_WORD 8
#define WORDS_PER_LINE 2
#define ASI_LEON_DFLUSH 0x11
#define WORD_SIZE 64 /* Assuming words of 32 bits */
#define L2_CACHE_BYTES (SETS * L2_WAYS * LINE_SIZE)
#define L2_CACHE_WORDS (L2_CACHE_BYTES / BYTES_PER_WORD)


/* Test features */
#define SPINLOCK 0
#define SEMAPHORE 1

#define BYTE		0
#define HALFWORD	1
#define WORD_32		2
#define WORD_64     3

/* Address map */
#define BASE_CACHEABLE_ADDRESS 0X45000000
#define LAST_CACHEABLE_ADDRESS 0X4FFFFFFF

/*
 * Test report
 */
#define TEST 0
#define FAIL 1

#define N_IDS_TEST 15
#define N_IDS_FAIL 12
#define N_IDS (N_IDS_TEST > N_IDS_FAIL ? N_IDS_TEST : N_IDS_FAIL)

#define MAX_REPORT_STRING 18

/* Test IDs (max 32, which is the word size) */
#define TEST_START 0
#define TEST_RISCV 1
#define TEST_REG 2
#define TEST_MUL 3
#define TEST_DIV 4
#define TEST_FPU 5
#define TEST_FILL_B 6
#define TEST_SHARING 7
#define TEST_L2 8
#define TEST_RAND_RW 9
#define TEST_FILL_HW 10
#define TEST_MESI 11
#define TEST_LOCK 12
#define TEST_FILL_W 13
#define TEST_END 14

/* Fail IDs (max 32, which is the word size) */
#define FAIL_REG 0
#define FAIL_MUL 1
#define FAIL_DIV 2
#define FAIL_FPU 3
#define FAIL_FILL_B 4
#define FAIL_SHARING 5
#define FAIL_RAND_RW 6
#define FAIL_FILL_HW 7
#define FAIL_MESI 8
#define FAIL_LOCK 9
#define FAIL_FILL_W 10
#define FAIL_MPTEST 11

/* Report functions */
extern void report_init();
extern void report_test();
extern void report_fail();
extern void report_parse();
extern void test_loop_start();
extern void test_loop_end();
/* Data structures */
int *cache_fill_matrix[MAX_N_CPU];

typedef volatile unsigned int arch_spinlock_t;
arch_spinlock_t lock;

/* Sync arrays */
volatile int sync_loop_end[MAX_N_CPU]; // = {0, 0, 0, 0};
volatile int sync_riscv_test[MAX_N_CPU]; // = {0, 0, 0, 0};
volatile int sync_cache_fill_bytes[MAX_N_CPU]; // = {0, 0, 0, 0};
volatile int sync_cache_fill_halfwords[MAX_N_CPU]; // = {0, 0, 0, 0};
volatile int sync_cache_fill_words[MAX_N_CPU]; // = {0, 0, 0, 0};
volatile int sync_false_sharing1[MAX_N_CPU]; // = {0, 0, 0, 0};
volatile int sync_false_sharing2[MAX_N_CPU]; // = {0, 0, 0, 0};
volatile int sync_rand_rw[MAX_N_CPU]; // = {0, 0, 0, 0};
volatile int sync_mesi1[MAX_N_CPU]; // = {0, 0, 0, 0};
volatile int sync_mesi2[MAX_N_CPU]; // = {0, 0, 0, 0};
volatile int sync_mesi3[MAX_N_CPU]; // = {0, 0, 0, 0};
volatile int sync_lock1[MAX_N_CPU]; // = {0, 0, 0, 0};
volatile int sync_lock2[MAX_N_CPU]; // = {0, 0, 0, 0};



void evict(int *ptr, int offset, int ways, int max_range, int hsize)
{
    int i;
    int curr_offset = offset;
    int tag_offset;
    short *ptr_short;
    char *ptr_char;

    /* if (hsize == WORD) { */
    tag_offset = 256 * 4;
    /* } else if (hsize == HALFWORD) { */
    /* 	tag_offset = 1 << (int) log2(SETS * WORDS_PER_LINE * 2); */
    /* 	ptr_short = (short *) ptr; */
    /* } else { */
    /* 	tag_offset = 1 << (int) log2(SETS * LINE_SIZE); */
    /* 	ptr_char = (char *) ptr; */
    /* } */

    for (i = 0; i <= ways; i++){
        curr_offset += tag_offset;
        /* curr_offset = curr_offset % max_range; */

        /* if (hsize == WORD) */
        ptr[curr_offset] = 0xEEEEEEEE;
        /* else if (hsize == HALFWORD) */
        /*     ptr_short[curr_offset] = 0xEEEE; */
        /* else */
        /*     ptr_char[curr_offset] = 0xEE; */
    }
}

void spin_lock(volatile int* l)
{
    int lock = 1;
    print_uart("Lock Function\n");
    print_uart("Lock old: ");
    print_uart_int(lock);
    print_uart("\n");

    int busy;
    int tmp = 1;

    __asm__ __volatile__ (
		"amoswap.w    %0, %2, %1  \n"
    : "=r" (busy), "+A" (lock)
    : "r" (tmp)
    : "memory");

    print_uart("Lock new: ");
    print_uart_int(lock);
    print_uart("\n");
    print_uart("Busy: ");
    print_uart_int(busy);
    print_uart("\n");
    print_uart("Tmp: ");
    print_uart_int(tmp);
    print_uart("\n");


}

void lock_test(int loop)
{

    volatile uint32_t l = 0;
    for (; loop > 0; loop--)
    {

        spin_lock(&l);

    }
}


void data_structures_setup()
{
    int i;
	// cache_fill_matrix[0] = (int *) malloc(SETS * L2_WAYS * LINE_SIZE * 2);
	cache_fill_matrix[0] = (int *) 0x80000000;
}

int cache_fill(int ways, int ncpu, int size)
{
    int i, sem;
    int cache_size, no_of_ints;
    int pid = 0;
    int *mem_ptr_int;
    short *mem_ptr_short;
    char *mem_ptr_char;

    cache_size = SETS * ways * LINE_SIZE;

    if (size == BYTE) {

        no_of_ints = cache_size;
        mem_ptr_char = (char *) cache_fill_matrix[pid];

        /* Fill the whole cache */
        for (i = 0; i < no_of_ints * 2; i++)
            mem_ptr_char[i] = (char) ((pid + 10 + (unsigned int) &mem_ptr_char[i]) % 0x100);
        
        print_uart("End of Fill cache\n");

        /* Read all values written */
        for (i = 0; i < no_of_ints * 2; i++) {
            if (mem_ptr_char[i] !=  (char) ((pid + 10 + (unsigned int) &mem_ptr_char[i]) % 0x100)) {
                print_uart("issue found\n");
                print_uart_int(i);
                print_uart("\n");
                print_uart_int(mem_ptr_char[i]);
                print_uart("\n");
                return 1;
            }
        }
        print_uart("no issue\n");

    } else if (size == HALFWORD) {

        no_of_ints = cache_size / sizeof(short);
        mem_ptr_short = (short *) cache_fill_matrix[pid];

        /* Fill the whole cache */
        for (i = 0; i < no_of_ints * 2; i++)
            mem_ptr_short[i] = (short) ((pid + 11 + (unsigned int) &mem_ptr_short[i]) % 0x10000);
        
        print_uart("End of Fill cache\n");

        /* Read all values written */
        for (i = 0; i < no_of_ints * 2; i++) {
            if (mem_ptr_short[i] !=  (short) ((pid + 11 + (unsigned int) &mem_ptr_short[i]) % 0x10000)) {
                print_uart("issue found\n");
                print_uart_int(i);
                print_uart("\n");
                print_uart_int(mem_ptr_char[i]);
                print_uart("\n");
                return 1;
            }
        }

        print_uart("no issue\n"); 
    
    } else if (size == WORD_32) {

        no_of_ints = cache_size / sizeof(int);
        mem_ptr_int = (int *) cache_fill_matrix[pid];

        /* Fill the whole cache */
        for (i = 0; i < no_of_ints * 2; i++)
            mem_ptr_int[i] = pid + 12 + (int) &mem_ptr_int[i];

        print_uart("End of Fill cache\n");

        /* Read all values written */
        for (i = 0; i < no_of_ints * 2; i++) {
            if (mem_ptr_int[i] !=  pid + 12 + (int) &mem_ptr_int[i]) {
                print_uart("issue found\n");
                print_uart_int(i);
                print_uart("\n");
                print_uart_int(mem_ptr_char[i]);
                print_uart("\n");
                return 1;
            }
        }

        print_uart("no issue\n");

    } else if (size == WORD_64) {

        long long int *mem_ptr_ll_int;
        
        no_of_ints = cache_size / sizeof(long long int);
        mem_ptr_ll_int = (long long int *) cache_fill_matrix[pid];
        

        /* Fill the whole cache */
        for (i = 0; i < no_of_ints * 2; i++)
            mem_ptr_ll_int[i] = pid + 12 + (long long int) &mem_ptr_ll_int[i];
        
        print_uart("End of Fill cache\n");

        /* Read all values written */
        for (i = 0; i < no_of_ints * 2; i++) {
            if (mem_ptr_ll_int[i] !=  pid + 12 + (long long int) &mem_ptr_ll_int[i]) {
                print_uart("issue found\n");
                print_uart_int(i);
                print_uart("\n");
                print_uart_int(mem_ptr_char[i]);
                print_uart("\n");
                return 1;
            }
        }

        print_uart("no issue\n");
    }

    return 0;
}

int false_sharing(int lines, int ncpu)
{
    int i, j, sem;
    int pid = 0;
    int *mem_ptr = cache_fill_matrix[0];

    for (i = 0; i < lines; i++) {
	j = i * WORDS_PER_LINE + pid;
	mem_ptr[j] = pid + 5 + (int) &mem_ptr[j];
    }

    for (i = 0; i < lines; i++) {
	j = i * WORDS_PER_LINE + pid;
	if (mem_ptr[j] != pid + 5 + (int) &mem_ptr[j]) {
	    return 1;
	}
    }

    return 0;
}

int rand_rw(int words, int ncpu) // words < 256
{
    unsigned int set;
    int i, sem, offset, cnt = 0;
    int pid = 0;


    /* srand(1); */

/* #if INT */

    int *ptr = cache_fill_matrix[pid];

    /* int max_range = L2_CACHE_WORDS * 2; */

/* #elif HALFWORD */

/*     short *ptr = (short *) cache_fill_matrix[pid]; */

/*     int max_range = L2_CACHE_WORDS * 4; */

/* #elif BYTE */

/*     char *ptr = (char *) cache_fill_matrix[pid]; */

/*     int max_range = L2_CACHE_BYTES * 2; */

/* #endif */

    for (i = 0; i < words; i++) {

        offset = i * 4 ; // rand() % 256;

        ptr[offset] = i + pid + (int) &ptr[offset];

        evict(ptr, offset, L2_WAYS, 0, WORD_32);

        if (ptr[offset] != i + pid + (int) &ptr[offset]) {
            print_uart("issue in rand rw\n");
            return 1;
        }
    }
    print_uart("no issue\n");

    return 0;
}


int riscv_sp_test()
{
    print_uart("Start testing.\n");

	data_structures_setup();

    // print_uart("cache_fill BYTE\n");
    // if (cache_fill(4, 1, BYTE)) {
    //     print_uart("Error!\n");
    //     return 1;
    // }

    // print_uart("cache_fill HALFWORD\n");
    // if (cache_fill(4, 1, HALFWORD)) {
    //     print_uart("Error!\n");
    //     return 1;
    // }
    
    // print_uart("cache_fill WORD_32\n");
    // if (cache_fill(4, 11, WORD_32)) {
    //     print_uart("Error!\n");
    //     return 1;
    // }

    // print_uart("cache_fill WORD_64\n");
    // if (cache_fill(4, 11, WORD_64)) {
    //     print_uart("Error!\n");
    //     return 1;
    // }
    
    // print_uart("false_sharing\n");
    // if (false_sharing(1, 1)) {
    //     print_uart("Error!\n");
    //     return 1;
    // }
    
    print_uart("rand_rw\n");
    if (rand_rw(16, 1)) {
        print_uart("Error!\n");
        return 1;
    }

    print_uart("Pass All\n");
    print_uart("\n");
    return 0;
}
