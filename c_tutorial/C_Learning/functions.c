#include <stdio.h>

void sayHi(char name[], int age);   // prototype

int main(int argc, char *argv[])
{
	sayHi("A", 40);
	sayHi("Jesen", 20);
	sayHi("B", 73);
    return 0;
}

void sayHi(char name[], int age){
	printf("Hello %s, you are %d\n", name, age);
}
