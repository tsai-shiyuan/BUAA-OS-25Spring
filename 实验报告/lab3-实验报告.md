# Lab3-Process

## 思考题

### 3.1

**请结合 MOS 中的页目录自映射应用解释代码中** `e->env_pgdir[PDX(UVPT)] = PADDR(e->env_pgdir) | PTE_V` **的含义**

```c
static int env_setup_vm(struct Env *e) {
	struct Page *p;
	try(page_alloc(&p));
	p -> pp_ref++;
	e -> env_pgdir = (Pde *)page2kva(p);
    memcpy(e->env_pgdir + PDX(UTOP), base_pgdir + PDX(UTOP),
       sizeof(Pde) * (PDX(UVPT) - PDX(UTOP)));
	e->env_pgdir[PDX(UVPT)] = PADDR(e->env_pgdir) | PTE_V;
	return 0;
}
```

![截屏2025-04-19 13.21.06](./images_report/%E6%88%AA%E5%B1%8F2025-04-19%2013.21.06.png)

`UVPT` 是 user virtual page table，用于访问进程自己的页表

`e->env_pgdir[PDX(UVPT)]` 代表页表起始地址对应的页目录项

`PADDR(e->env_pgdir)` 是该进程页目录物理地址 (由于页目录本身在内核虚拟地址空间中，写入页目录项的时候需要转换)

UVPT 所在的页目录项指向的页表，其实就是页目录本身

### 3.2

`elf_load_seg` **以函数指针的形式，接受外部自定义的回调函数** `map_page`**。请你找到与之相关的** `data` **这一参数在此处的来源，并思考它的作用。没有这个参数可不可以？**

`elf_load_seg` 函数的定义为:

```c
int elf_load_seg(Elf32_Phdr *ph, const void *bin, elf_mapper_t map_page, void *data) {
    ...
    for (i = offset ? MIN(bin_size, PAGE_SIZE-offset) : 0; i < bin_size; i += PAGE_SIZE) {
		if ((r = map_page(data, va+i, 0, perm, bin+i, MIN(bin_size-i, PAGE_SIZE))) != 0) {
			return r;
		}
	}
    ...
}
```

会被 `load_icode` 函数使用

```c
static void load_icode(struct Env *e, const void *binary, size_t size) {
    ...
    ELF_FOREACH_PHDR_OFF (ph_off, ehdr) {
		Elf32_Phdr *ph = (Elf32_Phdr *)(binary + ph_off);
		if (ph->p_type == PT_LOAD) {	// 需要被加载到内存中			
			panic_on(elf_load_seg(ph, binary + ph->p_offset, load_icode_mapper, e));
		}
	}
    ...
}
```

`data` 参数就是原先的进程指针 `struct Env *e` ，它可以转成 `void *` 任意传递

### 3.3

**结合** `elf_load_seg` **的参数和实现，考虑该函数需要处理哪些页面加载的情况。**

![截屏2025-04-19 14.38.05](./images_report/%E6%88%AA%E5%B1%8F2025-04-19%2014.38.05.png)

函数的实现如下:

```c
int elf_load_seg(Elf32_Phdr *ph, const void *bin, elf_mapper_t map_page, void *data) {
    ...
        
	u_long offset = va - ROUNDDOWN(va, PAGE_SIZE);
	if (offset != 0) {
		if ((r = map_page(data, va, offset, perm, bin, 
                          MIN(bin_size, PAGE_SIZE - offset))) != 0) {
			return r;
		}
	}
```

情况1: va未页面对齐，将开头不对齐的部分 “剪切” 下来，先映射到内存的页中。

```c
	/* Step 1: load all content of bin into memory. */
	for (i = offset ? MIN(bin_size, PAGE_SIZE-offset) : 0; i < bin_size; i += PAGE_SIZE) {
		if ((r = map_page(data, va+i, 0, perm, bin+i, MIN(bin_size-i, PAGE_SIZE))) != 0) {
			return r;
		}
	}
```

情况2: 接着将中间完整的部分加载到页

