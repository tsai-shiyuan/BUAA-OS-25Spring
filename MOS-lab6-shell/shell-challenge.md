# Shell Challenge

我们在 Lab 6 中完成的 Shell 只有 cat, ls 等基础命令，Shell Challenge 要求我们实现更丰富的命令。个人完成时间约为 35 个小时，前前后后花了 5 天。感觉测试样例偏弱，只要在本地能正常执行给出的样例基本就能通过测试？如果提交不通过，最好是检查细节而不是代码的逻辑。

添加指令的基本流程是在 user 里写这个指令的主函数，再在 user/include.mk 里添加 `newcommand.b` (为了本地运行)。

```c
	USERAPPS     := echo.b \
                    halt.b \
                    ls.b \
                    sh.b \
                    cat.b \
   					newcommand.b \
                    init.b
```

在 user/new.mk 中也要添加 (为了评测)。

```c
USERAPPS += touch.b mkdir.b rm.b pwd.b declare.b unset.b history.b
```

debug 主要采用 print 大法，内核态用 `printk` ，用户态用 `debugf` 或 `print` 。最后使用 `make test lab=6_2 && make run` 启动我们的 shell。

## 基础功能

### 不带 `.b` 后缀的命令

Shell 进程不断地读取命令，然后 fork 出子进程来执行命令。runcmd 函数会使用 spawn 函数调用文件系统中的可执行文件并执行。

```bash
main
 └─> readline → 获取命令
 └─> fork
     ├─ 子进程 → runcmd(buf) → 解析命令并执行
     └─ 父进程 → wait 子进程
```

只需要在 spawn 失败后加上 .b 再试一次。

```c
void runcmd(char *s) {
	// ...
	int child = spawn(argv[0], argv);

    /* 修改的部分 */
	if (child < 0) {
		char new_cmd[1024] = {0};
		strcpy(new_cmd, argv[0]);
		int len = strlen(argv[0]);
		new_cmd[len] = '.';
		new_cmd[len + 1] = 'b';
		new_cmd[len + 2] = '\0'; // 将ls扩展成ls.b
		child = spawn(new_cmd, argv);
	}

	close_all();
    // ...
}
```

### 注释

> 例如 `ls | cat # this is a comment meow`，`ls | cat` 会执行，而后面的注释则会被抛弃

识别到 `#` 就认为这行结束了，将 `#` 加进 SYMBOLS 宏，parsecmd 遇到 `#` 就 return

```c
#define SYMBOLS "<|>&;()#"
```

```c
int parsecmd(char **argv, int *rightpipe) {
	while (1) {
		switch (c) {
		case 0:
		case '#':
			return argc;
		// ...
```

### 一行多指令

> 使用 `;` 将指令隔开，并按顺序执行

当解析到 `;` 时，`fork` 一个子 Shell，让子 Shell 继续执行左边的命令，父 Shell 等待子 Shell 执行完成后，执行 `;` 右边的命令。

```c
int parsecmd(char **argv, int *rightpipe) {
	while (1) {
        switch(c) {
        // ...
        /* 新增的部分 */        
    	case ';':
			int child = fork();
			if (child == 0) {
				return argc;
			} else if (child > 0) {
				wait(child);
				close(0);
				close(1);
				dup(opencons(), 1);		// 打开一个终端文件并设置为标准输出
				dup(1, 0);		// 输入同样来自终端
				return parsecmd(argv, rightpipe);
			}
			break;
		// ...                
```

### 反引号

>将反引号内指令执行的所有标准输出代替原有指令中的反引号内容。例如:
>
>```bash
>echo `ls | cat | cat | cat`
>```

在 `runcmd` 函数执行之前，先替换反引号内的字符内容为执行结果。

```c
int executeCommandAndCaptureOutput(char *cmd, char *output, int maxLen);
int replaceBackquoteCommands(char *cmd);
```

### exit

> 内建指令，执行后退出当前shell

当识别到 exit 指令时，直接调用 exit 就可以了。

```c
// user/sh.c
int main(int argc, char **argv) {
    for (;;) {
        if (is_exit(buf)) {
			exit(0);	// 0 表示正常退出，见条件指令
		}
```

## 支持相对路径

>为每个进程维护**工作目录**这一状态，实现相关内建指令，并为其他与路径相关的指令提供路径支持。
>
>- **工作目录:** 进程当前所在的目录，用于解析相对路径
>- **绝对路径:** 以 `/` 开头的路径，从根目录开始
>- **相对路径:** 不以 `/` 开头的路径，**相对于当前工作目录**解析，可能包含以下特殊符号
>- **特殊符号:**  `.` 表示当前路径，`..` 表示上一级路径

