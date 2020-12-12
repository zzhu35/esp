#include "testmod.h"
#include "defines.h"
#include "stdio.h"

/* void (*mpfunc[16])(int index); */

/* Decide between GRLIB tests or the ESP multi-core extension to the GRLIB tests */
/* - Set to 0 for GRLIB tests */
/* - Set to 1 for ESP tests */
#define USE_ESP_TESTS 0

leon3_test(int domp, int *irqmp, int mtest)
{
    int tmp, i;
    int pid = get_pid();
    
    int ncpu = (((*(irqmp + 0x10/4)) >> 28) & 0x0f) + 1;

    if (!pid) printf("Start testing on %d CPUs.\n", ncpu);
    
    if (!pid) report_init();

    if (domp)
        mptest_start(irqmp);

    test_loop_start();
    if (!pid) printf("test_loop_start\n", ncpu);

    
    psync(sync_leon3_test, pid, ncpu);
    report_test(TEST_LEON3);

    /* TESTS */
    /* Uncomment the tests you want to execute. */
    
    // report_test(TEST_REG);
    // if (regtest()) report_fail(FAIL_REG);

    report_test(TEST_MUL);
    multest();
    if (!pid) printf("multest\n", ncpu);

    report_test(TEST_DIV);
    divtest();
    if (!pid) printf("divtest\n", ncpu);

    report_test(TEST_FPU);
    fputest();
    if (!pid) printf("fputest\n", ncpu);

	if (!pid) data_structures_setup();
    report_test(TEST_FILL_B);
    cache_fill(4, ncpu, BYTE);
    if (!pid) printf("cache_fill(4, ncpu, BYTE)\n", ncpu);

    report_test(TEST_FILL_HW);
    cache_fill(4, ncpu, HALFWORD);
    if (!pid) printf("cache_fill(4, ncpu, HALFWORD)\n", ncpu);

    report_test(TEST_FILL_W);
    cache_fill(4, ncpu, WORD);
    if (!pid) printf("cache_fill(4, ncpu, WORD)\n", ncpu);

    
    report_test(TEST_SHARING);
    false_sharing(20, ncpu);
    if (!pid) printf("false_sharing(20, ncpu)\n", ncpu);


    // l2_cache_test(domp, irqmp);

    report_test(TEST_LOCK);
    test_lock(100, ncpu);
    if (!pid) printf("test_lock(100, ncpu)\n", ncpu);


    report_test(TEST_MESI);
    mesi_test(ncpu, 1);
    if (!pid) printf("mesi_test(ncpu, 1)\n", ncpu);



    report_test(TEST_RAND_RW);
    rand_rw(200, ncpu);
    if (!pid) printf("rand_rw(200, ncpu)\n", ncpu);

    
    /* End of TESTS */
    
    test_loop_end();
    if (!pid) printf("test_loop_end\n", ncpu);

    
    if (domp)
        mptest_end(irqmp);

    /* Other TESTS */
    /* Uncomment the tests you want to execute. */

    /* grfpu_test(); */
    /* cachetest(); */
    /* mmu_test(); */
    /* rextest(); */
    /* awptest(); */

    /* End of other TESTS */

    printf("Parse and print report\n");
    
    report_parse(ncpu);

    printf("Test complete.\n");
}
