#include <stdio.h>

int main(int argc, char *argv[])
{
	int age = 30;
	int * pAge = &age;

	printf("age's memory address: %p\n", &age);
	printf("age's memory address: %p\n", pAge);
	printf("dereferencing pointers: %d\n", *pAge);

	printf("this %d is the same as %d\n", age, *&age); // dereferencing
    return 0;
}
