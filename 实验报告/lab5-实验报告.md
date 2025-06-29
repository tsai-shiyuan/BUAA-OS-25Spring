# Lab5-File System

## 思考题

### 5.1

如果通过 kseg0 读写设备，那么对于设备的写入会缓存到 Cache 中。

请思考: 这么做会引发什么问题? 对于不同种类的设备 (如我们提到的串口设备和 IDE 磁盘) 的操作会有差异吗? 可以从缓存的性质和缓存更新的策略来考虑。

【答案】

对于写操作: kseg0的写操作采用写回 (write back) 策略，仅仅写入 cache，等到 cache 被替换或刷新才写入内存。而设备通常需要立即响应写入的控制指令或数据，不能延迟。

对于读操作: 外设更新后对内存也进行了更新，但是 cache 中的数据没有被更新，如果我们通过 kseg0 来访问外设，并不能得到最新的数据。

磁盘作为块设备，总是以块为单位进行读写，通常会使用缓冲区来临时存储这些数据块。当内存缓冲区数据被修改 (例如写一个磁盘块)，操作系统会将其标记为 dirty，这些 dirty 缓存页稍后会被写回磁盘，从而实现同步。因此这种错误对磁盘出现概率较小。但是串口设备等字符型设备就很容易发生写回顺序混乱和读写不一致的问题。

### 5.2

MOS 的一个磁盘块中最多能存储多少个文件控制块？一个目录下最多能有多少个文件？我们的文件系统支持的单个文件最大为多大？

【答案】

```c
#define BLOCK_SIZE PAGE_SIZE	// 4096
#define FILE_STRUCT_SIZE 256
#define FILE2BLK (BLOCK_SIZE / sizeof(struct File))
```

可以算出 `FILE2BLK` 为 16，即一个磁盘块中最多有 `16` 个文件控制块

```c
struct File {
	...
	uint32_t f_direct[NDIRECT];
	uint32_t f_indirect;
	...
} __attribute__((aligned(4), packed));
```

![截屏2025-05-15 19.20.31](./images_report/%E6%88%AA%E5%B1%8F2025-05-15%2019.20.31.png)

MOS使用二级索引，每个FCB共可以指向1024个磁盘块。对于目录磁盘块，其存放子目录或子文件的文件控制块，所以一个目录最多存放 1024*16=16384 个子文件

单个文件最多索引到 1024 个磁盘块，所以文件最大为 1024*4KB=4MB

### 5.3

在满足磁盘块缓存的设计的前提下，我们实验使用的内核支持的最大磁盘大小是多少？

【答案】

```c
#define DISKMAP 0x10000000
/* Maximum disk size we can handle (1GB) */
#define DISKMAX 0x40000000
```

把文件系统服务进程的 [DISKMAP, DISKMAP + DISKMAX) 这段地址空间作为缓冲区，访问磁盘的第 `n` 块就等价于访问 `DISKMAP+(n*BLOCK_SIZE)` 这页，操作系统该页是否在物理内存中，如果不在，就从磁盘加载对应的块进来并建立映射，之后再访问这个磁盘块，就不用再读磁盘了，实现了缓冲！

从注释可以看出，最大磁盘大小为 `DISKMAX` ，1GB

### 5.4

在本实验中，fs/serv.h, user/include/fs.h 等文件中出现了许多宏定义，试列举你认为较为重要的宏定义，同时进行解释，并描述其主要应用之处

【答案】

文件控制块和超级块的定义很重要

```c
struct File {
	char f_name[MAXNAMELEN]; // filename
	uint32_t f_size;	 // file size in bytes
	uint32_t f_type;	 // file type
	uint32_t f_direct[NDIRECT];
	uint32_t f_indirect;

	struct File *f_dir; // the pointer to the dir where this file is in, valid only in memory.
	char f_pad[FILE_STRUCT_SIZE - MAXNAMELEN - (3 + NDIRECT) * 4 - sizeof(void *)];
} __attribute__((aligned(4), packed));

#define FILE2BLK (BLOCK_SIZE / sizeof(struct File))

// File types
#define FTYPE_REG 0 // Regular file
#define FTYPE_DIR 1 // Directory

// File system super-block (both in-memory and on-disk)

#define FS_MAGIC 0x68286097 // Everyone's favorite OS class

struct Super {
	uint32_t s_magic;   // Magic number: FS_MAGIC
	uint32_t s_nblocks; // Total number of blocks on disk
	struct File s_root; // Root directory node
};
```

### 5.5

