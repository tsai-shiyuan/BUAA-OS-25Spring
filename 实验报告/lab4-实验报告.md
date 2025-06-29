# Lab4-Syscall

## 思考题

### 4.1

- 内核在保存现场的时候是如何避免破坏通用寄存器的?
- 系统陷入内核调用后可以直接从当时的 \$a0-\$a3 参数寄存器中得到用户调用msyscall 留下的信息吗?
- 我们是怎么做到让 sys 开头的函数“认为”我们提供了和用户调用 msyscall 时同样的参数的?
- 内核处理系统调用的过程对 Trapframe 做了哪些更改? 这种修改对应的用户态的变化是什么?

【答案】

1. 内核使用 `SAVE_ALL` 保存现场，在 `SAVE_ALL` 中，使用 `k0` 获取 CP0 寄存器的值，再将其值保存在栈上。除 `k0` 之外所有的通用寄存器都在修改之前被保存了，`k0` 是约定好保留供操作系统使用的寄存器，用户代码不会使用，因此修改也没关系

2. 可以，因为没有对 `a0` - `a3` 重新赋值

3. 在 `do_syscall` 函数中，我们通过把参数存入 `a0` - `a3` 寄存器或者压入栈中，从而将参数加载到 sys 函数认为的位置

4. `do_syscall` 函数执行了

   ```c
   void do_syscall(struct Trapframe *tf) {
       ...
       tf->cp0_epc += 4;
       ...
       tf->regs[2] = func(arg1, arg2, arg3, arg4, arg5);
   }
   ```

   修改了 tf 的 `cp0_epc` 和 `v0` ，`cp0_epc` 原本保存当前的指令，修改后保证了系统调用结束后，从 syscall 的下一条开始执行

### 4.2

思考 envid2env 函数: 为什么 envid2env 中需要判断 `e->env_id != envid` 的情况? 如果没有这步判断会发生什么情况?

```c
int envid2env(u_int envid, struct Env **penv, int checkperm) {
    struct Env *e;
    if (envid == 0) {
        *penv = curenv;
        return 0;
    }
	e = envs + ENVX(envid);
    if (e->env_status == ENV_FREE || e->env_id != envid) {
		return -E_BAD_ENV;
	} 
```

【答案】

```c
u_int mkenvid(struct Env *e) {
	static u_int i = 0;
	return ((++i) << (1 + LOG2NENV)) | (e - envs);
}
```

```c
#define LOG2NENV 10
#define NENV (1 << LOG2NENV)
#define ENVX(envid) ((envid) & (NENV - 1))
```

创造 envid 的时候，**低10位**用的是进程控制块相对 envs 数组的偏移，高位用的累加出来的数。

通过 envid 还原进程控制块的时候，`ENVX(envid)` 只取了 envid 的**低10位**，并未对高位进行检验。通过 `e->envid == envid` ，来确保此 envid 确实对应进程控制块 `e` 。

如果不检验高位会怎样？考虑这样的情况，某一进程完成运行，资源被回收，这时其对应的进程控制块会插入回 `env_free_list` 中。当重新获取该进程控制块，并赋予其新的 `env_id` ，我们发现: 新老 `env_id` 的**低10位都是相同的**，只有高位通过不断累加而不同。如果我们用旧的 envid，能得到同样的进程控制块，但显然不是我们想要的，旧的 envid 应该被判为无效的。

### 4.3

请回顾 kern/env.c 中 `mkenvid()` 函数的实现，该函数不会返回 0，请结合系统调用和 IPC 部分的实现与 `envid2env()` 函数的行为进行解释

【答案】

`mkenvid()` 函数不会返回0，即不会有 env_id 为 0 的进程。再看 `envid2env` 函数，如果 envid 为 0，函数会找到 `curenv`。所以，`mkenvid` 不返回 0 才能实现 envid2env 的获取当前进程的功能。

### 4.4

关于 fork 函数的两个返回值，下面说法正确的是?

【答案】

`fork` 只在父进程中被调用一次，在两个进程中各产生一个返回值

### 4.5

