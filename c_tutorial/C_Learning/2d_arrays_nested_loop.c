#include <stdio.h>

int main(int argc, char *argv[])
{
	//int nums[3][2];
	int nums[3][2] = {
					{1, 2}, 
					{3, 4},
					{5, 6},
				    };
	//printf("%d\n", nums[0][1]);
	
	int i, j;
	for(i = 0; i<3; i++){
		for(j = 0; j<2; j++){
			printf("%d\n", nums[i][j]);
		}
	}

    return 0;
}
