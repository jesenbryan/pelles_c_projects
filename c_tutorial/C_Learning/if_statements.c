#include <stdio.h>

int max(int num1, int num2, int num3);


int main(int argc, char *argv[])
{
	printf("%d\n", max (10, 2, 3)); // || logical OR, ! logical not or negation
    return 0;
}

int max(int num1, int num2, int num3){
	int result;
	if (num1 >= num2 && num1 >= num3){
		result = num1;
	} else if (num2 >= num1 && num2 >= num3){
		result = num2;
	} else {
		result = num3;
	}
	return result;
}
