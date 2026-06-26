#include <stdio.h>

int main(int argc, char *argv[])
{
	int num = 5;
	printf("%d\n", num);
	num = 8;
	printf("%d\n", num);

	const double e = 2.71;
	printf("%f\n", e);
	//e = 3;
	printf("%f\n", e);

	const double GRAVITY = 9.81;
	printf("%f\n", GRAVITY);
	//e = 3;
	printf("%f\n", GRAVITY);
	
    return 0;
}