实现方式:

- 在内核态维护进程的当前目录 `r_path`
- 通过系统调用向用户态提供更改 `r_path` 的接口，实现用户调用函数 `chdir()` 和 `getcwd()`
- 更改 `sys_exofork` 逻辑，使子进程继承父进程的工作目录
- 修改 `sh.c` 实现内部命令 `cd` 和外部命令 `pwd`

### 保存路径

```c
// kern/env.c
struct Env {
    char r_path[256];
};

struct Env *env_create(const void *binary, size_t size, int priority) {
    /* 新增的部分 */
    e -> r_path[0] = '/';
}

// kern/syscall_all.c
int sys_exofork(void) {
    // ...
	strcpy(e->r_path, curenv->r_path);
    return e->env_id;
}
```

```c
// include/syscall.h
enum {
    SYS_get_rpath,
	SYS_set_rpath,
	MAX_SYSNO,
};

// kern/syscall_all.c
int sys_get_rpath(char *dst) {
	if (dst == 0) {
		return -1;
	}
	strcpy(dst, curenv->r_path);
	return 0;
}

int sys_set_rpath(char *newPath) {
	if (strlen(newPath) > 1024) {
		return -1;
	}
	strcpy(curenv->r_path, newPath);
    return 0;
}

void *syscall_table[MAX_SYSNO] = {
    /* 新增的部分 */
    [SYS_get_rpath] = sys_get_rpath,
	[SYS_set_rpath] = sys_set_rpath,
};

// user/include/lib.h
int syscall_get_rpath(char *dst);
int syscall_set_rpath(char *newPath);
int chdir(char *newPath);
int getcwd(char *path);

// user/lib/syscall_lib.c
int syscall_get_rpath(char *dst) {
	return msyscall(SYS_get_rpath, dst);
}

int syscall_set_rpath(char *newPath) {
	return msyscall(SYS_set_rpath, newPath);
}

// user/lib/file.c
int getcwd(char *path) {
	return syscall_get_rpath(path);
}

int chdir(char *newPath) {
	return syscall_set_rpath(newPath);
}
```

### 相对路径

我们的文件系统操作，如 open 等，接收到的参数是**绝对路径**，相对路径应当首先被解析，再作为参数传入，写一个解析相对路径为绝对路径的函数 `parse_rpath` ，思路很简单，就是以进程现在的目录为基础，读相对路径，读到 `.` 就不变，读到 `..` 就退到上一级目录，最后把相对路径剩余的部分拼接到现在的目录就可以。

```c
// user/include/lib.h
void parse_rpath(char *old_path, char *new_path);

// user/lib/file.c
void parse_rpath(char *old_path, char *new_path) {
    ...
}
```

什么时候解析路径呢？其实最简单就是由 shell 进程解析，但是在路径错误的时候要求输出原路径，所以就在 open, close, read, write 等文件操作**之前**进行解析。对应地，我们需要对之前实现过的命令进行修改，例如 cat.c 中:

```c
			parse_rpath(argv[i], new_path);		// 解析
			f = open(new_path, O_RDONLY);
```

### cd 和 pwd

cd 是内部指令，我们要直接改变当前 shell 进程的路径，而不能 fork 子进程去操作，因为父进程接收不到子进程的路径变化。修改 user/sh.c。

```c
int parseCD(char *buf);		// 命令中是否有 cd

int main(int argc, char **argv) {
    	/* 新增的部分 */
    	if (parseCD(buf) == 1) {
			runcmd(buf);	// 由当前shell进程自己run
			continue;
		}
}

void runcmd(char *s) {
    /* 新增 */
    if (strcmp("cd", argv[0]) == 0) {
		useCD(argc, argv[1]);
		return;
	}
}

void useCD(int argc, char* argv) {
	int r;
	char cur[1024];
	struct Stat st = {0};

	if (argc == 1) {
		cur[0] = '/';
	} else {
		parse_rpath(argv, cur);
	}

	if ((r = stat(cur, &st)) < 0) { 
		printf("cd: The directory \'%s\' does not exist\n", argv);
		return;
	}
	if (!st.st_isdir) {
		printf("cd: \'%s\' is not a directory\n", argv);
		return;
	}
	if ((r = chdir(cur)) < 0) { 
		printf("6");
	}
	return;
}
```

pwd 的实现非常简单，直接新建 user/pwd.c 即可:

```c
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
```

## 文件系统

### 创建文件

