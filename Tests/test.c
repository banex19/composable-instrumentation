#include <stdio.h>

volatile int x = 0;


void longRunningFunction()
{
	for (int i = 0; i < 100000000; ++i)
	{
		x = i + 2;
	}
}

int main()
{
	printf("Hey\n");

	longRunningFunction();

	puts("Bye");

	return 0;
}
