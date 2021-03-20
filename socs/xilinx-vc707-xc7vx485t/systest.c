
#include <stdio.h>
#include <stdlib.h>
// #include <esp_accelerator.h>
// #include <esp_probe.h>

void ERROR(const char *fname, int lineno)
{
	printf("ERROR: %s:%d\n", fname, lineno);
}

int test_lock_32()
{
	volatile int32_t lock = 0;
	volatile int32_t* lock_ptr = &lock;
	int32_t out = 0, tmp = 0;

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

	if (out != 0 || lock != 1) {
		printf("%d, %d\n", out, lock);
		ERROR(__FILE__, __LINE__);
		return -1;
	}
	
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

	if (out != 1 || lock != 0) {
		printf("%d, %d\n", out, lock);
		ERROR(__FILE__, __LINE__);
		return -1;
	}

	return 0;
}

int test_lock_64()
{
	volatile int64_t lock = 0;
	volatile int64_t* lock_ptr = &lock;
	int64_t out = 0, tmp = 0;

	asm volatile (
		"li t0, 1;"
		"li t2, 0;"
		"mv t1, %1;"
		"amoswap.d.aqrl t2, t0, (t1);"
		"mv %0, t2"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 0 || lock != 1) {
		ERROR(__FILE__, __LINE__);
		return -1;
	}
	
	asm volatile (
		"li t0, 0;"
		"li t2, 0;"
		"mv t1, %1;"
		"amoswap.d.aqrl t2, t0, (t1);"
		"mv %0, t2"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 1 || lock != 0) {
		ERROR(__FILE__, __LINE__);
		return -1;
	}

	return 0;
}

int test_add_32()
{
	volatile int32_t lock = 0;
	volatile int32_t* lock_ptr = &lock;
	int32_t out = 0, tmp = 0;

	int i;
	for (i = 0; i < 100; i++)
	{
		asm volatile (
			"li t0, 1;"
			"li t2, 0;"
			"mv t1, %1;"
			"amoadd.w.aqrl t2, t0, (t1);"
			"mv %0, t2"
			: "=r" ( out )
			: "r" ( lock_ptr )
			: "t0", "t1", "t2", "memory"
		);

		if (out != i || lock != i + 1) {
			ERROR(__FILE__, __LINE__);
			return -1;
		}
	}

	return 0;
}

int test_add_64()
{
	volatile int64_t lock = 0;
	volatile int64_t* lock_ptr = &lock;
	int64_t out = 0, tmp = 0;

	int i;
	for (i = 0; i < 100; i++)
	{
		asm volatile (
			"li t0, 1;"
			"li t2, 0;"
			"mv t1, %1;"
			"amoadd.d.aqrl t2, t0, (t1);"
			"mv %0, t2"
			: "=r" ( out )
			: "r" ( lock_ptr )
			: "t0", "t1", "t2", "memory"
		);

		if (out != i || lock != i + 1) {
			ERROR(__FILE__, __LINE__);
			return -1;
		}
	}

	return 0;
}

int test_and_32()
{
	volatile int32_t lock = 0xffffffff;
	volatile int32_t* lock_ptr = &lock;
	int32_t out = 0, tmp = 0;

	asm volatile (
		"li t0, 0xffff0000;"
		"li t2, 0;"
		"mv t1, %1;"
		"amoand.w.aqrl t2, t0, (t1);"
		"mv %0, t2"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 0xffffffff || lock != 0xffff0000) {
		printf("%x, %x\n", out, lock);
		ERROR(__FILE__, __LINE__);
		return -1;
	}
	
	asm volatile (
		"li t0, 0x00ff1111;"
		"li t2, 0;"
		"mv t1, %1;"
		"amoand.w.aqrl t2, t0, (t1);"
		"mv %0, t2"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 0xffff0000 || lock != 0x00ff0000) {
		printf("%x, %x\n", out, lock);
		ERROR(__FILE__, __LINE__);
		return -1;
	}

	return 0;
}

