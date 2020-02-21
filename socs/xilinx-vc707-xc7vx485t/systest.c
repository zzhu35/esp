
#ifdef __riscv
#include "uart.h"
#else
#include <stdio.h>
#endif

int main(int argc, char **argv)
{
#ifdef __riscv
#else
	printf("Leon3 on ESP\n");
	printf("Spandex Inside\n");
#endif

	return 0;
}
