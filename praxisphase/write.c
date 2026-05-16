#include <stdio.h>

void writeData() {
    FILE *f = fopen("data.txt", "w");

    for (int i = 0; i <= 10; i++) {
        fprintf(f, "%d %d\n", i, i*i);
    }

    fclose(f);
    return 0;
}