```c
	/* Step 2: alloc pages to reach `sgsize` when `bin_size` < `sgsize`. */
	while (i < sgsize) {
		if ((r = map_page(data, va + i, 0, perm, NULL, MIN(sgsize - i, PAGE_SIZE))) != 0) {
			return r;
		}
		i += PAGE_SIZE;
	}
```

情况3: 最后处理段大小大于数据大小的情况，不断创建新的页，但是并不向其中加载任何内容

### 3.4

**你认为这里的** `env_tf.cp0_epc` **存储的是物理地址还是虚拟地址?**

```c
static void load_icode(struct Env *e, const void *binary, size_t size) {
    ...
    e -> env_tf.cp0_epc = ehdr -> e_entry;	// ELF文件中设定的程序入口地址
}
```

`env_tf.cp0_epc` 字段指示了进程恢复运行时 PC 应恢复到的位置，是虚拟地址

### 3.5

**试找出 0、1、2、3 号异常处理函数的具体实现位置。8 号异常(系统调用)涉及的** `do_syscall()` **函数将在 Lab4 中实现**

```c
extern void handle_int(void);
extern void handle_tlb(void);
extern void handle_sys(void);
extern void handle_mod(void);
extern void handle_reserved(void);

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

0处理中断，定义在genex.S:

```assembly
NESTED(handle_int, TF_SIZE, zero)
	mfc0    t0, CP0_CAUSE
	mfc0    t2, CP0_STATUS
	and     t0, t2
	andi    t1, t0, STATUS_IM7
	bnez    t1, timer_irq
timer_irq:
	li      a0, 0
	j       schedule	# 在sched.c
END(handle_int)
```

1在本次Lab中处理未定义函数，在traps.c:

```c
void do_reserved(struct Trapframe *tf) {
	print_tf(tf);
	panic("Unknown ExcCode %2d", (tf->cp0_cause >> 2) & 0x1f);
}
```

23处理TLB相关异常，在tlbex.c中:

```c
void do_tlb_mod(struct Trapframe *tf) {
	struct Trapframe tmp_tf = *tf;

	if (tf->regs[29] < USTACKTOP || tf->regs[29] >= UXSTACKTOP) {
		tf->regs[29] = UXSTACKTOP;
	}
	tf->regs[29] -= sizeof(struct Trapframe);
	*(struct Trapframe *)tf->regs[29] = tmp_tf;
	Pte *pte;
	page_lookup(cur_pgdir, tf->cp0_badvaddr, &pte);
	if (curenv->env_user_tlb_mod_entry) {
		tf->regs[4] = tf->regs[29];
		tf->regs[29] -= sizeof(tf->regs[4]);
		// Hint: Set 'cp0_epc' in the context 'tf' to 'curenv->env_user_tlb_mod_entry'.
		/* Exercise 4.11: Your code here. */

	} else {
		panic("TLB Mod but no user handler registered");
	}
}
```

### 3.6

**阅读 entry.S、genex.S 和 env_asm.S 这几个文件，并尝试说出时钟中断在哪些时候开启，在哪些时候关闭。**

进入异常，执行 `exc_gen_entry` 函数，

```assembly
	mfc0    t0, CP0_STATUS
	and     t0, t0, ~(STATUS_UM | STATUS_EXL | STATUS_IE)
	mtc0    t0, CP0_STATUS
```

把 `EXL` 和 `IE` 位都清零，相当于进入了内核态且关闭中断，包括时钟中断

异常处理完成后，执行 `ret_from_exeption` 函数，

```assembly
FEXPORT(ret_from_exception)
	RESTORE_ALL
	eret
