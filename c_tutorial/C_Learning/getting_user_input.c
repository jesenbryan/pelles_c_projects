#include <stdio.h>
#include <stdlib.h>

int main()
{
	int age;
	double gpa;
	char grade;
	char name[20];

	printf("Enter your name: ");
	//scanf("%s", name); // doesnt work for John Smith because of white space
	fgets(name, 20, stdin); // automatically adds a new line 
	printf("Enter your age: ");
	scanf("%d", &age);
	printf("Enter your gpa: ");
	scanf("%lf", &gpa);
	printf("Enter your grade: ");
	scanf(" %c", &grade);

	printf("Your name is %s\n", name); 
	printf("You are %d years old\n", age);
	printf("Your gpa is %.2f\n", gpa);
	printf("Your grade is %c\n", grade);

    return 0;
}