int test_and_64()
{
	volatile int64_t lock = 0x00000000ffffffff;
	volatile int64_t* lock_ptr = &lock;
	int64_t out = 0, tmp = 0;

	asm volatile (
		"li t0, 0x00000000ffff0000;"
		"li t2, 0;"
		"mv t1, %1;"
		"amoand.d.aqrl t2, t0, (t1);"
		"mv %0, t2"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 0x00000000ffffffff || lock != 0x00000000ffff0000) {
		printf("%lx, %lx\n", out, lock);
		ERROR(__FILE__, __LINE__);
		return -1;
	}
	
	asm volatile (
		"li t0, 0x0000000000ff1111;"
		"li t2, 0;"
		"mv t1, %1;"
		"amoand.d.aqrl t2, t0, (t1);"
		"mv %0, t2"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 0x00000000ffff0000 || lock != 0x0000000000ff0000) {
		printf("%lx, %lx\n", out, lock);
		ERROR(__FILE__, __LINE__);
		return -1;
	}

	return 0;
}

int test_or_32()
{
	volatile int32_t lock = 0;
	volatile int32_t* lock_ptr = &lock;
	int32_t out = 0, tmp = 0;

	asm volatile (
		"li t0, 0xffff0000;"
		"li t2, 0;"
		"mv t1, %1;"
		"amoor.w.aqrl t2, t0, (t1);"
		"mv %0, t2"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 0 || lock != 0xffff0000) {
		printf("%x, %x\n", out, lock);
		ERROR(__FILE__, __LINE__);
		return -1;
	}
	
	asm volatile (
		"li t0, 0x00ff1111;"
		"li t2, 0;"
		"mv t1, %1;"
		"amoor.w.aqrl t2, t0, (t1);"
		"mv %0, t2"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 0xffff0000 || lock != 0xffff1111) {
		printf("%x, %x\n", out, lock);
		ERROR(__FILE__, __LINE__);
		return -1;
	}

	return 0;
}

int test_or_64()
{
	volatile int64_t lock = 0;
	volatile int64_t* lock_ptr = &lock;
	int64_t out = 0, tmp = 0;

	asm volatile (
		"li t0, 0x00000000ffff0000;"
		"li t2, 0;"
		"mv t1, %1;"
		"amoor.d.aqrl t2, t0, (t1);"
		"mv %0, t2"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 0 || lock != 0x00000000ffff0000) {
		printf("%lx, %lx\n", out, lock);
		ERROR(__FILE__, __LINE__);
		return -1;
	}
	
	asm volatile (
		"li t0, 0x0000000000ff1111;"
		"li t2, 0;"
		"mv t1, %1;"
		"amoor.d.aqrl t2, t0, (t1);"
		"mv %0, t2"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 0x00000000ffff0000 || lock != 0x00000000ffff1111) {
		printf("%lx, %lx\n", out, lock);
		ERROR(__FILE__, __LINE__);
		return -1;
	}

	return 0;
}

int test_xor_32()
{
	volatile int32_t lock = 0;
	volatile int32_t* lock_ptr = &lock;
	int32_t out = 0, tmp = 0;

	asm volatile (
		"li t0, 0xffff0000;"
		"li t2, 0;"
		"mv t1, %1;"
		"amoxor.w.aqrl t2, t0, (t1);"
		"mv %0, t2"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 0 || lock != 0xffff0000) {
		printf("%x, %x\n", out, lock);
		ERROR(__FILE__, __LINE__);
		return -1;
	}
	
	asm volatile (
		"li t0, 0x00ff1111;"
		"li t2, 0;"
		"mv t1, %1;"
		"amoxor.w.aqrl t2, t0, (t1);"
		"mv %0, t2"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 0xffff0000 || lock != 0xff001111) {
		printf("%x, %x\n", out, lock);
		ERROR(__FILE__, __LINE__);
		return -1;
	}

	return 0;
}

int test_xor_64()
{
	volatile int64_t lock = 0;
	volatile int64_t* lock_ptr = &lock;
	int64_t out = 0, tmp = 0;

	asm volatile (
		"li t0, 0x00000000ffff0000;"
		"li t2, 0;"
		"mv t1, %1;"
		"amoxor.d.aqrl t2, t0, (t1);"
		"mv %0, t2"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 0 || lock != 0x00000000ffff0000) {
		printf("%lx, %lx\n", out, lock);
		ERROR(__FILE__, __LINE__);
		return -1;
	}
	
	asm volatile (
		"li t0, 0x0000000000ff1111;"
		"li t2, 0;"
		"mv t1, %1;"
		"amoxor.d.aqrl t2, t0, (t1);"
		"mv %0, t2"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 0x00000000ffff0000 || lock != 0x00000000ff001111) {
		printf("%lx, %lx\n", out, lock);
		ERROR(__FILE__, __LINE__);
		return -1;
	}

	return 0;
}

