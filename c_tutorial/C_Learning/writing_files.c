#include <stdio.h>

int main(int argc, char *argv[])
{
	FILE * fpointer = fopen("employees.txt", "a"); // r->read, w->write, a->append
	//fprintf(fpointer, "Jim, Salesman\nPam, Receptionist\nOscar, Accounting");
	//fprintf(fpointer, "Overridden"); // this line wil override the file
	fprintf(fpointer, "\nKelly, Customer Service"); // to append
	fclose(fpointer);
    return 0;
}
