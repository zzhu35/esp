#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "defines.h"

static volatile int thread_proceed = 0;

int main(int argc, char **argv)
{
	mptest_start(0x80000200);
	int id = get_pid();

	if (id)
	{
		while (!thread_proceed);

		printf("Core 1\n");
		thread_proceed = 0;
		return 0;

	}

	printf("Core 0\n");

	thread_proceed = 1;
	while (thread_proceed);

	printf("----------End of Multicore Test-------------\n");

	return 0;
}