>`touch <file>` 会创建空文件 `file`，若文件存在则放弃创建，正常退出无输出
>
>若创建文件的父目录不存在则输出 `touch: cannot touch '<file>': No such file or directory`

`fs/fs.c` 中已经预留了创建文件的函数，但是没有向用户态提供接口。我们可以通过新增 `fsipc` 类型从而让文件服务函数调用这个接口，实现文件的创建。

```c
// fs/serv.c
void serve_create(u_int envid, struct Fsreq_create *rq){
	int r;
	struct File *f;
	if ((r = file_create(rq->req_path, &f)) < 0) {	// fs.c提供了 file_create 函数
		ipc_send(envid, r, 0, 0);
		return;
	}
	f->f_type = rq->f_type;
	ipc_send(envid, 0, 0, 0);
}

void *serve_table[MAX_FSREQNO] = {
    [FSREQ_CREATE] = serve_create,
};
```

```c
//user/include/fsreq.h
enum {
	FSREQ_CREATE,
	MAX_FSREQNO,
};

struct Fsreq_create{
	char req_path[MAXPATHLEN];
	u_int f_type;
};
```

```c
//user/lib/file.c
int create(const char *path, int f_type){	//touch.c调用
	int r;
	struct Fd *fd;
	try(fd_alloc(&fd));
	if ((r = fsipc_open(path, O_RDONLY, fd)) < 0) {		//如果文件不存在，就创建
		return fsipc_create(path, f_type, fd);
	} else {	//文件存在，直接返回
		return 1;
	}
}
```

```c
//user/lib/fsipc.c
int fsipc_create(const char *path, u_int f_type, struct Fd *fd){
	u_int perm;
	struct Fsreq_create *req;
	req = (struct Fsreq_create *)fsipcbuf;
	if (strlen(path) >= MAXPATHLEN) {
		return -E_BAD_PATH;
	}
	strcpy((char *)req->req_path, path);
	req->f_type = f_type;
	return fsipc(FSREQ_CREATE, req, fd, &perm);
}
```

最后完成 touch.c:

```c
// touch.c
#include <lib.h>
int main(int argc, char **argv){
	if (argc == 1) {
        user_panic("please touch a file with absolute path!\n");
    } else {
        char new_path[1024];
        parse_rpath(argv[1], new_path);		// 也进行了相对路径解析

        int r = create(new_path, FTYPE_REG);
        if (r < 0) { //创建不成功
            user_panic("touch: cannot touch '%s': No such file or directory\n", argv[1]);
        }
        return 0;
    }
}
```

### 创建文件夹

> `mkdir <dir>`：若目录已存在则输出 `mkdir: cannot create directory '<dir>': File exists`，若创建目录的父目录不存在则输出 `mkdir: cannot create directory '<dir>': No such file or directory`，否则正常创建目录
>
> `mkdir -p <dir>`：当使用 `-p` 选项时忽略错误，若目录已存在则直接退出，若创建目录的父目录不存在则递归创建目录

普通的创建和 touch 类似，只是传入的文件类型改为了 `FTYPE_DIR`。递归的创建，比如 `mkdir -p /a/b/c` ，就像 `/a` $\rightarrow$ `/a/b` $\rightarrow$ `/a/b/c` 这样一层一层地新建就可以。user/mkdir.c:

```c
#include <lib.h>
void normal_create(int argc, char **argv) {
	char new_path[1024];
	parse_rpath(argv[1], new_path);
	int r = create(new_path, FTYPE_DIR);
	if (r < 0) {	//创建目录的父目录不存在
		user_panic("mkdir: cannot create directory '%s': No such file or directory\n", argv[1]);
	} else if (r == 1) {	//创建的已经存在
		user_panic("mkdir: cannot create directory '%s': File exists\n", argv[1]);
	}
}

void special_create(int argc, char **argv) {
    
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
```

### 删除文件

调用 remove 函数就可以了，还要注意 close(fd)

```c
#include <lib.h>

int flag_r;
int flag_f;

void rm(char *path) {
    char new_path[1024];
    parse_rpath(path, new_path);

    int fd;
    struct Stat st;
    if ((fd = open(new_path, O_RDONLY)) < 0) {
        if (!flag_f) {
            user_panic("rm: cannot remove \'%s\': No such file or directory\n", path);
        }
        return;
    }
    close(fd);
    stat(new_path, &st);
    if (st.st_isdir && !flag_r) {
        user_panic("rm: cannot remove \'%s\': Is a directory\n", path);
    }
    remove(new_path);
}

int main(int argc, char **argv) {
    char s_r[5] = "-r";
    char s_rf[5] = "-rf";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], s_r) == 0) {
            argv[i] = 0;
            flag_r = 1;
        } else if (strcmp(argv[i], s_rf) == 0) {
            argv[i] = 0;
            flag_f = 1;
            flag_r = 1;
        }
    }

    if (argc < 2) {
        user_panic("nothing to rm\n");
    } else {
        for (int i = 1; i < argc; ++i) {
            if (argv[i] == 0) {
                continue;
            }
            rm(argv[i]);
        }
    }
}
```

