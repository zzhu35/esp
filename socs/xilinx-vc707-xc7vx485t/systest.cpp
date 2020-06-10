#include <atomic>
using namespace std;

extern "C"
{
#include "uart.h"
#include "riscv_test.h"
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
		// memory_order_relaxed
    	// memory_order_consume
    	// memory_order_acquire
    	// memory_order_release
    	// memory_order_acq_rel
    	// memory_order_seq_cst

		// tmp = atom.fetch_add(0, std::memory_order_seq_cst); // atomic read
		atom.store(1, std::memory_order_seq_cst); // atomic write
		// atom.store(1, std::memory_order_seq_cst);
		// tmp = atom.load(std::memory_order_seq_cst);


		cprintint(tmp); cprintf("\n");

		riscv_sp_test();

	}

	return 0;
}