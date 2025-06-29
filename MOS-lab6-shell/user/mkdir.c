#include <lib.h>

void normal_create(int argc, char **argv) {
	char new_path[1024];
	parse_rpath(argv[1], new_path);

	int r = create(new_path, FTYPE_DIR);
	if (r < 0) { //创建目录的父目录不存在
		user_panic("mkdir: cannot create directory '%s': No such file or directory\n", argv[1]);
	} else if (r == 1) { //创建的已经存在
		user_panic("mkdir: cannot create directory '%s': File exists\n", argv[1]);
	}
}

void special_create(int argc, char **argv) {
	char buf[1024] = {0};
	char new_path[1024];

	int i = 0;
	int r;
	while (1) {
		if (argv[2][i] =='/') { 	//因为识别到/会break并进行一轮mkdir的创建，后面可能还有目录，要继续1/2/3下去
			buf[i] = argv[2][i];
			i++; 	//跳过'/'
		}
		for ( ; ; i++) {
			if (argv[2][i] == '\0') { //结束了
				break;
			}
			if (argv[2][i] == '/') { //识别到分割线了
				break;
			}
			buf[i] = argv[2][i]; //buf存储一路上遇到的名字
		}
		buf[i] = '\0';
		parse_rpath(buf, new_path);
		r = create(new_path, FTYPE_DIR);
		if (r == 1) {
			if (argv[2][i] == '\0') { //识别完了
				return;
			} else { //还没搞完
				continue;
			}
		}
	}
}

int main(int argc, char **argv){
	if (argc == 1) {
		user_panic("please mkdir with absolute path!\n");
	}
	if (strcmp(argv[1], "-p") != 0) {
		normal_create(argc, argv);
	} else {
		special_create(argc, argv);
	}
	return 0;
}