### 追加重定向

需要支持:

```bash
ls >> file1
```

文件打开默认是 `f_offset = 0` ，追加模式只将 `f_offset` 设置成 `f_size` 就可以了。

```c
// user/include/lib.h
#define O_APPEND 0x2000

// fs/serv.c
void serve_open(u_int envid, struct Fsreq_open *rq) {
	o->o_mode = rq->req_omode;
	ff->f_fd.fd_omode = o->o_mode;
	ff->f_fd.fd_dev_id = devfile.dev_id;
    /* 新增的部分 */
    if (o->o_mode & O_APPEND) {
		struct Fd *fff = (struct Fd *)ff;
		fff -> fd_offset = f->f_size;
	}
}
```

实现和 `>` 差不多，

```c
int parsecmd(char **argv, int *rightpipe) {
    	/* 新增 */
		case '>':
			int tok = gettoken(0, &t);
			if (tok == '>') {
				if (gettoken(0, &t) != 'w') {
					debugf("syntax error: > not followed by word\n");
					exit(1);
				}
				parse_rpath(t, new_p);
				if ((fd = open(new_p, O_APPEND | O_WRONLY | O_CREAT)) < 0) {
					debugf("error opening\n");
					exit(1);
				}
				dup(fd, 1);
				close(fd);
				break;
			}
}
```

## 环境变量

> ```bash
> declare [-xr] [NAME [=VALUE]]
> ```
> - `-x` 表示 `NAME` 为环境变量，否则为局部变量
>   - **仅**环境变量可以传入子Shell中（在Shell中输入 `sh.b` 启动一个子Shell后，可以读取并修改 `NAME` 的值）
>   - `-r` 表示设置 `NAME` 只读，不能被 `declare` 重新赋值或被 `unset` 删除
>   - 环境变量在命令中以 `$` 开头

在 env.h 中，定义 EnvVar 结构体，所有的 EnvVar 以数组的形式存储在内核空间。关键的字段有 shell_id，它表示环境变量属于哪个 Shell。

```c
struct EnvVar {
    char name[MAX_VAR_LEN];
    char value[MAX_VAR_LEN];    // 键值对
    int shell_id;   // 标志 Shell
    int read_only;  // 只读
    int valid;      // 有效,主要看是不是 unset 过了
    int share;      // 是否能共享给子 Shell
};

// 保存所有的环境变量
struct EnvVar envvars[MAX_VAR_NUM] __attribute__((alligned(PAGE_SIZE)));

struct Env {
    int shellid;
};
```

我们先区分一下子进程和子 Shell。子进程由 fork 创建，其应当保留父进程的 shell_id，而且其对环境变量的操作应该对父进程可见。这主要是因为与环境变量相关的 declare 和 unset 指令是内建指令，而我们的 Shell 执行指令就是靠 fork 出子进程，再由子进程运行相关的指令，子进程执行完毕后就退出了，但我们的 Shell 进程还需要保留下子进程设置的环境变量信息。子进程这个比较好实现，就是在 fork 的时候也复制父进程的 shell_id，这样父子就视作使用同一个 Shell 的环境变量，实现共享。

而子 Shell 是在 Shell 进程中运行了 `sh` 命令而新建的另一个 Shell 进程，它只**继承父 Shell 的部分环境变量**，具体就是 declare 的时候具有参数 `-x` 的环境变量（下文称全局环境变量）。父子 Shell 的 shell_id 是不一样的。另外，子 Shell 对全局环境变量的**修改对父 Shell 不可见**。

怎么实现这一点呢？

- 直接把全局环境变量的 shell_id 设置为 0，子 Shell 进程可以使用 shell_id 为 0 的全局变量，但是这样子 Shell 进程对全局环境变量的修改就对父 Shell 可见了，NO
- 不要对 shell_id 赋予更多的含义，它就只指定环境变量属于哪个 Shell，再次强调，只有当前进程的 shellid 和EnvVar 的 shell_id 匹配时，进程才能读到 EnvVar。“继承，可以放心地第一次读，但是修改不可见……”，想到了 Copy On Write 机制。比如子 Shell 进程第一次尝试读/写全局环境变量，但是发现 shell_id 不匹配，此时，如果发现一个具有 share 属性的同名 EnvVar，我们就复制一个 EnvVar，并把它的 shell_id 设置成子 Shell 进程的 shellid，这样后续父子就独立开来了。NO
- 回归本质，直接在新建子 Shell 进程的时候完成**继承**。

