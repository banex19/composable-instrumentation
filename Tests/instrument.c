#include  <stdio.h>

extern "C" {

void __instrument_before_call(char* fnName)
{
	printf("[instrument.c] Instrumented before call to %s\n", fnName);
}

void __instrument_after_call(char* fnName)
{
	printf("[instrument.c] Instrumented after call to %s\n", fnName);
}
}