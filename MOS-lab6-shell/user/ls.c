#include <lib.h>

int flag[256];

void lsdir(char *, char *);
void ls1(char *, u_int, u_int, char *);

void ls(char *path, char *prefix) {
	int r;
	struct Stat st;

	if ((r = stat(path, &st)) < 0) {
		user_panic("stat %s: %d", path, r);
	}
	if (st.st_isdir && !flag['d']) {
		lsdir(path, prefix);
	} else {
		ls1(0, st.st_isdir, st.st_size, path);
	}
}

void lsdir(char *path, char *prefix) {
	int fd, n;
	struct File f;

	if ((fd = open(path, O_RDONLY)) < 0) {
		user_panic("open %s: %d", path, fd);
	}
	while ((n = readn(fd, &f, sizeof f)) == sizeof f) {
		if (f.f_name[0]) {
			ls1(prefix, f.f_type == FTYPE_DIR, f.f_size, f.f_name);
		}
	}
	if (n > 0) {
		user_panic("short read in directory %s", path);
	}
	if (n < 0) {
		user_panic("error reading directory %s: %d", path, n);
	}
}

void ls1(char *prefix, u_int isdir, u_int size, char *name) {
	char *sep;

	if (flag['l']) {
		printf("%11d %c ", size, isdir ? 'd' : '-');
	}
	if (prefix) {
		if (prefix[0] && prefix[strlen(prefix) - 1] != '/') {
			sep = "/";
		} else {
			sep = "";
		}
		printf("%s%s", prefix, sep);
	}
	printf("%s", name);
	if (flag['F'] && isdir) {
		printf("/");
	}
	printf(" ");
}

void usage(void) {
	printf("usage: ls [-dFl] [file...]\n");
	exit(1);
}

int main(int argc, char **argv) {
	int i;
	char cur_path[1024];
	syscall_get_rpath(cur_path);

	ARGBEGIN {
	default:
		usage();
	case 'd':
	case 'F':
	case 'l':
		flag[(u_char)ARGC()]++;
		break;
	}
	ARGEND

	if (argc == 0) {
		ls(cur_path, "");
	} else {
		for (i = 0; i < argc; i++) {
			char new_path[1024];
			parse_rpath(argv[i], new_path);
			// ls(argv[i], argv[i]);
			ls(new_path, new_path);
		}
	}
	printf("\n");
	return 0;
}
