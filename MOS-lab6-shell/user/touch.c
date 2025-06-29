#include <lib.h>

int main(int argc, char **argv){
	if (argc == 1) {
        user_panic("please touch a file with absolute path!\n");
    } else {
        char new_path[1024];
        parse_rpath(argv[1], new_path);

        int r = create(new_path, FTYPE_REG);
        if (r < 0) { //创建不成功
            user_panic("touch: cannot touch '%s': No such file or directory\n", argv[1]);
        }
        return 0;
    }
}