#include <stdio.h>
#include <stdlib.h>

#define NO_LOGGING

#ifdef NO_LOGGING
	#define log(...) 
#else
	#define log(...) printf(__VA_ARGS__)
#endif

static inline void err(const char* a)
{
	puts(a);
	fflush(stdout);
	exit(1);
}

static inline void printhex(const char* a, int len)
{
#ifndef NO_LOGGING
	for (int i = 0; i < len; i++)
		printf("%x ", a[i]);
	printf("\n");
#endif
}