具体地，修改 user/sh.c 的 main 函数:

```c
int main(int argc, char **argv) {
	int r;
	int echocmds = 0;
	interactive = iscons(0);
	
	syscall_shellid_alloc();	// 分配一个新的 shell
	syscall_extend_vars();		// 继承环境变量
```

### 环境变量相关函数

内核态:

```c
// include/env.h
int is_allow(int var_index);
int check_var(char *name, int create);
int shellid_alloc();
struct EnvVar* get_var(int index);
int get_all_values(char *values);
int get_value(char *name, char *valuebuf);
void extend_share_vars();

// kern/env.c
struct EnvVar envvars[MAX_VAR_NUM] __attribute__((alligned(PAGE_SIZE)));
extern struct Env *curenv;

int sid = 1;		// shellid
int varnum = 0;     // 记录环境变量的总个数

// shell_id 匹配
int is_allow(int var_index) {
    return envvars[var_index].shell_id == curenv -> shellid;
}

int check_var(char *name, int create) {
    for (int i = 0; i < varnum; i++) {
        if (envvars[i].valid && strcmp(envvars[i].name, name) == 0 && is_allow(i)) {
            return i;
        }
    }
    if (create) {
        int ans = varnum++;
        return ans;
    } else {
        return -1;
    }
}

void extend_share_vars() {
	struct EnvVar var;
	struct EnvVar *pnewvar;
	int bef_varsum = varnum;
	for (int i = 0; i < bef_varsum; i++) {
		var = envvars[i];
		if (var.valid && var.share) {
			pnewvar = &envvars[varnum];
			strcpy(pnewvar -> name, var.name);
			strcpy(pnewvar -> value, var.value);
			pnewvar -> shell_id = curenv -> shellid;
			pnewvar -> read_only = var.read_only;
			pnewvar -> valid = var.valid;
			pnewvar -> share = var.share;
			varnum++;
		}
	}
}

int shellid_alloc() {
    sid++;
    return sid;
}

struct EnvVar* get_var(int index) {
    return &envvars[index];
}

int get_all_values(char *values) {
	struct EnvVar var;
	char *vp = values;
    int cnt = 0;
    for (int i = 0; i < varnum; i++) {
        var = envvars[i];
        if (var.valid && is_allow(i)) {
            cnt++;
            strcpy(vp, var.name);
            vp += strlen(var.name);
            *vp++ = '=';
            strcpy(vp, var.value);
            vp += strlen(var.value);
            *vp++ = '\n';
        }
    }
    *vp = '\0';
    return cnt;
}

int get_value(char *name, char *valuebuf) {
    int index;
    if ((index = check_var(name, 0)) < 0) {
        return -ENV_VAR_ERR;
    }
    strcpy(valuebuf, envvars[index].value);
    return 0;
}
```

注意拷贝的时候，一定要用 struct EnvVar * 去指内存区域，如果只是 struct EnvVar newvar ，然后对这个 newvar 操作，并不会写回 envvars 数组。

### declare 和 unset

主要利用前面的工具，实现系统调用的接口。

```c
// include/syscall.h
enum {
    SYS_shellid_alloc,
	SYS_declare_shell_var,
	SYS_unset_shell_var,
	SYS_get_shell_var,
	SYS_extend_vars,
    MAX_SYSNO,
}

// kern/syscall_all.c
int sys_extend_vars() {
	extend_share_vars();
	return 0;
}

int sys_declare_shell_var(char *name, char *value, int share, int rdonly) {
	int index = check_var(name, 1);
	if (index < 0) {	
		return -ENV_VAR_ERR;
	}
	struct EnvVar *var = get_var(index);
	if (var -> read_only) {
		return -ENV_VAR_ERR;
	}
	strcpy(var->name, name);
	strcpy(var->value, value);
	var->shell_id = curenv->shellid;
	var->read_only = rdonly;
	var->valid = 1;
	var->share = share;
	return 0;
}

int sys_unset_shell_var(char *name) {
	int index = check_var(name, 0);
	if (index < 0) {
		return -ENV_VAR_ERR;	
	}
	struct EnvVar *pvar = get_var(index);
	if (pvar -> read_only) {
		return -ENV_VAR_ERR;
	}
	pvar -> valid = 0;
	return 0;
}

int sys_get_shell_var(char *name, char *value) {
	if (name == 0) {	// 要返回所有
        return get_all_values(value);
	} else {
		return get_value(name, value);
	}
}

int sys_shellid_alloc() {
	curenv -> shellid = shellid_alloc();
	return 0;
}

int sys_exofork(void) {
	/* 新增的部分 */
	e->shellid = curenv->shellid;	// fork 的时候要复制 shell_id
}

void *syscall_table[MAX_SYSNO] = {
    /* 新增的部分 */
	[SYS_shellid_alloc] = sys_shellid_alloc,
	[SYS_declare_shell_var] = sys_declare_shell_var,
	[SYS_unset_shell_var] = sys_unset_shell_var,
	[SYS_get_shell_var] = sys_get_shell_var,
	[SYS_extend_vars] = sys_extend_vars,
};
```

