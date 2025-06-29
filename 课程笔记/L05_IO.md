# I/O

## I/O硬件基本原理

> 总线部分可以看ostep的笔记

![截屏2025-05-13 09.04.54](./images_buaa/%E6%88%AA%E5%B1%8F2025-05-13%2009.04.54.png)

### I/O设备 (Device)

一个设备包含两部分:

- **hardware interface:** allow the system software to control its operation
  - 包含3个寄存器: status, command(告诉设备执行特定任务), data(向设备发送数据或从设备接收数据)
  - 通过读写这些寄存器，操作系统可以控制设备的行为
- **internal structure:** hardware chips implementing devices' functionality

![截屏2025-05-13 09.20.51](./images_buaa/%E6%88%AA%E5%B1%8F2025-05-13%2009.20.51.png)

### 设备控制器 (Device Controller)

外设与CPU要通过**设备控制器**来沟通 (感觉就是设备的hardware interface？)

CPU与设备控制器的通信方式:

- **I/O指令 (Port-Mapped I/O):** CPU使用专门的I/O指令来访问controller的寄存器
- **内存映射I/O (Memory-Mapped I/O):** 将设备控制器的寄存器和数据缓冲区映射到主存的物理地址空间中，CPU就像访问普通内存单元一样，使用 `load`/`store` 指令来访问设备控制器

(两种方式都很常用)

## I/O软件基本原理

### I/O软件层概览

![截屏2025-05-13 19.22.18](./images_buaa/%E6%88%AA%E5%B1%8F2025-05-13%2019.22.18.png)

1. 用户进程: 通过系统调用发起I/O请求，如 `open` `read` `write` `close`
2. 设备无关软件层
3. 设备驱动程序: 针对每类设备控制器编写的特定代码，将设备无关软件层传来的抽象请求（如“从磁盘读一个块”）翻译成对具体设备控制器的操作序列（如向控制寄存器写入特定命令字、数据地址、长度等）
   - 检查设备状态
   - 启动I/O操作
   - 响应中断
4. 中断处理程序: 当I/O操作完成时，设备控制器会向CPU发出中断信号
5. 硬件: 包括设备控制器和物理设备，实际执行I/O操作

### I/O控制技术

**程序控制I/O (PIO, Programmed I/O):**

CPU向I/O发出指令，然后忙等轮询，直到I/O操作完毕再继续执行进程。如下图，1表示进程

![截屏2025-05-13 16.07.18](./images_buaa/%E6%88%AA%E5%B1%8F2025-05-13%2016.07.18.png)

**中断驱动方式:**

不同于CPU一直轮询，OS让调用进程休眠，切换到其他进程运行。当I/O设备完成操作后，会产生一个 interrupt，OS执行中断处理程序并完成响应、唤醒进程。例如:

![截屏2025-05-13 16.15.48](./images_buaa/%E6%88%AA%E5%B1%8F2025-05-13%2016.15.48.png)

进程切换到代价很高，所以如果设备速度快，最好采用轮询的方式；设备速度慢，采用中断驱动更好。对于未知速度的设备，hybrid，即先进行一段时间的轮询，还未响应则中断进程。

**DMA (Direct Memory Access):**

当需要在设备和内存之间进行大量的数据交换时，如果使用PIO，CPU自己需要不断地从内存拷贝数据然后发往设备。此时使用DMA更好，DMA使用专门的控制器来完成**内存**与**设备**间的数据传输。

DMA的工作流程:

1. OS would program the DMA engine by telling it where the data lives in memory, how much data to copy, and which device to send it to.
2. At that point, the OS is done with the transfer and can proceed with other work.
3. When the DMA is complete, the DMA controller raises an **interrupt**, and the OS thus knows the transfer is complete.

![截屏2025-05-13 16.26.16](./images_buaa/%E6%88%AA%E5%B1%8F2025-05-13%2016.26.16.png)

**通道 (Channel):**

