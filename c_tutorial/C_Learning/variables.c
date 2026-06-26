#include <stdio.h>

int main(int argc, char *argv[])
{
	char characterName[] = "Jesen"; // [] -> bunch of characters
	int characterAge = 26;

    printf("There once was a man named %s\n", characterName);
	printf("he was %d years old.\n", characterAge);

	characterAge = 30;
	printf("He really liked the name %s\n", characterName);
	printf("but did not like being %d.\n", characterAge);

    return 0;
}