```c
// user/include/lib.h
int syscall_shellid_alloc();
int syscall_declare_shell_var(char *name, char *value, int share, int rdonly);
int syscall_unset_shell_var(char *name);
int syscall_get_shell_var(char *name, char *value);
int syscall_extend_vars();

// user/lib/syscall_lib.c
int syscall_shellid_alloc() {
	return msyscall(SYS_shellid_alloc);
}

int syscall_declare_shell_var(char *name, char *value, int share, int rdonly) {
	return msyscall(SYS_declare_shell_var, name, value, share, rdonly);
}

int syscall_unset_shell_var(char *name) {
	return msyscall(SYS_unset_shell_var, name);
}

int syscall_get_shell_var(char *name, char *value) {
	return msyscall(SYS_get_shell_var, name, value);
} 

int syscall_extend_vars() {
	return msyscall(SYS_extend_vars);
}
```

然后实现 declare.c 和 unset.c 即可。

### 环境变量的替换

环境变量可能这样 `$envvar/dir2` 使用，怎么识别并替换？因为环境变量的命名规则和 C 的变量命名规则一致，所以，我们只读取变量名部分 (字母/数字/下划线)，就得到环境变量 `$envvar`。

```c
#define SYMBOLS "<|>&;()#$"

int parsecmd(char **argv, int *rightpipe) {
	while (1) {
		/* 新增的部分 */
		switch (c) {
		case '$':
			if (gettoken(0, &t) != 'w') {
				debugf("syntax error: $ not followed by word\n");
				exit(1);
			}
			char *pbuf = t;
			char varname[128] = {0};
			char varbuf[1024];
			int i = 0;
			while (*pbuf >= 'a' && *pbuf <= 'z' || *pbuf >= 'A' && *pbuf <= 'Z' 
					|| *pbuf >= '0' && *pbuf <= '9' || *pbuf == '_') {
				varname[i++] = *pbuf++;
			}
            ...
```

## 光标

> 键入命令时，可以使用 Left 和 Right 移动光标位置，并可以在当前光标位置进行字符的增加与删除。

### 左右移动

在 ANSI 兼容终端中，按下方向键 “←” 时，终端会发送三个字符构成的转义序列 `\033[D` (即 ESC, `[`, `D`)，而不是一个单独的字符。我们要修改 readline 函数，连续获取到左右键代表的三个字符后，再对光标的位置进行修改。看学长代码发现可以通过 printf 显示光标的移动变化。比如，`\033[%dD` 是 ANSI 转义序列，表示 `ESC [ <n> D` ，作用是将光标左移 `n`。

```c
void readline(char *buf, u_int n) {
	char curIn[1024];
	int r, len = 0;		// len 表示键入的命令长度
	char temp = 0;
	for (int i = 0; len < n; ) {	// i 表示光标的位置
		if ((r = read(0, &temp, 1)) != 1) {		// 逐字符读取
			if (r < 0) {
				debugf("read error: %d\n", r);
			}
			exit(1);
		}
		switch (temp) {
			case '\033':
				read(0, &temp, 1);
				if (temp == '[') {
					read(0, &temp, 1);
					if (temp == 'D') {		// ⬅️
						if (i > 0) {
							i -= 1;
						} else {
							printf("\033[C");
						}
					} else if (temp == 'C') {	// ➡️
						if (i < len) {
							i += 1;
						} else {
							printf("\033[D");
						}
					}
                }
                break;
            default:
				buf[len + 1] = 0;
				for (int j = len; j >= i + 1; j--) {
					buf[j] = buf[j - 1];
				}
				buf[i++] = temp;
				if (interactive != 0) {
					printf("\033[%dD%s", i, buf);
					if ((r = len++ + 1 - i) != 0) {
						printf("\033[%dD", r);
					}
				} else {
					len++;
				}
			break;
        }     
```