int test_max_32()
{
	volatile int32_t lock = 0;
	volatile int32_t* lock_ptr = &lock;
	int32_t out = 0, tmp = 0;

	asm volatile (
		"li t0, 1;"
		"li t2, 0;"
		"mv t1, %1;"
		"amomax.w.aqrl t2, t0, (t1);"
		"mv %0, t2"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 0 || lock != 1) {
		printf("%x, %x\n", out, lock);
		ERROR(__FILE__, __LINE__);
		return -1;
	}
	
	asm volatile (
		"li t0, -1;"
		"li t2, 0;"
		"mv t1, %1;"
		"amomax.w.aqrl t2, t0, (t1);"
		"mv %0, t2"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 1 || lock != 1) {
		printf("%x, %x\n", out, lock);
		ERROR(__FILE__, __LINE__);
		return -1;
	}

	return 0;
}

int test_max_64()
{
	volatile int64_t lock = 0;
	volatile int64_t* lock_ptr = &lock;
	int64_t out = 0, tmp = 0;

	asm volatile (
		"li t0, 1;"
		"li t2, 0;"
		"mv t1, %1;"
		"amomax.d.aqrl t2, t0, (t1);"
		"mv %0, t2"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 0 || lock != 1) {
		printf("%lx, %lx\n", out, lock);
		ERROR(__FILE__, __LINE__);
		return -1;
	}
	
	asm volatile (
		"li t0, -1;"
		"li t2, 0;"
		"mv t1, %1;"
		"amomax.d.aqrl t2, t0, (t1);"
		"mv %0, t2"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 1 || lock != 1) {
		printf("%lx, %lx\n", out, lock);
		ERROR(__FILE__, __LINE__);
		return -1;
	}

	return 0;
}

int test_min_32()
{
	volatile int32_t lock = 0;
	volatile int32_t* lock_ptr = &lock;
	int32_t out = 0, tmp = 0;

	asm volatile (
		"li t0, 1;"
		"li t2, 0;"
		"mv t1, %1;"
		"amomin.w.aqrl t2, t0, (t1);"
		"mv %0, t2"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 0 || lock != 0) {
		printf("%x, %x\n", out, lock);
		ERROR(__FILE__, __LINE__);
		return -1;
	}
	
	asm volatile (
		"li t0, -1;"
		"li t2, 0;"
		"mv t1, %1;"
		"amomin.w.aqrl t2, t0, (t1);"
		"mv %0, t2"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 0 || lock != -1) {
		printf("%x, %x\n", out, lock);
		ERROR(__FILE__, __LINE__);
		return -1;
	}

	return 0;
}

int test_min_64()
{
	volatile int64_t lock = 0;
	volatile int64_t* lock_ptr = &lock;
	int64_t out = 0, tmp = 0;

	asm volatile (
		"li t0, 1;"
		"li t2, 0;"
		"mv t1, %1;"
		"amomin.d.aqrl t2, t0, (t1);"
		"mv %0, t2"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 0 || lock != 0) {
		printf("%lx, %lx\n", out, lock);
		ERROR(__FILE__, __LINE__);
		return -1;
	}
	
	asm volatile (
		"li t0, -1;"
		"li t2, 0;"
		"mv t1, %1;"
		"amomin.d.aqrl t2, t0, (t1);"
		"mv %0, t2"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 0 || lock != -1) {
		printf("%lx, %lx\n", out, lock);
		ERROR(__FILE__, __LINE__);
		return -1;
	}

	return 0;
}

int test_maxu_32()
{
	volatile uint32_t lock = 0;
	volatile uint32_t* lock_ptr = &lock;
	uint32_t out = 0, tmp = 0;

	asm volatile (
		"li t0, 1;"
		"li t2, 0;"
		"mv t1, %1;"
		"amomaxu.w.aqrl t2, t0, (t1);"
		"mv %0, t2"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 0 || lock != 1) {
		printf("%x, %x\n", out, lock);
		ERROR(__FILE__, __LINE__);
		return -1;
	}
	
	asm volatile (
		"li t0, -1;"
		"li t2, 0;"
		"mv t1, %1;"
		"amomaxu.w.aqrl t2, t0, (t1);"
		"mv %0, t2"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 1 || lock != 0xffffffff) {
		printf("%x, %x\n", out, lock);
		ERROR(__FILE__, __LINE__);
		return -1;
	}

	return 0;
}

