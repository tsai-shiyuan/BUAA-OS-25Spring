#include <lib.h>

void usage(void) {
	printf("usage: unset error\n");
	exit(1);
}

int main(int argc, char **argv) {
    if (argc != 1) {
        debugf("args count is wrong");
        return -1;
    }
    int r = syscall_unset_shell_var(argv[0]);
    if (r < 0) {
        debugf("environment variable unset error\n");
        return -1;
    }
    return 0;
}