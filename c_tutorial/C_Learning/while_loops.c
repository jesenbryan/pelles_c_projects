#include <stdio.h>

int main(int argc, char *argv[])
{
	int index = 1;
	int index1 = 1;
	while(index <= 5){
		printf("%d\n", index);
		index++;
	}

	do {
		printf("%d\n", index1);
		index1++;
	}
		while(index1 <= 5);

    return 0;
}
