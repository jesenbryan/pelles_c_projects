#include <stdio.h>

int main(int argc, char *argv[])
{
	char line[255];
	FILE * fpointer = fopen("employees.txt", "r"); // r->read, w->write, a->append

	fgets(line, 255, fpointer);
	fgets(line, 255, fpointer);
	printf("%s", line);

	fclose(fpointer);
    return 0;
}
