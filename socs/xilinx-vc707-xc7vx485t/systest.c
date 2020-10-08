#include <stdio.h>
#include <stdatomic.h>

volatile long long int lock = 1;
volatile long long int busy = 0;
volatile long long int tmp = 1;

int main(int argc, char **argv)
{
	// printf("Hello from Ariane!\n");

	// printf("Lock: %ld\n", lock);
	// atomic_fetch_add(&lock, 1);
	// printf("Lock: %ld\n", lock);
	// printf("End.\n");
	int i;
	unsigned char* mem = (unsigned char*)0xa0100330;
	printf("Start\n");
	for (i = 0; i < 32; i++)
	{
		printf("%c", mem[i]);
	}
	printf("\nStart write\n");

	for (i = 0; i < 32; i++)
	{
		mem[i] = '0' + i;
		printf("%c", mem[i]);
	}
	printf("\nEnd\n");

}
