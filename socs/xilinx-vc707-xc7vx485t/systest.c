
#include <stdio.h>

int test_lock_64()
{
	int64_t lock = 0;
	int64_t* lock_ptr = &lock;
	int64_t out = 0, tmp = 0;

	asm volatile (
		"li t0, 1;"
		"li t2, 0;"
		"mv t1, %1;"
		"amoswap.w.aqrl t2, t0, (t1);"
		"mv %0, t2"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 0 || lock != 1) return -1;
	
	asm volatile (
		"li t0, 0;"
		"li t2, 0;"
		"mv t1, %1;"
		"amoswap.w.aqrl t2, t0, (t1);"
		"mv %0, t2"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 1 || lock != 0) return -1;

	return 0;
}

int main(int argc, char **argv)
{
	printf("Testing AMO...\n");
	int64_t lock = 0;
	int64_t* lock_ptr = &lock;
	int64_t out = 0, tmp = 0;

	int ret = 0;
	ret |= test_lock_64();

	if (!ret) printf("PASS.\n");
	else printf("Error!\n");

	return 0;
}
