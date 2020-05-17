#include <atomic>
using namespace std;

extern "C"
{
#include "uart.h"
void cprintf(const char* str)
{
    print_uart(str);
}
void cprintint(uint32_t i)
{
	print_uart_int(i);
}
}

void fence()
{
	__asm__ volatile ("fence" : : : "memory");

}

int main(int argc, char **argv)
{
	char str[50];
	int tmp, tmp2;
    cprintf("Hello from CPP!\n");
	volatile int* c = (volatile int*)&tmp;
	volatile int* d = (volatile int*)&tmp2;

	atomic<int> atom(0);

	while(true)
	{
		atom.fetch_add(1);
    	cprintf("step\n");
		tmp = atom.load();
		cprintint(tmp); cprintf("\n");

	}

	return 0;
}