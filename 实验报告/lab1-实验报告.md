# Lab1-Bootstrap

## 思考题

### 1.1

**Where is the defination of `printf` ?** 

```c
#include <stdio.h>
int main() {
	printf("Hello World!\n");
	return 0;
}
```

只进行预处理，得到:

```c
extern struct _IO_FILE *stdin; 
extern struct _IO_FILE *stdout; 
extern struct _IO_FILE *stderr;
extern int printf (const char *__restrict __format, ...);

int main() 
{
	printf("Hello World!\n");
	return 0; 
}
```

只编译不链接时，进行反汇编

```bash
$ objdump -DS 要反汇编的目标文件名 > 导出文本文件名
```

x86-64下反汇编:

<img src="./images_report/%E6%88%AA%E5%B1%8F2025-03-15%2021.51.26.png" alt="截屏2025-03-15 21.51.26" style="zoom:50%;" />

`callq` 是函数调用的指令，e8后本应跟 `printf` 的地址，现却填 `0`

再经过链接生成可执行文件，再反汇编，发现已经填好了地址

<img src="./images_report/%E6%88%AA%E5%B1%8F2025-03-15%2021.57.43.png" alt="截屏2025-03-15 21.57.43" style="zoom:50%;" />

$\Rightarrow$ `printf` 是在链接的时候被插入可执行文件的。`printf` 早就被编译成了二进制！

<img src="./images_report/%E6%88%AA%E5%B1%8F2025-03-15%2022.01.12.png" alt="截屏2025-03-15 22.01.12" style="zoom: 33%;" />

> 1.1 分别使用实验环境中的原生 x86 工具链(gcc、ld、readelf、objdump 等)和 MIPS 交叉编译工具链(带有 mips-linux-gnu- 前缀，如 mips-linux-gnu-gcc、mips-linux-gnu-ld)，重复其中的编译和解析过程，观察相应的结果，并解释其中向 objdump 传入的参数的含义 (`objdump` on Linux or `otool` on a Mac to disassemble)

- `objdump -d <file(s)>`: 将代码段反汇编
- `objdump -S <file(s)>`: 将反汇编代码与源代码交替显示，编译时需要使用`-g`参数，即需要调试信息
- `objdump -C <file(s)>`: 将C++符号名逆向解析
- `objdump -l <file(s)>`: 反汇编代码中插入文件名和行号
- `objdump -j section <file(s)>`: 仅反汇编指定的section

```bash
$ mips-linux-gnu-gcc -c main.c -o main.o
$ mips-linux-gnu-objdump -DS main.o > main.s
```

![截屏2025-03-18 20.22.45](./images_report/%E6%88%AA%E5%B1%8F2025-03-18%2020.22.45.png)

### 1.2

使用我们编写的 readelf 程序，解析之前在 target 目录下生成的内核 ELF 文件:

![截屏2025-03-16 10.55.20](./images_report/%E6%88%AA%E5%B1%8F2025-03-16%2010.55.20.png)

我们编写的 readelf 程序是不能解析 readelf 文件本身的，而系统工具 readelf 则可以解析?

readelf的Makefile:

```makefile
readelf: main.o readelf.o
        $(CC) $^ -o $@	# $^: 所有的依赖文件 $@: 目标文件
hello: hello.c
        $(CC) $^ -o $@ -m32 -static -g
```

自己写的readelf.c的目标是解析32位程序，hello的 `-m32` 参数强制使用32位进行编译，而readelf默认使用64位编译，无法被解析

### 1.3

MIPS 体系结构上电时，启动入口地址为 0xBFC00000 (某个确定的地址)，但实验操作系统的内核入口并没有放在上电启动地址，而是按照内存布局图放置，为什么这样放置内核还能保证内核入口被正确跳转到?

My Ans: 启动分为两个阶段。首先，处理器上电复位后，从0xBFC00000处开始执行ROM中的代码 (即bootloader)，然后，bootloader负责加载内核到特定的内存区域，并跳转到内核入口，将控制权交给内核。所以，内核不放在启动入口也能被正确跳转到

## 难点分析

### MIPS内存布局

编译出的mos内核程序需要被放到合适的位置，最终决定将OS放到kseg0中 (运行在kseg1中的bootloader在载入内核前会对kseg0所需要的cache进行初始化)

> kseg0 vs kseg1
>
> 他们映射到相同的物理地址范围，存的内容是相同的，但二者访问方式不同。kseg0使用cache，能加速CPU对数据的访问，kseg1不加cache，直接与外设交互，保证I/O的数据一致。(外设不能用cache，因为用cache可能会读到旧值)

<img src="./images_report/%E6%88%AA%E5%B1%8F2025-03-16%2014.39.32.png" alt="截屏2025-03-16 14.39.32" style="zoom:33%;" />

### 地址空间

![截屏2025-03-27 11.34.12](./images_report/%E6%88%AA%E5%B1%8F2025-03-27%2011.34.12.png)

### readelf

elf.h:

```c
/* Type for section indices, which are 16-bit quantities.  */
typedef uint16_t Elf32_Section;

/* Type of symbol indices.  */
typedef uint32_t Elf32_Symndx;
```

- `typedef uint16_t Elf32_Half;` : `uint16_t` 定义在stdint.h中，表示无符号16位整数

`size_t` : typedef unsigned int size_t;  (在 32 位系统上)

`EI_` 是e_ident相关

`PT_` 是p_type相关

```c
typedef struct {
	unsigned char e_ident[EI_NIDENT]; /* Magic number and other info */ /*验证elf的有效性*/
    Elf32_Addr e_entry;		  /* Entry point virtual address */
    Elf32_Off e_phoff;		  /* Program header table file offset */
	Elf32_Off e_shoff;		  /* Section header table file offset */
} Elf32_Ehdr;

/* Section segment header.  */
typedef struct {
    Elf32_Addr sh_addr;	 /* Section addr */
	Elf32_Off sh_offset;	 /* Section offset */
} Elf32_Shdr;

/* Program segment header.  */
typedef struct {
    Elf32_Addr p_vaddr;  /* Segment virtual address */
	Elf32_Addr p_paddr;  /* Segment physical address */
} Elf32_Phdr;
```

> 按结构体大小偏移，所以
>
> ```c
> Elf32_Shdr *shdr = (Elf32_Shdr *)sh_table + i;
> ```
>
> 也是对的✅

## 实验体会

做题倒挺快，但是我比较纠结自身也作为程序的OS到底是怎么运行的 (其实我也说不清楚我到底纠结什么，像"bootstrap"一样)，然后花了很多时间理解代码的整体架构，最后是读了wokron学长的博客，受到很大启发！

虽然很难懂，但是慢慢读总会有收获滴～

## Reference

- [wokron｜BUAA-OS 实验笔记之 Lab1](https://wokron.github.io/posts/buaa-os-lab1/)
