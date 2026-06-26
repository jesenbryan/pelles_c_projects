#include <stdio.h>

int main(int argc, char *argv[])
{
	int int_num1;
	int int_num2;
	double num1;
	double num2;

	printf("Enter first number: ");
	scanf("%d", &int_num1);
	printf("Enter second number: ");
	scanf("%d", &int_num2);
	
	printf("Enter first number: ");
	scanf("%lf", &num1);
	printf("Enter second number: ");
	scanf("%lf", &num2);

	printf("Integer answer: %d\n", int_num1 + int_num2);
	printf("Float answer: %f\n", num1 + num2);

    return 0;
}