我们并不应该对所有的用户空间页都使用 `duppage` 进行映射。那么究竟哪些用户空间页应该映射，哪些不应该呢？请结合 kern/env.c 中 `env_init` 函数进行的页面映射、include/mmu.h 里的内存布局图以及本章的后续描述进行思考。

【答案】

首先，只考虑 `USTACKTOP` 之下的地址空间，因为其上的空间总是会被共享。其次，需要判断页目录项和页表项是否有效，如果无效的话，也不需要使用 `duppage` 进行映射。

对应着 fork 函数中的这段代码:

```c
int fork(void) {
    ...
	for (i = 0; i < VPN(USTACKTOP); i++) {
		if ((vpd[i >> 10] & PTE_V) && (vpt[i] & PTE_V)) {	// PDE有效且PTE有效
			duppage(child, i);
		}
	}
    ...
}
```

### 4.6*

在遍历地址空间存取页表项时你需要使用到 vpd 和 vpt 这两个指针，请参考 user/include/lib.h 中的相关定义，思考并回答这几个问题:

- vpt 和 vpd 的作用是什么? 怎样使用它们?
- 从实现的角度谈一下为什么进程能够通过这种方式来存取自身的页表? 它们是如何体现自映射设计的?
- 进程能够通过这种方式来修改自己的页表项吗? 

【答案】

- vpt 和 vpd 的定义:

  ```c
  #define vpt ((const volatile Pte *)UVPT)
  #define vpd ((const volatile Pde *)(UVPT + (PDX(UVPT) << PGSHIFT)))
  ```

- vpt 的使用示例:

  ```c
  	perm = vpt[VPN(va)] & 0xfff;
  ```

  从表现上来看，`vpt` 是一个 `Pte` 类型的数组，其中按虚拟地址的顺序存储了所有的页表项。从本质上看，`vpt` 是用户空间中的地址，并且正是页表自映射时设置的基地址。通过 vpt 就能取得所有页表项。使用时 `vpt[VPN]`
  
- vpd 的使用示例:

  ```c
  if ((vpd[i >> 10] & PTE_V) && (vpt[i] & PTE_V)) {	// PDE有效且PTE有效
      duppage(child, i);
  }
  ```
  
  vpd 是页目录项数组，地址的高10位作为页目录的索引，就可以取到页目录项。使用时 `vpd[PDX]`
  
- 因为内核为每个进程的地址空间设置了自映射机制。vpd 的定义是 `((const volatile Pde *)(UVPT + (PDX(UVPT) << PGSHIFT)))` ，是自映射公式。

- 不能，因为页表项没有 `PTE_W` 标记，用户进程只有通过系统调用陷入内核才能修改页表项

### 4.7

在 `do_tlb_mod` 函数中，你可能注意到了一个向异常处理栈复制 Trapframe 运行现场的过程，请思考并回答这几个问题:

- 这里实现了一个支持类似于“异常重入”的机制，在什么时候会出现这种“异常重入”?
- 内核为什么需要将异常的现场 Trapframe 复制到用户空间?

【答案】

- 异常重入 (Exception Re-entrancy) 指的是在用户态异常处理函数 (如 cow_entry) 执行过程中再次发生了异常的情况。`do_tlb_mod` 是支持“异常重入”的，

  ```c
  void do_tlb_mod(struct Trapframe *tf) {
  	struct Trapframe tmp_tf = *tf;	// 保留原先 trap frame 的内容
  
  	if (tf->regs[29] < USTACKTOP || tf->regs[29] >= UXSTACKTOP) {
  		tf->regs[29] = UXSTACKTOP;
  	}
  ```

  在设置用户异常栈指针 (sp) 时，会检查当前的 sp 值是否已经位于异常栈的范围内，如果 sp 已经在异常栈中，说明当前已经在处理一个异常，并且正在处理新的异常，我们保留了 sp 的值也就保留了原先的异常信息。

- 出现这种情况的场景可能是: 用户进程触发了第一次 TLB Mod 异常，内核将其转到用户态的 `cow_entry` 函数处理，在 `cow_entry` 函数执行期间，如果它尝试写入另一个没有 PTE_D 权限位 (可能是另一个 COW 页面) 的页面，就会再次触发 TLB Mod 异常。