### 历史指令

> 保存历史指令，可以通过 Up 和 Down 选择所保存的指令并执行。你需要将历史指令保存到根目录的 `.mos_history` 文件中（一条指令一行），为了评测的方便，我们设定 `$HISTFILESIZE=20`（bash 中默认为 500），即在 `.mos_history` 中至多保存最近的 20 条指令。还需要支持通过 `history` 命令输出 `.mos_history` 文件中的内容。

首先生成 .mos_history 文件。

```c
// user/sh.c
int hisCount;		// 历史指令的个数
int hisBuf[1024];	// 第 i 条指令所占字节数

int main(int argc, char **argv) {
	/* 新增 */
	if ((r = open("/.mos_history", O_CREAT)) < 0) {
		printf("err1\n");
		exit(1);
	}
```

当 Shell 检测到换行符时，便会判定指令输入的结束，我们就在这时开始写入历史命令。在 `readline` 函数的 `switch` 分支内，针对 `case '\r'` 与 `case '\n'` 需要做写入文件的操作:

```c
			case '\r':
			case '\n':
				buf[len] = '\0';
				int hisFd;
				if ((hisFd = open("/.mos_history", O_APPEND | O_WRONLY)) < 0) exit(1);
				if ((r = write(hisFd, buf, len)) != len) exit(1);			
				if ((r = write(hisFd, "\n", 1)) != 1) exit(1); 
				if ((r = close(hisFd)) < 0) exit(1);
				hisBuf[hisCount++] = len;
				curLine = hisCount;
				memset(curIn, '\0', sizeof(curIn));
				return;
```

当识别到 Up/Down 键，需要实时变动并显示 buf 的内容。

```c
// user/sh.c
int readPast(int target, char *code) {		// 读第 target 行的指令到 code
	int r, fd, spot = 0;
	char buff[1024];
	if ((fd = open("/.mos_history", O_RDONLY)) < 0) {
		return fd;
	}
    ...
}

void readline(char *buf, u_int n) {
    		/* 新增 */
			case '\033':
				read(0, &temp, 1);
				if (temp == '[') {
					read(0, &temp, 1);
					if (temp == 'A') {		// up
						printf("\033[B");
						if (curLine != 0) {
							buf[len] = '\0';
							if (curLine == hisCount) {
								strcpy(curIn, buf);
							}
							if (i != 0) { printf("\033[%dD", i); }
							for (int j = 0; j < len; j++) { printf(" "); }
							if (len != 0) { printf("\033[%dD", len); }

							if ((r = readPast(--curLine, buf)) < 0) { exit(1); }
							printf("%s", buf);
							i = strlen(buf);
							len = i;
						}
					}
				}
				break;
}
```

### 快捷键

需要在Shell中实现以下快捷键:

| 快捷键    | 行为                                                         |
| :-------- | :----------------------------------------------------------- |
| backspace | 删除光标左侧 1 个字符并将光标向左移动 1 列；若已在行首则无动作 |
| Ctrl-E    | 光标跳至最后                                                 |
| Ctrl-A    | 光标跳至最前                                                 |
| Ctrl-K    | 删除从当前光标处到最后的文本                                 |
| Ctrl-U    | 删除从最开始到光标前的文本                                   |
| Ctrl-W    | 向左删除最近一个 word：先越过空白(如果有)，再删除连续非空白字符 |

backspace 主要移动光标和删除字符，在 readline 中操作，其他快捷键实现类似:

```c
			case '\b':
			case 0x7f:
				if (i <= 0) {		// 光标已经位于最左端
                    break; 
                }
				for (int j = (--i); j <= len - 1; j++) {
					buf[j] = buf[j + 1];
				}
				buf[--len] = 0;
				printf("\033[%dD%s \033[%dD", (i + 1), buf, (len - i + 1));
				break;
```

## 条件指令

> 实现 Linux shell 中的 `&&` 与 `||`
>
> - 对于 `command1 && command2`，`command2` 被执行当且仅当 `command1` 返回 0；
> - 对于 `command1 || command2`，`command2` 被执行当且仅当 `command1`返回非 0 值

参考学长学姐的做法，将**进程的返回值**保存在进程控制块的 `env_exit_value` 。

```c
struct Env {
    int env_exit_value;
}
```