I/O通道独立于CPU，是专门负责输入输出的处理器，有自己的指令体系。通道与DMA的原理几乎是一样的，CPU只负责下放任务，通道改进的点在于进一步减少了CPU的干预 (不需要CPU指示数据的内存起始地址和数据块长度等等)，并且通道可同时控制多个设备

### 缓冲技术

缓冲可以匹配CPU与外设的不同处理速度 (外设太慢)，减少对CPU的中断次数

**单缓冲 (single buffer):**

 一个缓冲区，CPU和外设轮流使用，任何时刻只能有一方对缓冲区进行操作

<img src="./images_buaa/%E6%88%AA%E5%B1%8F2025-05-13%2016.54.23.png" alt="截屏2025-05-13 16.54.23" style="zoom:33%;" />

**双缓冲 (double buffer):**

<img src="./images_buaa/%E6%88%AA%E5%B1%8F2025-05-13%2016.54.33.png" alt="截屏2025-05-13 16.54.33" style="zoom: 33%;" />

两个缓冲区，CPU和外设都可以连续处理而无需等待对方。比如播放视频的时候，一块缓冲区 (对应于图中的B) 用于播放当前帧 (处理)，另一块 (缓冲区A) 从磁盘或网络读取下一帧数据 (填充)，播放完就切换身份

要求CPU和外设速度相近

**环形缓冲 (circular buffer):**

生产者从 `head` 处写，消费者从 `tail` 处读。只要有空位，写入无需等待；只要有数据，读取也无需等待。可以处理CPU和外设速度相差大的情况

<img src="./images_buaa/%E6%88%AA%E5%B1%8F2025-05-13%2017.03.35.png" alt="截屏2025-05-13 17.03.35" style="zoom: 40%;" />

**缓冲池 (buffer pool):**

池中有三个队列: 空闲缓冲区、输入缓冲区、输出缓冲区

### 设备分配

外设是有限资源 (例如打印机、键盘)，当多个进程都想使用外设的时候，必须要对资源进行管理

两种管理方法:

- 让不同进程在不同时间段交替使用同一个设备，OS负责切换使用权。比如用户排队打印文件
- 虚拟设备。每个进程看起来都有一个自己的设备，但实际多个虚拟设备共享一个外设

相关数据结构:

- DCT (Device Control Table): 设备控制表，每个设备一张，描述设备的特性和状态

- COCT (Controller Control Table): 记录I/O控制器的状态，如DMA控制器，描述DMA占用的中断号和数据通道的分配
- CHCT (Channel Control Table): 描述通道工作状态
- SDT (System Device Table): 记录所有设备的信息，包括DCT指针、正在使用设备的进程等

单通路I/O系统 (一个设备对应一个控制器，一个控制器对应一个通道) 的设备分配过程:

1. 根据物理设备名查SDT，找到对应的DCT，若设备忙则等待，否则计算是否死锁然后进行分配 (分配设备)
2. 将设备分配给进程后，找到DCT所属的COCT，空闲则分配，否则等待 (分配控制器)
3. 找到COCT所属CHCT，空闲则分配，否则等待 (分配通道)

### 假脱机技术 (Spooling)

假脱机技术 (Spooling, Simultaneous Peripheral Operation On Line): 也叫虚拟设备技术。在多道程序系统中，专门利用一道程序 (Spooling程序) 来完成对设备的I/O操作，而无需使用I/O处理机

Spooling程序进行数据交换:

- Spooling程序预先从外设输入数据并缓冲，在以后需要的时候输入到应用程序
- Spooling程序接受应用程序的输出数据并缓冲，在以后适当的时候输出到外设

### 设备驱动程序 (Device Driver)

将抽象I/O请求转换为对物理设备的请求，例如 Printer driver, CD driver，是内核的一部分

组成:

- 自动配置和初始化子程序: 检测所要驱动的硬件设备是否存在、是否正常
- 服务于I/O请求的子程序: 响应用户的系统调用，如 `read()` ，操作设备
- 中断服务子程序: 当设备完成操作并触发硬件中断时被调用