`fork` 前后的父子进程是否会共享文件描述符和定位指针呢？请编写一个程序进行验证

【答案】

```c
#include "../../fs/serv.h"
#include <lib.h>

int main() {
	char buffer[256];
	int fd_num = open("/newmotd", O_RDONLY);
	int bytes;
	if (fork() == 0) {
		bytes = read(fd_num, buffer, 8);
		debugf("child buffer: %s\n", buffer);
	} else {
		bytes = read(fd_num, buffer, 8);
		debugf("parent buffer: %s\n", buffer);
	}
}
```

```bash
$ make test lab=5_off && make run
FS is running
superblock is good
read_bitmap is good
parent buffer: This is	#
child buffer: the NEW 	# 
[00000800] destroying 00000800
[00000800] free env 00000800
i am killed ... 
[00001802] destroying 00001802
[00001802] free env 00001802
i am killed ... 
panic at sched.c:46 (schedule): schedule: no runnable envs
ra:    80026038  sp:  803ffe80  Status: 00008000
Cause: 00000420  EPC: 00402dd0  BadVA:  7fd7f004
curenv:    NULL
cur_pgdir: 83fce000
```

可以看出子进程和父进程的文件描述符相同。即父子进程共享文件描述符和定位指针

### 5.6

请解释 File, Fd, Filefd 结构体及其各个域的作用。比如各个结构体会在哪些过程中被使用，是否对应磁盘上的物理实体还是单纯的内存数据等。可大致勾勒出文件系统数据结构与物理实体的对应关系与设计框架。

【答案】

struct File 在前文出现过了😏

```c
// file descriptor
struct Fd {
	u_int fd_dev_id;	// 设备id
	u_int fd_offset;	// 文件定位指针，标记当前读写的位置
	u_int fd_omode;		// 用户对文件的权限
};

// file descriptor + file
struct Filefd {
	struct Fd f_fd;		// 文件描述符
	u_int f_fileid;		// 打开文件的编号
	struct File f_file;	// 文件的文件控制块FCB
};
```

### 5.7

图中有多种不同形式的箭头，请解释这些不同箭头的差别，并思考我们的操作系统是如何实现对应类型的进程间通信的。

![截屏2025-05-15 20.32.37](./images_report/%E6%88%AA%E5%B1%8F2025-05-15%2020.32.37.png)

【答案】

> 同步消息，用黑三角箭头搭配黑实线表示。同步的意义: 消息的发送者把进程控制传递给消息的接收者，然后暂停活动，等待消息接收者的回应消息。 
>
> 返回消息，用开三角箭头搭配黑色虚线表示。返回消息和同步消息结合使用，因为异步消息不进行等待，所以不需要知道返回值。
>
> 当前 serve 执行到 ipc_recv ，通过 syscall_ipc_recv 进入使进程变成接收态 (NOT_RUNNABLE)，直到它收到消息，变成 RUNNABLE，才能够被调度，从 ipc_recv 的 syscall_ipc_recv 的后面的语句开始执行，并回到 serve，所以这个服务中的文件系统进程不会死循环，空闲时会让出 CPU 让其余进程执行。
>
> 从系统 IPC 的角度来看， ipc_recv 应当是同步消息，需要等待返回值，而 ipc_send 应当是异步消息，发送后无需关心返回值可以去做自己的事情。

具体地，`ipc_send` 函数向 whom 进程发送值 val 并把当前进程的 srcva 对应的页映射到 whom 进程的 dstva 中， `ipc_recv` 函数接收消息，映射到自己的 dstva 中

```c
void ipc_send(u_int whom, u_int val, const void *srcva, u_int perm) {
	int r;
	while ((r = syscall_ipc_try_send(whom, val, srcva, perm)) == -E_IPC_NOT_RECV) {
		syscall_yield();
	}
}

u_int ipc_recv(u_int *whom, void *dstva, u_int *perm) {
	int r = syscall_ipc_recv(dstva);
	...
	if (whom) *whom = env->env_ipc_from;
	if (perm) *perm = env->env_ipc_perm;
	return env->env_ipc_value;
}
```

以用户进程的 open 为例

```c
int open(const char *path, int mode) {
    ...
    try(fd_alloc(&fd));
	try(fsipc_open(path, mode, fd));
    ...
}

int fsipc_open(const char *path, u_int omode, struct Fd *fd) {
    ...
	return fsipc(FSREQ_OPEN, req, fd, &perm);   
}

static int fsipc(u_int type, void *fsreq, void *dstva, u_int *perm) {
	u_int whom;
	ipc_send(envs[1].env_id, type, fsreq, PTE_D);
	return ipc_recv(&whom, dstva, perm);
}
```

