#include <stdio.h>
#include <math.h>

int main(int argc, char *argv[])
{
	int num = 6;
	printf("%f \n", 8.9);
	printf("%f \n", 5.0 + 4.5);
	printf("%f \n", 5.0 * 4.5);
	
	printf("%f \n", 5 + 4.5);
	//printf("%f \n", 5 + 4); //Argument 2 to 'printf' does not match the format string; expected 'double' but found 'int'.
	printf("%d \n", 5 / 4);
	printf("%f \n", 5 / 4.0);
	printf("%d \n", num);

	printf("%f \n", pow(2, 3));
	printf("%f \n", sqrt(36));
	printf("%f \n", ceil(36.1));
	printf("%f \n", floor(36.9));
    return 0;
}

