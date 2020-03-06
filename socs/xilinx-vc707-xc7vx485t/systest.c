#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "defines.h"

#define NCPU 2

#define RUN_AND_REPORT(PROGRAM, ...) { PROGRAM(__VA_ARGS__); printf("[TEST] "#PROGRAM" complete\n");}


static arch_spinlock_t uart_lock;
static int barrier[2];


int main(int argc, char **argv)
{
	printf("barrier addresss: 0x%x\n", barrier);
	mptest_start(0x80000200);
	int id = get_pid();

	arch_spin_lock(&uart_lock);

	printf("Leon3 #%d on ESP\n", id);
	printf("Spandex Inside\n");

	// RUN_AND_REPORT(test_lock, 4, NCPU);
	// RUN_AND_REPORT(divtest);
	// RUN_AND_REPORT(multest);
	// RUN_AND_REPORT(leon3_test, 0, NULL, 0, NCPU);
	// RUN_AND_REPORT(cache_fill, L2_WAYS, NCPU, 32);
	// RUN_AND_REPORT(false_sharing, 4, NCPU);
	// RUN_AND_REPORT(fputest);

	// test_lock(1, NCPU);
	printf("---------- End of Test %d ----------\n", id);

	arch_spin_unlock(&uart_lock);

	psync(barrier, id, NCPU);

	return 0;
}
