# Lab6-Shell

## 思考题

### 6.1

示例代码中，父进程操作管道的写端，子进程操作管道的读端。如果现在想让父进程作为“读者”，代码应当如何修改？

【答案】

把父进程的写端关闭，把子进程的读端关闭即可

```c
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

int fildes[2];
char buf[100];
int status;

int main() {
	status = pipe(fildes);

	if (status == -1) {
        printf("error\n");
    }
    
    switch (fork()) {
        case -1:
            break;
        case 0:
            close(fildes[0]);
            write(fildes[1], "Hello world\n", 12);
            close(fildes[1]); 
            exit(EXIT_SUCCESS);
        default:
            close(fildes[1]);
            read(fildes[0], buf, 100);
            printf("father-process read: %s", buf);
            close(fildes[0]);            
            exit(EXIT_SUCCESS);
    }
}
```

### 6.2

不同步修改 pp_ref 而导致的进程竞争问题在 user/lib/fd.c 中的 dup 函数中也存在。请结合代码分析 dup 函数中为什么会出现预想之外的情况。

【答案】

原来的 dup

```c
int dup(int oldfdnum, int newfdnum) {
    // ...
	if ((r = syscall_mem_map(0, oldfd, 0, newfd, 
                             vpt[VPN(oldfd)] & (PTE_D | PTE_LIBRARY))) < 0) {
		goto err;
	}
	
	if (vpd[PDX(ova)]) {
		for (i = 0; i < PDMAP; i += PTMAP) {
			pte = vpt[VPN(ova + i)];

			if (pte & PTE_V) {
				// should be no error here -- pd is already allocated
				if ((r = syscall_mem_map(0, (void *)(ova + i), 0, (void *)(nva + i),
							 pte & (PTE_D | PTE_LIBRARY))) < 0) {
					goto err;
				}
			}
		}
	}
```

dup 先对文件描述符进行映射，后对文件内容进行映射。如果在这两个系统调用中发生进程的中断，根据文件描述符的 pp_ref 将判断映射已经完成，而实际上文件内容并未被映射。

### 6.3

为什么系统调用一定是原子操作呢？如果你觉得不是所有的系统调用都是原子操作，请给出反例。

【答案】MOS系统调用会陷入内核态，并且屏蔽中断，整个系统调用过程不会被时钟中断打断，因此可以认为系统调用是原子的。

```assembly
exc_gen_entry:
	SAVE_ALL
	mfc0    t0, CP0_STATUS
	and     t0, t0, ~(STATUS_UM | STATUS_EXL | STATUS_IE)	# IE 屏蔽了中断
	mtc0    t0, CP0_STATUS
	mfc0 t0, CP0_CAUSE
	andi t0, 0x7c
	lw t0, exception_handlers(t0)
	jr t0
```

### 6.4

- 控制 pipe_close 中 fd 和 pipe unmap 的顺序，是否可以解决进程竞争问题？
- 我们只分析了 close 时的情形，在 fd.c 中有一个 dup 函数，用于复制文件描述符。 试想，如果要复制的文件描述符指向一个管道，那么是否会出现与 close 类似的问题？

【答案】

- 可以，因为我们判断 close 出错的原因是 $\mathrm{pageref(fd) = pageref(pipe)}$ 并不等价于读/写端关闭，现在修改为先 unmap fd 再 unmap pipe，就能始终保证 $\mathrm{pageref(fd) \leqslant pageref(pipe)}$ ，等号只在真正关闭了 pipe，也就是执行完 pipe_close 函数后才成立。可以解决题面的竞争问题
- 若先增加 fd 的 pp_ref，后增加文件内容（也就是 pipe）的 pp_ref，就不能保证始终有 pageref(pipe) > pageref(fd)。在两个 map 的间隙，就会存在 pageref(pipe) == pageref(fd) 的时刻，导致误判。修改map的顺序，先对 pipe 进行 map，就可以解决这一问题

### 6.5

- 回顾 Lab1 与 Lab3，思考如何读取并加载 ELF 文件。
- 在 Lab1 中我们介绍了 data text bss 段及它们的含义，data 段存放初始化过的全局变量，bss 段存放未初始化的全局变量。回顾 Lab3 并思考: elf_load_seg() 和 load_icode_mapper() 函数是如何确保加载 ELF 文件时，bss 段数据被正确加载进虚拟内存空间。bss 段在 ELF 中并不占空间，但 ELF 加载进内存后，bss 段的数据占据了空间，并且初始值都是 0。请回顾 elf_load_seg() 和 load_icode_mapper() 的实现，思考这一点是如何实现的？

【答案】

* 在 `user/lib/files.c` 文件中，`open` 函数调用了同目录下的 `fsipc_open` 函数。`fsipc_open` 函数通过调用`fsipc`函数与服务进程进行进程间通信，并接收返回的消息。文件系统服务进程中的`serve_open`函数随后调用`file_open`函数来打开文件。最终，这个过程通过`fsipc`实现了用户进程与文件描述符的共享。
* 调用`load_icode`，将elf文件作为二进制文件打开并保存为char数组
* `elf_load_seg`函数中的`map_page`（即`load_icode_mapper`）完成了对虚拟空间的映射和初始化为0

