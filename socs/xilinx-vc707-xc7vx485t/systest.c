#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BYTE_MAGIC '*'
#define BLOCK_SIZE 32
#define GOOD_MAGIC 0x600D600D
#define BAD_MAGIC  0x0BAD0BAD

/*
	test malloc, continuous read and write
*/
int test_for_loop()
{
	int i, j;
	char* arr[8] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
	// write
	for (i = 0; i < 4; i++)
	{
		char* temp = (char*)malloc(BLOCK_SIZE * sizeof(char));
		for (j = 0; j < BLOCK_SIZE; j++)
		{
			temp[j] = i;
		}
		arr[i] = temp;
	}
	// read and compare
	for (i = 0; i < 4; i++)
	{
		char* temp = arr[i];
		for (j = 0; j < BLOCK_SIZE; j++)
		{
			if (temp[i] != i) 
			{
				return -1;
			}
		}
	}
	return 0;
}

/*
	Fibonacci recursion test
*/
int test_fib_helper(int i)
{
	if (i == 0) return 0;
	if (i == 1) return 1;
	return test_fib_helper(i - 1) + test_fib_helper(i - 2);
}
int test_fib()
{
	if (3 == test_fib_helper(4)) return 0;
	return -1;
}

/*
	LLC eviction
*/
int test_llc_evict()
{
	int i;
	int llc_ways = 16;
	size_t stride = 1 << 14;
	unsigned long base = 0x80000000 >> 14;
	for (i = 0; i < llc_ways << 1; i++)
	{
		void* ptr = (void*)((base + i) << 14);
		*(long*)ptr = base + i;
		printf("%x -> %d\n", ptr, *(long*)ptr);

	}

	return 0;
	
}

int main(int argc, char **argv)
{
	printf("Leon3 on ESP\n");
	printf("Spandex Inside\n");
	printf("Test #0\n%s\n", (test_for_loop()) ? "FAIL" : "PASS");
	printf("Test #1\n%s\n", (test_fib()) ? "FAIL" : "PASS");
	printf("Test #2\n%s\n", (test_llc_evict()) ? "FAIL" : "PASS");
	printf("---------- End of Program ----------\n");


	return 0;
}
