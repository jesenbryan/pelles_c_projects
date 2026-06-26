#include <stdio.h>

int main(int argc, char *argv[])
{
	int secretNumber = 5;
	int guess;
	int guessCount = 0;
	int guessLimit = 3;
	int outOfGuesses = 0;

	while (guess != secretNumber && outOfGuesses == 0){
		if (guessCount < guessLimit){
			printf("Enter secret number: ");
			scanf("%d", &guess);
			guessCount++;
		} else {
			outOfGuesses = 1;
			break;
		}
	}
	if (outOfGuesses == 1){
		printf("Out of Guesses\n");
	} else {
		printf("You win!\n");
	}

    return 0;
}
