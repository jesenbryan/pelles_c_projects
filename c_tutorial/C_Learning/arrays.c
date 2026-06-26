#include <stdio.h>

int main(int argc, char *argv[])
{
	//int luckyNumbers[] = {4, 7, 17, 42};
	//printf("%d\n", luckyNumbers[2]); // no ampersand / pointer?
	//luckyNumbers[2] = 200;
	//printf("%d\n", luckyNumbers[2]);

	int luckyNumbers[10];
	luckyNumbers[1] = 80;
	luckyNumbers[0] = 90;
	printf("%d\n", luckyNumbers[1]);
	printf("%d\n", luckyNumbers[0]);
    return 0;
}