int test_maxu_64()
{
	volatile uint64_t lock = 0;
	volatile uint64_t* lock_ptr = &lock;
	uint64_t out = 0, tmp = 0;

	asm volatile (
		"li t0, 1;"
		"li t2, 0;"
		"mv t1, %1;"
		"amomaxu.d.aqrl t2, t0, (t1);"
		"mv %0, t2"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 0 || lock != 1) {
		printf("%lx, %lx\n", out, lock);
		ERROR(__FILE__, __LINE__);
		return -1;
	}
	
	asm volatile (
		"li t0, -1;"
		"li t2, 0;"
		"mv t1, %1;"
		"amomaxu.d.aqrl t2, t0, (t1);"
		"mv %0, t2"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 1 || lock != 0xffffffffffffffff) {
		printf("%lx, %lx\n", out, lock);
		ERROR(__FILE__, __LINE__);
		return -1;
	}

	return 0;
}

int test_minu_32()
{
	volatile uint32_t lock = 0;
	volatile uint32_t* lock_ptr = &lock;
	uint32_t out = 0, tmp = 0;

	asm volatile (
		"li t0, 1;"
		"li t2, 0;"
		"mv t1, %1;"
		"amominu.w.aqrl t2, t0, (t1);"
		"mv %0, t2"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 0 || lock != 0) {
		printf("%x, %x\n", out, lock);
		ERROR(__FILE__, __LINE__);
		return -1;
	}
	
	asm volatile (
		"li t0, -1;"
		"li t2, 0;"
		"mv t1, %1;"
		"amominu.w.aqrl t2, t0, (t1);"
		"mv %0, t2"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 0 || lock != 0) {
		printf("%x, %x\n", out, lock);
		ERROR(__FILE__, __LINE__);
		return -1;
	}

	return 0;
}

int test_minu_64()
{
	volatile uint64_t lock = 0;
	volatile uint64_t* lock_ptr = &lock;
	uint64_t out = 0, tmp = 0;

	asm volatile (
		"li t0, 1;"
		"li t2, 0;"
		"mv t1, %1;"
		"amominu.d.aqrl t2, t0, (t1);"
		"mv %0, t2"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 0 || lock != 0) {
		printf("%lx, %lx\n", out, lock);
		ERROR(__FILE__, __LINE__);
		return -1;
	}
	
	asm volatile (
		"li t0, -1;"
		"li t2, 0;"
		"mv t1, %1;"
		"amominu.d.aqrl t2, t0, (t1);"
		"mv %0, t2"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 0 || lock != 0) {
		printf("%lx, %lx\n", out, lock);
		ERROR(__FILE__, __LINE__);
		return -1;
	}

	return 0;
}

int amo_all()
{
	printf("Testing AMO...\n");
	int ret = 0;
	ret |= test_lock_32();
	ret |= test_lock_64();
	ret |= test_add_32();
	ret |= test_add_64();
	ret |= test_and_32();
	ret |= test_and_64();
	ret |= test_or_32();
	ret |= test_or_64();
	ret |= test_xor_32();
	ret |= test_xor_64();
	ret |= test_max_32();
	ret |= test_max_64();
	ret |= test_min_32();
	ret |= test_min_64();
	ret |= test_maxu_32();
	ret |= test_maxu_64();
	ret |= test_minu_32();
	ret |= test_minu_64();
	return ret;
}

#define WB_SIZE 4
int wb_fill()
{
	printf("Testing WB Fill...\n");
	uint64_t magic = 0xbeefcafe;
	static volatile uint64_t buf[2 * WB_SIZE * 2] __attribute__ ((aligned (16)));
	int i;
	for (i = 0; i < 2 * WB_SIZE * 2; i++)
	{
		buf[i] = i + 0xbeefcafe;
	}

	for (i = 0; i < 2 * WB_SIZE * 2; i++)
	{
		if (buf[i] != i + 0xbeefcafe)
		{
			printf("Loop iter: %d\n", i);
			ERROR(__FILE__, __LINE__);
			return -1;
		}
	}
	return 0;
}

