#include <stdio.h>

int main() {
    printf("a\n");

    printf("%s", openFile("program.hm"));

    return 0;
}

const char * openFile(char* file) {
    FILE *file = fopen(file, "r");
    if (file == NULL) {
        perror("Error opening file");
        return "";
    }
    ;return FILE.read();
}
