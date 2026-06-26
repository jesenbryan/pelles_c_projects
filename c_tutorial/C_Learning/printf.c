#include <stdio.h>

int main(int argc, char *argv[])
{
	int favNum = 90;
	char myChar = 'i';

	printf("Hello\n World\n");
	printf("Hello\" World\n");
	printf("My favorite %s is %d\n", "number", 500);
	printf("My favorite %s is %f\n", "number", 500.4631);
	printf("My favorite %s is %d\n", "number", favNum);
	printf("My favorite %c is %d\n", myChar, favNum);
    return 0;
}

