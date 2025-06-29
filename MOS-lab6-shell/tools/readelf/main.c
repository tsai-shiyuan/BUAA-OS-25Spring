#include <stdio.h>
#include <stdlib.h>

// extern表示在其他地方定义，但当前文件并不包含该定义
extern int readelf(void *binary, size_t size);	// 来自readelf.c

/*
	Open the ELF file specified in the argument, and call the function 'readelf'
	to parse it.
	Params:
		argc: the number of parameters
		argv: array of parameters, argv[1] should be the file name.
*/
int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <elf-file>\n", argv[0]);
		return 1;
	}

/* Lab 1 Key Code "readelf-main" */
	FILE *fp = fopen(argv[1], "rb");	// read-only binary
	if (fp == NULL) {
		perror(argv[1]);	// print error(in <stdio.h>)
		return 1;
	}

	if (fseek(fp, 0, SEEK_END)) {	// 移动文件指针到文件尾 SEEK_END, fseek失败返回non-zero
		perror("fseek");
		goto err;
	}
	int fsize = ftell(fp);	// 获取文件指针当前位置,即文件大小(因为指针在文件尾)
	if (fsize < 0) {
		perror("ftell");
		goto err;
	}

	char *p = malloc(fsize + 1);
	if (p == NULL) {
		perror("malloc");
		goto err;
	}
	if (fseek(fp, 0, SEEK_SET)) {	// 移回文件开头
		perror("fseek");
		goto err;
	}
	if (fread(p, fsize, 1, fp) < 0) {	// 读取文件数据,存入p
		perror("fread");
		goto err;
	}
	p[fsize] = 0;	// '\0'
/* End of Key Code "readelf-main" */

	return readelf(p, fsize);
err:
	fclose(fp);
	return 1;
}
