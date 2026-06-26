#include <stdio.h>

double cube(double num)

int main(int argc, char *argv[])
{
	printf("Answer: %f\n", cube(7.0));	
    return 0;
}

double cube(double num){
	//double result = num * num * num;
	//return result;
	return num * num * num; // return keyword breaks out of the function
	printf("test"); // this will not get printed
}
