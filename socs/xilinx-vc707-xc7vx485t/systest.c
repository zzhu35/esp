#include <stdio.h>
#include <stdatomic.h>

volatile long long int lock = 1;
volatile long long int busy = 0;
volatile long long int tmp = 1;

int main(int argc, char **argv)
{
	printf("Hello from Ariane!\n");

	printf("Lock: %ld\n", lock);
	atomic_fetch_add(&lock, 1);
	printf("Lock: %ld\n", lock);
	printf("End.\n");

}
