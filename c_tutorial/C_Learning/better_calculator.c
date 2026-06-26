#include <stdio.h>

int main(int argc, char *argv[])
{
	double num1;
	double num2;
	char op;

	printf("Enter a number: ");
	scanf("%lf", &num1);
	printf("Enter operator: ");
	scanf(" %c", &op); // space before %c
	printf("Enter a number: ");
	scanf("%lf", &num2);

	if(op == '+'){
		printf("%f", num1 + num2);
	} else if (op == '-'){
		printf("%f", num1 - num2);
	} else if (op == '*'){
		printf("%f", num1 * num2);
	} else if (op == '/'){
		printf("%f", num1 / num2);
	} else{
		printf("Invalid operator");
	}
	
    return 0;
} 