```

恢复进程上下文，重新启用中断

### 3.7

**思考操作系统是怎么根据时钟中断切换进程的。**

CPU中的CP0寄存器内置了可产生中断的 Timer，MOS利用此周期性地产生中断。当时钟中断产生时，cpu 就会自动跳转到虚拟地址 `0x80000180` 处，从此处执行程序，即 `exc_gen_entry` (定义在 kern/entry.S)

在 `exc_gen_entry` 函数内，会判断是哪一类异常，并跳到专门的异常处理程序去处理，对于时钟中断，则跳至 `handle_int` 函数 (定义在 kern/genex.S) ，进一步跳到 `schedule` 函数进行进程调度、进程切换并 `env_run()` 

## 实验难点

### 内存空间布局

![截屏2025-04-19 13.21.06](./images_report/%E6%88%AA%E5%B1%8F2025-04-19%2013.21.06.png)

### 进程映像的加载

```c
int elf_load_seg(Elf32_Phdr *ph, const void *bin, elf_mapper_t map_page, void *data) {
	u_long va = ph->p_vaddr;
	size_t bin_size = ph->p_filesz;
	size_t sgsize = ph->p_memsz;
	u_int perm = PTE_V;
	if (ph->p_flags & PF_W) {
		perm |= PTE_D;
	}

	int r;
	size_t i;
	u_long offset = va - ROUNDDOWN(va, PAGE_SIZE);
	if (offset != 0) {
		if ((r = map_page(data, va, offset, perm, bin,
				  MIN(bin_size, PAGE_SIZE - offset))) != 0) {
			return r;
		}
	}

	/* Step 1: load all content of bin into memory. */
	for (i = offset ? MIN(bin_size, PAGE_SIZE-offset) : 0; i < bin_size; i += PAGE_SIZE) {
		if ((r = map_page(data, va + i, 0, perm, bin + i, MIN(bin_size - i, PAGE_SIZE))) !=
		    0) {
			return r;
		}
	}

	/* Step 2: alloc pages to reach `sgsize` when `bin_size` < `sgsize`. */
	while (i < sgsize) {
		if ((r = map_page(data, va + i, 0, perm, NULL, MIN(sgsize - i, PAGE_SIZE))) != 0) {
			return r;
		}
		i += PAGE_SIZE;
	}
	return 0;
}
```

```c
int elf_load_seg(Elf32_Phdr *ph, const void *bin, elf_mapper_t map_page, void *data) {
```

根据程序头表中的信息将 `bin` 中的数据加载到指定位置

`Elf32_Phdr *ph` 指向ELF文件段头，Program Header 包含段的虚拟地址、物理地址、大小、标志等信息

`const void *bin` 指向ELF文件二进制代码和数据

`elf_mapper_t map_page` 是一个回调函数，用于将数据映射到虚拟地址所在的页上

`void *data` 回调函数的参数

elf.h 中定义了 `elf_mapper_t`，此类型的函数接收数据要加载到的虚拟地址 `va`，数据加载的起始位置相对于页的偏移 `offset`，页的权限 `prem`，所要加载的数据 `src` 和要加载的数据大小 `len` 

```c
typedef int (*elf_mapper_t)(void *data, u_long va, size_t offset, u_int perm, const void *src, size_t len);
```

为函数原型 `int (*elf_mapper_t)(...);` 起了新名字 `elf_mapper_t` ，这是函数指针类型，以后就可以用 `elf_mapper_t f` 

```c
const Elf32_Ehdr *elf_from(const void *binary, size_t size);
```

`elf_from` 函数仅仅是检查是否符合ELF文件格式，并将 `binary` 以 `Elf32_Ehdr` 指针的形式返回

### 进程调度

实验实际采用的是经典的时间片轮转调度算法 (Round-Robin Scheduling)。进程的优先级在数值上等于运行所需要的时间片个数。切换到该进程时 `count` 设为优先级，每经过一次时钟中断，计数器 `count--` ，减到 `0` 说明当前进程时间片耗尽。这种算法保证每个进程都能公平获得CPU时间。

## 体会与感想

Lab 3 大量地用到 Lab 2 写好的页面相关函数，加深了我对内存管理和进程运行的理解。

然后我对地址映射这一块还不是特别清楚，再回炉重造一下 Lab 2 吧。

## Reference

- [Wokron｜BUAA-OS 实验笔记之 Lab3](https://wokron.github.io/posts/buaa-os-lab3/)
- [flyinglandlord｜lab-3 实验总结](https://flyinglandlord.github.io/2022/06/15/BUAA-OS-2022/lab3/doc-lab3/)
