#include <lib.h>

int main(int argc, char **argv) {
    if (argc > 1) {
        printf("pwd: expected 0 arguments; got %d\n", argc - 1);
        return 2;
    } else {
        char path[1024];
        getcwd(path);
        printf("%s\n", path);
        return 0;
    }
}