- `do_tlb_mod` 函数只会引导程序跳到用户空间去处理TLB Mod异常，即需要借助用户态的 `cow_entry` 函数处理异常，用户态函数需要异常现场的信息

![截屏2025-05-07 18.29.50](./images_report/%E6%88%AA%E5%B1%8F2025-05-07%2018.29.50.png)

### 4.8

在用户态处理页写入异常，相比于在内核态处理有什么优势？

【答案】

减少内核代码的工作量，用户处理出现错误时还可以由内核进行处理，但是如果内核出现错误，就会导致系统崩溃

### 4.9

为什么需要将 `syscall_set_tlb_mod_entry` 的调用放置在 `syscall_exofork` 之前? 如果放置在写时复制保护机制完成之后会有怎样的效果?

【答案】

fork函数中:

```c
int fork(void) {
	u_int child;
	u_int i;
	if (env->env_user_tlb_mod_entry != (u_int)cow_entry) {
		try(syscall_set_tlb_mod_entry(0, cow_entry));
	}
    
	child = syscall_exofork();
    if (child == 0) {
		env = envs + ENVX(syscall_getenvid());
		return 0;
	}	

    // Map all mapped pages below 'USTACKTOP' into the child's address space
	for (i = 0; i < VPN(USTACKTOP); i++) {
		if ((vpd[i >> 10] & PTE_V) && (vpt[i] & PTE_V)) {	// PDE有效且PTE有效
			duppage(child, i);
		}
	}
```

需要在 `syscall_exofork()` 前设置好父进程的TLB Mod异常处理函数，因为在 `syscall_exofork()` 之后， 父进程会遍历地址空间，并调用 `duppage` 函数来复制页面并设置写时复制保护。在 `duppage` 中，父进程会修改它自己的页表项perm。如果此时父进程意外触发页写入异常，也可以正确捕获和处理。

## 实验难点图示

### 系统调用的流程

中断异常的处理流程为:

1. 处理器跳转到异常分发代码处
2. 进入异常分发程序，根据 cause 寄存器值判断异常类型并跳转到对应的处理程序
3. 处理异常，并返回

异常分发向量组中的 `8` 号异常是操作系统处理系统调用时的异常

![截屏2025-05-03 09.25.14](./images_report/%E6%88%AA%E5%B1%8F2025-05-03%2009.25.14.png)

首先，用户主动调用 `syscall_*` ，其定义在 user/lib/syscall_lib.c

```c
void syscall_putchar(int ch) {
	msyscall(SYS_putchar, ch);
}

int syscall_print_cons(const void *str, u_int num) {
	return msyscall(SYS_print_cons, str, num);
}
```

内部调用 `msyscall` ，他是一个拥有可变参数的函数，在 user/lib/syscall_wrap.S 中实现

```c
LEAF(msyscall)
	syscall
	jr ra
END(msyscall)
```

处理器执行 `syscall` ，跳转到异常入口，并执行 entry.S 中的:

```assembly
exc_gen_entry:
	SAVE_ALL
	mfc0    t0, CP0_STATUS
	and     t0, t0, ~(STATUS_UM | STATUS_EXL | STATUS_IE)
	mtc0    t0, CP0_STATUS
	mfc0 t0, CP0_CAUSE
	andi t0, 0x7c
	lw t0, exception_handlers(t0)
	jr t0
```

其中 `exception_handlers` 在 traps.c 中定义

```c
void (*exception_handlers[32])(void) = {
    [0 ... 31] = handle_reserved,
    [0] = handle_int,
    [2 ... 3] = handle_tlb,
#if !defined(LAB) || LAB >= 4
    [1] = handle_mod,
    [8] = handle_sys,
#endif
};
```

随后调用异常处理函数 `handle_*` ，在Lab4中我们主要实现 `handle_sys` 。genex.S 给出了 `handle_sys` 的实现，其本质上是跳转到 `do_syscall` 函数处理异常，

```assembly
#if !defined(LAB) || LAB >= 4
BUILD_HANDLER mod do_tlb_mod
BUILD_HANDLER sys do_syscall
#endif
```