> 题目后续指令的执行都依据于前面指令的执行结果，因此是串行的，处理逻辑与 `;` 类似。需要使用 `wait` 等待前面的进程执行完，同时需要其返回子进程的执行结果，拿着这个结果决定本身指令是否该进行。所以 `wait` 是要去取子进程的 `exit_value` 的。所以又要实现进程与进程之间的通信，又要构建系统调用。

```c
// user/lib/wait.c
int wait(u_int envid) {
	const volatile struct Env *e;
	e = &envs[ENVX(envid)];
	while (e->env_id == envid && e->env_status != ENV_FREE) {
		syscall_yield();
	}
	int ans = syscall_wait(envid);		// 返回 envid 对应进程的执行结果
	return ans;
}
```

所有用户进程都在 user/lib/libos.c 的 `libmain` 里面的 `main` 进行调用。因此这个函数也要有返回值。

```c
void exit(int value) {
#if !defined(LAB) || LAB >= 5
	close_all();
#endif
	syscall_exit(value);
	syscall_env_destroy(0);
	user_panic("unreachable code");
}

const volatile struct Env *env;
extern int main(int, char **);

void libmain(int argc, char **argv) {
	env = &envs[ENVX(syscall_getenvid())];
	int value = main(argc, argv);
	exit(value);
}
```

然后完成系统调用。

```c
// include/syscall.h
enum {
    SYS_wait,
	SYS_exit,
	MAX_SYSNO,
};

// kern/syscall_all.c
int sys_wait(u_int envid) {
	// printk("I am here...\n");
	struct Env *e;
    // try(envid2env(envid, &e, 0));
	// 不能这样, envid2env 只能返回 ENV_RUNNABLE 的 env, 遇到 ENV_FREE 就返回错误
	// 手动实现
	e = &envs[ENVX(envid)];
	if (e -> env_status == ENV_FREE) {
		// printk("syswait: %d\n", e -> env_exit_value);
		return e -> env_exit_value;
	}
	return -1;
}

void sys_exit(int value) {
	curenv -> env_exit_value = value;
}

void *syscall_table[MAX_SYSNO] = {
    [SYS_wait] = sys_wait,
	[SYS_exit] = sys_exit,
};

// user/include/lib.h
void exit(int value) __attribute__((noreturn));
int syscall_wait(u_int envid);
void syscall_exit(int value);
int wait(u_int envid);

// user/lib/syscall_lib.c
int syscall_wait(u_int envid) {
	return msyscall(SYS_wait, envid);
}

void syscall_exit(int value) {
	msyscall(SYS_exit, value);
}
```

要对所有的 exit 调用进行修改，带上返回值。

```c
int parsecmd(char **argv, int *rightpipe) {
    int argc = 0;
	while (1) {
		char *t;
		int fd, r;
		int c = gettoken(0, &t);
		int child;

		if (c == 0) {
			next_run = 1;
			flag = 0;
			return argc;
		}
		
		if (!next_run) {
			if (c == 'a' && flag == 0) {
				next_run = 1;
			} else if (c == 'o' && flag == 1) {
				next_run = 1;
			}
			continue;
		}

		switch (c) {
		/* 新增 */
		case 'a':		// 表示 cmd1 && cmd2
			child = fork();
			if (child == 0) {
				return argc;
			} else if (child > 0) {
				flag = wait(child);		// 等待 child 进程执行完
				close(0);
				close(1);
				dup(opencons(), 1);
				dup(1, 0);
				if (flag == 0) {		// cmd1 执行结果为 0 (即正常)，继续执行 cmd2
					next_run = 1;
				} else {				// 否则不执行 cmd2
					next_run = 0;
				}
				return parsecmd(argv, rightpipe);
			}
			break;
}
```

## 参考资料

- [banana889 | OS-lab6-challenge](https://banana889.github.io/2022/06/27/OS-lab6-challenge/)

- [zhangyitong | BUAA-OS-challenge](https://github.com/zhangyitonggg/BUAA-OS-challenge/blob/main/readme.md)
- [MOS_Volca_Shell](https://github.com/volcaxiao/MOS_Volca_Shell)
- [cookedbear | BUAA-OS-2023-Lab6-Challenge](https://cookedbear.top/p/28193.html)
- [JulySakufo | BUAA_OS_2024](https://github.com/JulySakufo/BUAA_OS_2024/blob/challenge-shell/挑战性任务challenge-shell.md)
- [alkaid-zhong | OS Lab6 Challenge - Enhanced Shell](https://alkaid-zhong.github.io/posts/os/os-lab6-challenge/os-lab6-challenge/)
