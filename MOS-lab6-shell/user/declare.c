#include <lib.h>

int flag[256];

void usage(void) {
	printf("usage: declare error\n");
	exit(1);
}

int main(int argc, char **argv) {
    ARGBEGIN {
    default:
        usage();
	case 'x':
	case 'r':
        flag[(u_char)ARGC()]++;
	}
	ARGEND

    if (argc > 2) {
        return -1;
    }

    int share = 0;
    int rdonly = 0;
    if (flag['x']) {
        share = 1;
    }
    if (flag['r']) {
        rdonly = 1;
    }

    if (argc == 0) {    // declare
        char values[1024];
        syscall_get_shell_var(0, values);
        debugf("%s", values);
    } else {
        char *value = "";
        if (argc == 2) {
            /*
            if (argv[1][0] == '=') {
                value = argv[1] + 1;
            }
            */
           value = argv[1];
        }
        int r = syscall_declare_shell_var(argv[0], value, share, rdonly);
        if (r < 0) {
            debugf("environment var declaration error\n");
        } else {
            debugf("declare success: %s, %s\n", argv[0], value);
        }
    }
}