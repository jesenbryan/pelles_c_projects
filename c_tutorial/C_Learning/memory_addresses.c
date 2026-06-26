#include <stdio.h>

int main(int argc, char *argv[])
{
	int age = 30;
	double gpa = 3.4;
	char grade = 'A';
	
	printf("age: %p\ngpa: %p\ngrade: %p\n", &age, &gpa, &grade);
	//printf("%p\n", &gpa);
	//printf("%p\n", &grade);

    return 0;
}