`do_syscall` 的具体实现在 kern/syscall_all.c 中:

```c
void do_syscall(struct Trapframe *tf) {
	int (*func)(u_int, u_int, u_int, u_int, u_int);
	int sysno = tf->regs[4];
	...
}
```

### 系统调用中参数的传递

![截屏2025-05-03 09.34.16](./images_report/%E6%88%AA%E5%B1%8F2025-05-03%2009.34.16.png)

> by Flyinglandlord:
>
> 用户空间向内核传入的参数，syscall_all 中实现的系统调用是如何看到的呢？我觉得这个问题的答案是，其实内核是直接看不到的，但是我们可以在进行系统调用之前做一些操作，伪装成好像有某个函数调用了它并传给了它参数。这就需要用到调用约定，把相应的参数存入相应的寄存器里，这样 syscall_* 函数就可以通过参数直接访问这些数据了。 如上图所示，参数其实放在了 $a0-$a3 ，其余的参数需要压栈，这也就是 syscall.S 中要填写的内容，刚开始没有提示需要看这方面的内容，因此我在理解代码上遇到了一些阻碍。尤其需要注意的是 arg0-arg4 实际上是存在寄存器中的，但是我们仍然在内存中给它们留下了空位，这在编写汇编代码访问参数时尤其需要注意。

### 用户态Fork的流程

![截屏2025-05-04 10.14.28](./images_report/%E6%88%AA%E5%B1%8F2025-05-04%2010.14.28.png)

### 页写入异常处理流程

当写入不能写入的页面时 (即没有PTE_D标识位的页面)，会触发TLB Mod异常，将陷入内核处理异常，流程为:

![截屏2025-05-07 18.29.50](./images_report/%E6%88%AA%E5%B1%8F2025-05-07%2018.29.50.png)

### vpd和vpt指针

`vpt` 和 `vpd` 是用户空间中的特殊指针或数组，它们允许用户程序直接访问自身进程的页表和页目录。

在 MOS 操作系统中，每个进程都有其独立的虚拟地址空间，实现了两级页表结构，由内核管理和维护，用户程序是**无法直接访问和修改页表**的。

同时，MOS 操作系统采用页目录自映射，把进程的页表和页目录本身映射到进程的虚拟地址空间中 (0x7fc00000 到 0x80000000)，具体是通过把UVPT地址范围内的页目录项 `e->env_pgdir[PDX(UVPT)]` 设置为指向页目录自身的物理地址 `PADDR(e->env_pgdir)`。这样，页目录和页表就出现在了进程的虚拟地址空间。

```bash
 o      ULIM     -----> +----------------------------+------------0x8000 0000-------
 o                      |         User VPT           |     PDMAP                /|\
 o      UVPT     -----> +----------------------------+------------0x7fc0 0000    |
 o                      |           pages            |     PDMAP                 |
 o      UPAGES   -----> +----------------------------+------------0x7f80 0000    |
 o                      |           envs             |     PDMAP                 |
```

`vpt` 指向这个自映射区域起始地址，其中按照虚拟地址的顺序存储了所有的页表项，可以通过类似 `vpt[va / PAGE_SIZE]` 的方式来获取 `va` 对应的页表项。可以通过 `vpd[PDX(va)]` 来获取页目录项。

```c
#define vpt ((const volatile Pte *)UVPT)
#define vpd ((const volatile Pde *)(UVPT + (PDX(UVPT) << PGSHIFT)))
```

## 实验体会

Lab4用到了很多Lab2和Lab3的函数，加深了我对这些函数的理解 (btw仍然需要再看看函数的实现) 。Fork还有TLB Mod的处理是在用户态下进行的，顺着这两个流程，也加深了我对user和kern的理解。但是关于 trap frame 还不是很懂，需要再琢磨琢磨。

## Reference

- [flyinglandlord | lab-4 实验总结](http://flyinglandlord.github.io/2022/06/15/BUAA-OS-2022/lab4/doc-lab4/)
- [wokron | BUAA-OS 实验笔记之 Lab4](https://wokron.github.io/posts/buaa-os-lab4/)
- Gemini AI