extern "C"{
#include "uart.h"
void myprintf(){
    print_uart("Hello from ESP!\n");
}
}



int main(int argc, char **argv)
{
    myprintf();
	return 0;
}
