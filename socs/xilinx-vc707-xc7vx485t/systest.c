#ifdef __riscv

#include "uart.h"

int main(int argc, char **argv)
{
	print_uart("Hello from C!\n");
	print_uart("Hello from Spandex!\n");
	print_uart("Hello from Ariane!\n");
	print_uart("Hello from LLC AMO!\n");

	return 0;
}

#else
#include <stdio.h>

int main(int argc, char **argv)
{
	printf("Hello from Leon3!\n");
}

#endif