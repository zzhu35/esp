extern "C"{
#include "uart.h"
void myprintf(const char* str){
    print_uart(str);
}
}

#include <atomic>
using namespace std;

int main(int argc, char **argv)
{
    myprintf("Hello from ESP!\n");
	int i, tmp, tmp2;
	volatile int* c = (volatile int*)&tmp;
	volatile int* d = (volatile int*)&tmp2;

	*c = 0;
	for (i = 0; i < 10; i++)
	{
		*c = *c + 1;
		__asm__ volatile ("fence" : : : "memory");
		// asm volatile (
		// " amoswap.w.aq t0, t0, (%1);"
		// : "=&r"(d)
		// : "0" (d)
		// : "memory"
		// );
	}
	if (*c == 10)
		myprintf("C == 10.\n");
	else
		myprintf("C is wrong.\n");
	
	return 0;
}