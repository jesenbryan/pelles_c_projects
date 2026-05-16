#include <stdlib.h>
#include "io.h"

int main(void) {
    writeData();
    readData();
	system("\"C:\\Program Files\\gnuplot\\bin\\gnuplot.exe\" -persistent plot.plt");
//    system("gnuplot.exe -persistent plot.plt");
    return 0;
}
