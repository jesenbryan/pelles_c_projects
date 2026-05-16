#include <stdio.h>

void readData() {
    FILE *f = fopen("data.txt", "r");
    char line[100];

    while (fgets(line, sizeof(line), f)) {
        printf("%s", line);
    }

    fclose(f);
}
