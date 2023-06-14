#include <stdio.h>
#include <unistd.h>
typedef int (*printf_ptr_t)(const char *format, ...);

void solver(printf_ptr_t fptr) {
	char tmp[24] = {'\0'};
	fptr("%016lx %016lx %016lx ", *((long*)(tmp + 24)), *((long*)(tmp + 24 + 8)), *((long*)(tmp + 24 + 8 + 8)));
}

int main() {
	char fmt[16] = "** main = %p\n";
	printf(fmt, main);
	solver(printf);
	return 0;
}