#define L2_SIZE (2*32)
int cache_fill()
{
	printf("Testing Cache Fill...\n");
	uint64_t magic = 0xbeefcafe;
	static volatile uint8_t buf[L2_SIZE*16] __attribute__ ((aligned (16)));
	volatile uint64_t* buf64 = (volatile uint64_t*)buf;
	volatile uint32_t* buf32 = (volatile uint32_t*)buf;
	volatile uint16_t* buf16 = (volatile uint16_t*)buf;
	volatile uint8_t* buf8 = (volatile uint8_t*)buf;

	int i;

	// TEST 64
	for (i = 0; i < L2_SIZE*2; i++)
	{
		buf64[i] = i + magic;
	}

	for (i = 0; i < L2_SIZE*2; i++)
	{
		if (buf64[i] != i + magic)
		{
			printf("cache_fill 64 Loop iter: %d\n", i);
			ERROR(__FILE__, __LINE__);
			return -1;
		}
	}

	// TEST 32
	for (i = 0; i < L2_SIZE*4; i++)
	{
		buf32[i] = i + magic;
	}

	for (i = 0; i < L2_SIZE*4; i++)
	{
		if (buf32[i] != i + magic)
		{
			printf("cache_fill 32 Loop iter: %d\n", i);
			ERROR(__FILE__, __LINE__);
			return -1;
		}
	}

	// TEST 16
	for (i = 0; i < L2_SIZE*8; i++)
	{
		buf16[i] = i + magic;
	}

	for (i = 0; i < L2_SIZE*8; i++)
	{
		if (buf16[i] != (uint16_t)(i + magic))
		{
			printf("cache_fill 16 Loop iter: %d\n", i);
			printf("addr: %x\n", &(buf16[i]));
			printf("exp: %x, actual: %x\n", (i + magic), buf16[i]);
			ERROR(__FILE__, __LINE__);
			return -1;
		}
	}

	// TEST 8
	for (i = 0; i < L2_SIZE*16; i++)
	{
		buf8[i] = i + magic;
	}

	for (i = 0; i < L2_SIZE*16; i++)
	{
		if (buf8[i] != (uint8_t)(i + magic))
		{
			printf("cache_fill 8 Loop iter: %d\n", i);
			ERROR(__FILE__, __LINE__);
			return -1;
		}
	}

	return 0;
}

#define TAG_OFF (1 << 9)
int evict()
{
	printf("Testing Cache Evict...\n");
	volatile int8_t* tmp = (volatile int8_t*)0xA0000000;
	int i;
	for (i = 0; i < 1000; i++)
	{
		*(volatile int64_t*)tmp = i;
		tmp += TAG_OFF;
	}
	return 0;

}

int lrsc64()
{
	printf("Testing LRSC 64...\n");
	volatile int64_t lock = 0;
	volatile int64_t* lock_ptr = &lock;
	int64_t out = 0, tmp = 0;

	asm volatile (
		"li t0, 0;"
		"li t2, 1;"
		"mv t1, %1;"
		"lr.d.aqrl t0, (t1);"
		"sc.d.aqrl t0, t2, (t1);"
		"mv %0, t0"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 0 || lock != 1) {
		printf("%x %x\n", out, lock);
		ERROR(__FILE__, __LINE__);
		return -1;
	}

	return 0;
}

int lrsc32()
{
	printf("Testing LRSC 32...\n");
	volatile int32_t lock = 0;
	volatile int32_t* lock_ptr = &lock;
	int32_t out = 0, tmp = 0;

	asm volatile (
		"li t0, 0;"
		"li t2, 1;"
		"mv t1, %1;"
		"lr.w.aqrl t0, (t1);"
		"sc.w.aqrl t0, t2, (t1);"
		"mv %0, t0"
		: "=r" ( out )
		: "r" ( lock_ptr )
		: "t0", "t1", "t2", "memory"
	);

	if (out != 0 || lock != 1) {
		printf("%x %x\n", out, lock);
		ERROR(__FILE__, __LINE__);
		return -1;
	}


	return 0;
}

int main(int argc, char **argv)
{

	int ret = 0;
	int i;

	ret |= amo_all();
	ret |= wb_fill();
	ret |= cache_fill();
	ret |= evict();
	ret |= lrsc64();
	ret |= lrsc32();

	if (!ret) printf("PASS.\n");
	else printf("FAIL.\n");

	return 0;
}