### 6.6

通过阅读代码空白段的注释我们知道，将标准输入或输出定向到文件，需要我们将其 dup 到 0 或 1 号文件描述符 fd。在哪步，0 和 1 被“安排”为标准输入和标准输出？请分析代码执行流程，给出答案。

【答案】

Shell 进程初始化的时候，调用了 `opencons` 打开一个终端文件，因为调用 `opencons` 时系统内必定没有其他文件被打开，所以该函数申请得到的文件描述符必定为 0。之后代码中又调用 `dup` 函数申请了一个新的文件描述符 1。该文件描述符是 0 的复制。也就是说，对这两个文件描述符进行读写等文件操作，都是对终端文件的操作。

```c
// user/init.c
int main(int argc, char **argv) {
	int i, r, x, want;
	// omit...
    // stdin should be 0, because no file descriptors are open yet
	if ((r = opencons()) != 0) {
		user_panic("opencons: %d", r);
	}
	// stdout
	if ((r = dup(0, 1)) < 0) {
		user_panic("dup: %d", r);
	}
```

### 6.7

在 shell 中执行的命令分为内置命令和外部命令。在执行内置命令时 shell 不需要 fork 一个子 shell，如 Linux 系统中的 cd 命令。在执行外部命令时 shell 需要 fork 一个子 shell，然后子 shell 去执行这条命令。

据此判断，在 MOS 中我们用到的 shell 命令是内置命令还是外部命令？请思考为什么 Linux 的 cd 命令是内部命令而不是外部命令？

【答案】

MOS 的 Shell 是外部命令，因为在 spawn 中，每次运行都要使用 `child = syscall_exofork()`

cd 命令的作用是改变当前进程的工作目录。如果像普通外部命令一样，运行时创建新进程，那么它对文件系统的操作就不会影响父进程的环境，当前目录不会改变，这显然不符合用户预期。

### 6.8

在你的 shell 中输入命令 `ls.b | cat.b > motd`

你可以在你的 shell 中观察到几次 spawn？分别对应哪个进程？

你可以在你的 shell 中观察到几次进程销毁？分别对应哪个进程？

【答案】

spawn 函数加上输出

```c
int spawn(char *prog, char **argv) {
	debugf("spawn: %s\n", prog);
```

```bash
$ ls.b | cat.b > motd
[00002803] pipecreate 
spawn: ls.b
spawn: cat.b
[00003805] destroying 00003805
[00003805] free env 00003805
i am killed ... 
[00004006] destroying 00004006
[00004006] free env 00004006
i am killed ... 
[00003004] destroying 00003004
[00003004] free env 00003004
i am killed ... 
[00002803] destroying 00002803
[00002803] free env 00002803
i am killed ... 
```

可以发现进行了 2 次 spawn，分别对应 ls.b 和 cat.b。进行了 4 次进程销毁，分别对应 ls.b，cat.b 以及他们用于管道的两个子进程。

## 实验难点

### 管道机制

在 Unix 中，管道由 `int pipe(int fd[2])` 函数创建，成功创建管道返回0，`fd[0]` 对应读端，`fd[1]` 对应写端。父进程调用 `pipe` 函数时，会打开两个新文件描述符，只读端和只写端，两个文件描述符都映射到同一片内存区域。在管道的读写过程中，要保证父子进程通过管道访问的内存相同。

### Shell 的工作原理

shell 进程不断地读取命令，然后 fork 出子进程来执行命令。

```bash
main
 └─> readline → 获取命令
 └─> fork
     ├─ 子进程 → runcmd(buf) → 解释命令并执行（可能处理重定向、管道等）
     └─ 父进程 → wait 子进程
```

```bash
runcmd (char *s)
├─ gettoken (char *s, char **p1)
	└─ _gettoken (char *s, char **p1, char **p2)
├─ parsecmd (char **argv, int *rightpipe)
├─ spawn (argv[0], argv) - 左侧命令执行
├─ wait(child)
├─ wait(rightpipe) - 如果有管道
└─ exit()
```

## 实验体会

Lab 6 是建立在之前 Lab 的基础之上的，管道机制需要好好理解文件描述符 fd 和 pipe 的映射关系，Shell 进程需要理解 fork 还有非常复杂的 runcmd, parsecmd 函数调用。至此，MOS 终于可以和用户交互了，看到命令正确的执行还是非常开心的。

这学期的 OS 让我见识到了一个大型 C 工程是什么样的，错综复杂的函数，密密麻麻的文件……我也渐渐学会沉下心读代码，画各种图。学到后面 Lab 会对之前的 Lab 有更深的理解，坚持下去一定有收获！

## Reference

- [Wokron｜BUAA-OS 实验笔记之 Lab6](https://wokron.github.io/posts/buaa-os-lab6/)
- [Demiurge｜北航OS Lab6 管道与SHELL](https://demiurge-zby.github.io/p/北航os-lab6-管道与shell/)
