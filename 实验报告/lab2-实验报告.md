# Lab2-Memory

## 思考题

### 2.1

在编写的 C 程序中，指针变量中存储的地址被视为虚拟地址，还是物理地址？MIPS 汇编程序中 lw 和 sw 指令使用的地址被视为虚拟地址，还是物理地址？

都是虚拟地址

### 2.2

Q: 用宏来实现链表的好处？

宏通过参数化的方式来定义链表操作，这样就可以避免为不同类型的链表编写重复的代码。增强代码的可扩展性和可维护性。

Q: 实验环境中的 /usr/include/sys/queue.h 中单向链表与循环链表与双向链表的差异

```c
#define SLIST_HEAD(name, type)                                          \
struct name {                                                           \
        struct type *slh_first; /* first element */                     \
}

#define SLIST_ENTRY(type)                                               \
struct {                                                                \
        struct type *sle_next;  /* next element */                      \
}

#define SLIST_REMOVE(head, elm, type, field) do {                       \
        if ((head)->slh_first == (elm)) {                               \
                SLIST_REMOVE_HEAD((head), field);                       \
        }                                                               \
        else {                                                          \
                struct type *curelm = (head)->slh_first;                \
                while(curelm->field.sle_next != (elm))                  \
                        curelm = curelm->field.sle_next;                \
                curelm->field.sle_next =                                \
                    curelm->field.sle_next->field.sle_next;             \
        }                                                               \
} while (/*CONSTCOND*/0)
```

单向链表在删除某个节点的时候需要遍历链表找到前驱节点，具有 O(n) 的复杂度

循环链表将最后一个节点指向头节点，性能与单向链表差不多

### 2.3

Page_list的结构

`queue.h` 定义了相关宏，

```c
#define LIST_HEAD(name, type) 						\
	struct name {			  						\
		struct type *lh_first;	/* first element */	\
	}

#define LIST_ENTRY(type) 	\
	struct { 				\
		struct type *le_next;  /* next element */ 					  \
		struct type **le_prev; /* address of previous next element */ \
	}
```

`pmap.h` 中定义

```c
LIST_HEAD(Page_list, Page);
typedef LIST_ENTRY(Page) Page_LIST_entry_t;
struct Page {
	Page_LIST_entry_t pp_link; /* free list link */
	u_short pp_ref;
};
```

对宏进行替换，得到:

```c
struct Page_list {
    struct Page *lh_first;
};

typedef struct {
    struct Page *le_next;
    struct Page **le_prev;
} Page_LIST_entry_t;

struct Page {
  	struct {
        struct Page *le_next;
        struct Page **le_prev;
    } pp_link;
    u_short pp_ref;
};
```

最终

```c
struct Page_list {
    struct {
        struct {
            struct Page *le_next;
            struct Page **le_prev;
        } pp_link;
        u_short pp_ref;
    } *lh_first;
};
```

### 2.4

**ASID (Address Space Identifier)** 用来标识不同地址空间，在多任务的操作系统中，操作系统的每个进程通常有自己的地址空间，这些虚拟地址会通过映射到物理内存来执行代码、读写数据等。ASID确保一个进程的内存访问不会影响到其他进程，确保进程的安全性。

4Kc 中可容纳不同的地址空间的最大数量？

![截屏2025-04-07 16.23.27](./images_report/%E6%88%AA%E5%B1%8F2025-04-07%2016.23.27.png)

在 MIPS 4Kc 中，ASID 位数通常是 8 位，则其最多可表示 2^8 个地址空间。

### 2.5

`tlb_invalidate` 的作用是清除TLB中 < VPN, ASID > $\rightarrow$ < PFN, N, D, V, G > 的映射关系，函数定义在 tlbex.c 中:

```c
void tlb_invalidate(u_int asid, u_long va) {
	tlb_out((va & ~GENMASK(PGSHIFT, 0)) | (asid & (NASID - 1)));
}
```

通过 `a0` 寄存器传入参数，调用 `tlb_out` 来实现

`tlb_out` 中:

```assembly
LEAF(tlb_out)
.set noreorder	# 关闭指令重排
	mfc0    t0, CP0_ENTRYHI	# 备份当前 CP0_ENTRYHI (虚拟地址的高位)
	mtc0    a0, CP0_ENTRYHI	# a0 存的旧表项的 key
	nop
	tlbp	# probe TLB entry, 将表项的索引存入Index
	nop
	mfc0    t1, CP0_INDEX	# TLB查询结果
.set reorder
	bltz    t1, NO_SUCH_ENTRY	# 小于0表示没有此表项
.set noreorder
	mtc0    zero, CP0_ENTRYHI	# 写0
	mtc0    zero, CP0_ENTRYLO0
	mtc0    zero, CP0_ENTRYLO1
	nop
	tlbwi	# write CP0.EntryHi/Lo into TLB at CP0.Index
.set reorder
NO_SUCH_ENTRY:
	mtc0    t0, CP0_ENTRYHI
	j       ra
END(tlb_out)
```

### 2.6

请结合 Lab2 开始的 CPU 访存流程与下图中的 Lab2 用户函数部分，尝试将函数调用与 CPU 访存流程对应起来，思考函数调用与 CPU 访存流程的关系。

基本的函数调用流程通常包括: 保存调用者的上下文，跳转到函数，执行函数 (对堆和栈操作)，返回调用者。这些操作通常会涉及对内存的读写，会在物理内存或缓存中进行。

### 2.7

简单了解并叙述 X86 体系结构中的内存管理机制，比较 X86 和 MIPS 在内存管理上的区别。

X86的内存管理机制通过硬件和操作系统的协作实现。X86支持多级分页，通常会使用4级分页 (PML4、PDPT、PGD、PTE)。与MIPS的区别有: X86有保护模式，支持虚拟内存、内存保护、多个任务并行执行等高级功能，它使得操作系统隔离各个进程的地址空间。在保护模式下，X86支持段式管理，用于进一步细化对内存的管理。每个段有不同的权限，操作系统可以通过段选择子 (segment selector) 来访问特定的内存段。

### A.1

页面大小4KB，页内偏移为12位。一级页表指向512个二级页表，每个二级页表指向512个三级页表。

若三级页表的基地址为PTbase， 则三级页表页目录的基地址应当是PTbase + PTbase >> 9，映射到页目录自身的页目录项地址为 PTbase + PTbase >> 9 + PTbase >> 18

## 难点分析

> 实验笔记放到[博客](https://www.chasetsai.cc/Dev/Operating%20System/lab2_thinking/)上了

### 理解链表宏

与数据结构课上不同，链表宏把数据和连接项分开，数据就是 Page 结构体中 ref 等变量，而连接项 pp_link 是由 LIST_ENTRY 宏指定的，对于任何一种数据结构组成链表都相同的 le_next 和 le_prev。其次链表宏不局限于单一类型，而是用宏实现了泛型，这种封装思想很值得学习。此外，连接项中的指针并不是简单地指向前后链表元素，le_prev 指向的是前一个元素的 le_next，需要好好理解。

还有一个细节，宏定义时可以用 `do...while` 来封装一些语句

### 虚拟地址

```c
static inline u_long page2kva(struct Page *pp) {
	return KADDR(page2pa(pp));
}
```

```c
int page_alloc(struct Page **new) {
	struct Page *pp;
	if (LIST_EMPTY(&page_free_list)) {
		return -E_NO_MEM;	// 定义在error.h
	}
	pp = LIST_FIRST(&page_free_list);
	LIST_REMOVE(pp, pp_link);
	memset((void *)page2kva(pp), 0, PAGE_SIZE);
	*new = pp;
	return 0;
}
```

Line 8 的 `memset((void *)page2kva(pp), 0, PAGE_SIZE);` 我觉得，因为程序中使用的都是虚拟地址，所以要转成虚拟地址再memset

## 实验体会

好多地址操作还是不太清楚，希望结合Lab3的进程管理再好好理解一下。然后这段时间正好和OO多线程电梯冲突了，我花了比较多的时间在OO上，到ddl前才断断续续看完Lab2，也没理解透彻，以后的Lab需要更加合理地规划时间！ (真希望自己也能有丝分裂成多线程啊～)

## Reference

- [Wokron｜BUAA-OS 实验笔记之 Lab2](https://wokron.github.io/posts/buaa-os-lab2/)