用户进程向文件系统服务进程发送打开文件请求，接着等待接收具体的文件内容消息，阻塞直到消息传来

同时，文件系统服务进程执行 `serve` 函数不断地尝试接收请求

```c
void serve(void) {
	u_int req, whom, perm;
	void (*func)(u_int, u_int);
	for (;;) {
		perm = 0;
		req = ipc_recv(&whom, (void *)REQVA, &perm);
        ...
            
		// Select the serve function and call it.
		func = serve_table[req];
		func(whom, REQVA);
		panic_on(syscall_mem_unmap(0, (void *)REQVA));
	}
}
```

如果收到了请求，则调用相应的函数来处理，最后回复用户进程的文件请求

```c
void serve_open(u_int envid, struct Fsreq_open *rq) {
	struct File *f;
	struct Filefd *ff;
	int r;
	struct Open *o;

	if ((r = open_alloc(&o)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	if ((rq->req_omode & O_CREAT) && (r = file_create(rq->req_path, &f)) < 0 &&
	    r != -E_FILE_EXISTS) {
		ipc_send(envid, r, 0, 0);
		return;
	}

	if ((r = file_open(rq->req_path, &f)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}
    ...

	// Fill out the Filefd structure
	ff = (struct Filefd *)o->o_ff;
	ff->f_file = *f;
	ff->f_fileid = o->o_fileid;
	o->o_mode = rq->req_omode;
	ff->f_fd.fd_omode = o->o_mode;
	ff->f_fd.fd_dev_id = devfile.dev_id;
	ipc_send(envid, 0, o->o_ff, PTE_D | PTE_LIBRARY);	//////
}
```

## 实验难点图示

### 文件系统总览

![截屏2025-05-11 18.37.30](./images_report/%E6%88%AA%E5%B1%8F2025-05-11%2018.37.30.png)

```bash
      Regular env           FS env
   +---------------+   +---------------+
   |      read     |   |   file_read   |
   |   (lib/fd.c)  |   |   (fs/fs.c)   |
...|.......|.......|...|.......^.......|...............
   |       v       |   |       |       |    IPC 机制
   |  devfile_read |   |  serve_read   |
   |  (lib/file.c) |   |  (fs/serv.c)  |
   |       |       |   |       ^       |
   |       v       |   |       |       |
   |     fsipc     |   |     serve     |
   |  (lib/file.c) |   |  (fs/serv.c)  |
   |       |       |   |       ^       |
   |       v       |   |       |       |
   |   ipc_send    |   |   ipc_recv    |
   |       |       |   |       ^       |
   +-------|-------+   +-------|-------+
           |                   |
           +-------------------+
```

### IDE磁盘驱动

IDE (Integrated Drive Electronics) 是过去主流的硬盘接口

```c
 *	* ---------------------------------*
 *	|   device   | start addr | length |
 *	* -----------+------------+--------*
 *	|  console   | 0x180003f8 | 0x20   |
 *	|  IDE disk  | 0x180001f0 | 0x8    |
 *	* ---------------------------------*
```

MOS微内核采用内存映射I/O (MMIO) ，把外设的寄存器，通常包含控制、状态、数据寄存器，映射到物理地址空间，特别地，映射到 kseg1 段。用户空间的磁盘驱动程序通过系统调用，进而在内核态修改 kseg1 的值，实现对设备寄存器的控制。具体地，需要读写 kseg1 段的这部分空间。

![截屏2025-05-27 14.09.46](./images_report/%E6%88%AA%E5%B1%8F2025-05-27%2014.09.46.png)

### 文件描述符的存储位置

如图:

![newfd.drawio](./images_report/newfd.drawio.svg)

![fd.drawio](./images_report/fd.drawio.svg)

## 实验体会

Lab5 主要的思想是利用进程间通信的 IPC 机制，启用一个用户态的文件系统服务进程，不断接收用户进程的文件请求，“代替”用户进程执行部分对文件的操作，再映射回用户进程的地址空间。这种设计非常精妙！

另外，Lab5 的函数超级多！函数调用的层次相当深，要一段一段地看，认真考虑函数们之间的关系。我也没完全理清楚，有空一定再认真看看！～

## Reference

[flyinglandlord | lab-5 实验报告](https://flyinglandlord.github.io/2022/06/15/BUAA-OS-2022/lab5/doc-lab5/)