# Bootstrap

OS启动 (Bootstrap)

- 必须运行程序来启动计算机
- 程序又必须要运行在硬件上

$\Rightarrow$ 逐步解放OS

Bootloader (实现依赖于具体硬件)

- 初始化OS硬件 (用汇编)
- 加载OS到内存

启动过程

step 1: 加载BIOS

step 2: 读取MBR (Master Boot Record)(磁盘第0磁头第0磁道第一个扇区)，此处安装了OS的Bootloader

step 3: Bootloader

- Bootloader 是在操作系统内核运行之前运行的一段小程序，能初始化硬件设备、建立内存空间的映射图，分为stage1和stage2
- stage1运行的程序直接从非易失存储器上 (比如 ROM 或 FLASH) 加载，初始化硬件设备
- stage2运行在 RAM 中，此时有足够的运行环境从而可以用 C 语言来实现较为复杂的功能，会将内核镜像从存储器读到 RAM 中

![截屏2025-03-16 14.35.25](./images_buaa/%E6%88%AA%E5%B1%8F2025-03-16%2014.35.25.png)

Task of kernel:

- 资源管理
  - Process management
  - Memory management
  - Device management
- 用户服务
  - System call
