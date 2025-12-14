#include <stdio.h>

int main() {
    printf("a\n");

    // prints out what openFile returns
    printf("%s", openFile("program.hm"));

    return 0;
}

//should return the contents of the file
const char * openFile(char* file) {
    FILE *file = fopen(file, "r");
    if (file == NULL) {
        perror("Error opening file");
        return "";
    }
    ;return FILE.read();
}