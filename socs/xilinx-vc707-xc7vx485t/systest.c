#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "defines.h"

// volatile int sync_array[4] = {0, 0, 0, 1};
// volatile int sync_array_2[4] = {0, 0, 0, 0};

int main(int argc, char **argv)
{
	// base_test();
	leon3_test(1, 0x80000200, 0);
	return 0;

	// int tmp, i;
    // int pid = get_pid();
    
    // int ncpu = (((*((int*)0x80000200 + 0x10/4)) >> 28) & 0x0f) + 1;

    // if (!pid) printf("Start testing on %d CPUs.\n", ncpu);

	// report_init();
	// mptest_start(0x80000200);


	// // psync(sync_array, pid, ncpu);
    // // if (!pid) printf("Checkpoint.\n");
    // // psync(sync_array_2, pid, ncpu);
    
    // test_loop_start();
    
	// if (!pid) printf("sync: -> 0x%x\n", sync_leon3_test);
    // psync(sync_leon3_test, pid, ncpu);
    // // if (!pid) printf("Checkpoint P0.\n");
    // // if (pid) printf("Checkpoint P1.\n");

    // report_test(TEST_LEON3);

    // test_loop_end();
	// mptest_end(0x80000200);

	// printf("Parse and print report\n");
    
    // report_parse(ncpu);
    // if (!pid) printf("End testing on %d CPUs.\n", ncpu);

}
