# 进程的同步与互斥

> The best kind of approach would be fast and correct.

## 同步与互斥

### 问题引入

进程的三个特征:

- 并发
- 共享: 比如共享打印机
- 不确定性: 进程执行的结果与其执行的相对速度有关，是不确定的

**临界资源:** 一次仅允许一个进程访问的资源称为临界资源，例如打印机

**临界区:** 每个进程中访问临界资源的那段代码称为临界区

**同步与互斥:**

- 互斥: 两个及以上的进程不能同时进入临界区域，并没有规定次序，只是时间的变量
- 同步: 在互斥的基础上（大多数情况），通过其它机制实现访问者对资源的**有序**访问 (常常利用相互交换信息而产生制约关系)

可以通过软件和硬件实现互斥⬇️

互斥区管理所遵循的原则:

- **空闲让进**：临界资源处于空闲状态，允许进程进入临界区
- **忙则等待**：临界区有正在执行的进程，所有其他进程则不可以进入临界区
- **有限等待**：对要求访问临界区的进程，应在保证在有限时间内进入自己的临界区，避免死等
- **让权等待**：当进程（长时间）不能进入自己的临界区时，应立即释放处理机，尽量**避免忙等**

### 忙等的软件实现

**Dekker算法**

- 每个进程有一个 `pturn` 或 `qturn` 表示是否想进入临界区，就像举手🙋说“我想进临界区”

- `turn` 变量决定让权，比如: 同时举手，`turn` 就说轮到xxx了，xxx先进临界区，ok那另一个人就先放下手，等到xxx执行完了，再举手申请

- 进程P的代码

  ```c
  pturn = true;
  while (qturn) {
      if (turn == 1) {	// 让权
          pturn = false;
          while (turn == 1);	// 忙等
          pturn = true;
      }
  }
  // 临界区
  ...
  turn = 1;
  pturn = false;
  ```

- 进程Q的代码

  ```c
  qturn = true;
  while (pturn) {
      if (turn == 0) {
          qturn = false;
          while (turn == 0);
          qturn = true;
      }
  }
  // 临界区
  ...
  turn = 0;
  qturn = false;
  ```

- 缺点是中间 `while` 部分存在忙等，浪费CPU时间

**Peterson算法**

- `interested[i]` 表示进程是否想进入临界区，`turn` 表示轮到谁了

- 首先礼貌地让对方先来，然后再声明“我感兴趣”，只有等到“对方不想进”或者“轮到我”时再进入临界区

  ```c
  #define FALSE 0
  #define TRUE 1
  #define N 2		// 进程的个数,只适用于2个进程的情况
  int turn;
  int interested[N];
  
  void enter_region(int process) {
      int other = 1 - process;	// 另一个进程的进程号
      turn = other;	// 假设对方先来
      interested[process] = TRUE;	// 本进程想访问临界资源    	
      while (turn == other && interested[other] == TRUE);
  }
  
  void leave_region(int process) {
      interested[process] = FALSE;
  }
  
  // 进程i
  enter_region(i);
  // 临界区
  ...
  leave_region(i);
  ```

**Lamport Bakery Algorithm**

- Leslie Lamport, 1974，可以处理多个进程的互斥

- 买面包排队: 顾客进入面包店前，首先抓取一个号码，然后按号码从小到大的次序依次进入面包店购买面包。号码从小到大发放，顾客看到了一个号码，他就觉得这是自己的序号了 (可以很多人同时“看到”一个号码)，得到同样号码的顾客按照名字字典序排

- 顾客相当于进程，每个进程有唯一的标识 $P_i$ ，`choosing[i]` 为1表示正在抓号，再设置 `choosing[i] = 0` 表示抓完了号。然后比较优先级，编号小者优先，如果 `j` 比自己早，就等待它完成临界区

  ```c
  #define N 100
  int choosing[N];
  int number[N];
  
  int max_number() {
      int max = 0;
      for (int i = 0; i < N; ++i)
          if (number[i] > max)
              max = number[i];
      return max;
  }
  
  void lock(int i) {
      choosing[i] = 1;
      number[i] = 1 + max_number();
      choosing[i] = 0;
  
      for (int j = 0; j < N; ++j) {
          if (j == i) continue;
          while (choosing[j]) ; // 等待 j 选完号
          // number[j] == 0 对应的是j并不想进入临界区或者已经出临界区的情况
          while (number[j] != 0 &&
                 (number[j] < number[i] || 
                 (number[j] == number[i] && j < i))) ;
      }
  }
  
  void unlock(int i) {
      number[i] = 0;
  }
  
  Thread (int i) {
      while (1) {
          lock(i);
          // critical section
          ...
          unlock(i);
      }
  }
  ```

### 忙等的硬件实现

中断屏蔽: 

- 在临界区关闭中断 (无法切换到别的进程)，退出临界区前打开中断
- 效率低，不常使用

**TestAndSet指令:** 

- `TestAndSet(..)` 是一种不可中断的硬件原语 (如果一个进程正在执行TAS，在他完成之前，其他进程不可以执行)，它会写值到某个内存位置并传回其旧值

  ```c
  bool TestAndSet(bool *target) {
      bool old = *target;
      *target = true;
      return old;
  }
  ```

- 用TAS指令实现自旋锁 (spinlock) 互斥: 如果你是第一个设置 `lock` 为 `true` 的线程，因为此时返回旧值为`false`，那么你就能进入临界区。其他线程发现 `TestAndSet` 返回为 `true` ，就不停循环等待

  ```c
  bool lock = false;
  
  void enter_critical_section() {
      while (TestAndSet(&lock));  // 忙等
  }
  
  void leave_critical_section() {
      lock = false;
  }
  ```

**swap指令:** 

- swap也是不会被中断的原子指令，其功能是交换两个字的内容

  ```c
  swap(bool *a, bool *b) {
      bool temp;
      temp = *a;
      *a = *b;
      *b = temp;
  }
  ```

- 使用swap实现进程互斥:

  ```c
  int lock = 0;
  
  void lock_acquire() {
      int key = 1;
      while (key == 1) {
          swap(&lock, &key);
      }
  }
  
  void lock_release() {
      lock = 0;
  }
  ```

> 以上锁均会造成“轮询”，能否像OO那样使用wait-notify? ⬇️

### 信号量机制

**信号量机制:**

- P(S), V(S)
- S是信号量 (semaphore)，在生成并**赋上初值**后只能被 P(test) 和 V(verhogen) 进行操作，不受进程调度的打断

  ```c
  Type semaphore = record
      value : integer;	// value是信号量的计数器值
      L : list of process;	// 进程链表
  end
      
  Procedure P(S)		// P操作,也称wait操作
  begin
      S.value := S.value - 1;
      if S.value < 0 then block(S.L);		// 加入等待队列,并阻塞该进程
  end
  
  Procedure V(S)		// V操作,也称signal操作
  begin
      S.value := S.value + 1;
      if S.value <= 0 then wakeup(S.L)	// 唤醒一个进程
  end
  ```

- S.value为正时表示资源的个数，S.value为负时表示等待进程的个数。例如 S.value = 2 表示资源还有2个，S.value = -2 表示正有2个进程被阻塞。P操作分配资源，如果无法分配则阻塞wait；V操作释放资源，如果有等待进程则唤醒

- **信号量机制的实现:** 

  - 需要考虑原子性问题，例如整段P操作就是原子操作 (可以关中断或TAS指令实现)；
  - 用PCB链表

- 应用: 

  - 进程互斥 (Mutual exclusion): S=1

  - 进程同步 (Synchronization): 指当一个进程 $P_i$ 想要执行一个 $a_i$ 操作时，它只在进程 $P_j$ 执行完 $a_j$ 后，才会执行 $a_i$ 操作。将信号量初始化为0，$P_i$ 执行 $a_i$ 前进行P操作；而 $P_j$ 执行 $a_j$ 操作后，执行V操作，将 $P_i$ 唤醒

  - 有限并发 (Bounded concurrency): 指有 $n$ ($1 \leqslant n \leqslant c$, $c$ 为常量) 个进程并发执行的一个函数或一个资源，初始值为 $c$ 的信号量可以实现这种并发

    ```c
    var S : semaphore = c;  // 允许的最大并发数
    
    Procedure access_shared_resource():
        P(S);               // 尝试获取信号量
        use_resource();     // 临界区：最多c个进程可同时进入
        V(S);               // 释放信号量
    ```

**多进程同步原语 Barriers:** 等所有进程都**到达**Barrier后才开始执行

```c
n = the number of threads
count = 0                   // 记录已到达 Barrier 的线程数
semaphore mutex = 1         // 保护 count 的互斥锁
semaphore barrier = 0

// 线程到达Barrier
mutex.wait()                // 进入临界区（获取保护count的锁）
count = count + 1
if count == n:           
    barrier.signal()       
mutex.signal()              // 离开临界区（释放锁）
        
// 线程阻塞与接力唤醒
barrier.wait()
barrier.signal()            // 唤醒下一个线程
```

- `barrier.wait()` :
  - 如果 `barrier = 0` 当前线程会被阻塞
  - 如果 `barrier > 0` 说明有线程调用了 `signal` ，线程会继续执行
- `barrier.signal()` : 每个被唤醒的线程必须再次调用 `signal`，接力唤醒下一个线程
- 前 n-1 个线程执行 `barrier.wait()` 时，因为 `barrier=0` 都被阻塞；第 n 个线程检测到 `count==n` 唤醒一个线程

**信号量集:**

- 指*同时需要多个资源*时的信号量操作

- AND型信号量集:

  - 基本思想: 将进程需要的所有共享资源一次全部分配给它，待该进程使用完后再一起释放

  ```c
  SP(S1, S2,...,Sn)
  if S1 >= 1 and ... and Sn >= 1 then
      for i := 1 to n do
          Si := Si - 1;
  else
  	wait in Si;
  
  SV(S1, S2,...,Sn)
  for i := 1 to n do
  	Si := Si + 1;
  	wake waited process
  ```

- 一般“信号量集”机制:

  - 是AND的拓展，更精细地控制进程资源
  - 引入测试值 ($t_i$) 和占用值 ($d_i$)
  - **测试值**: 进程在申请资源时，检查信号量 $S_i$ 是否满足 $S_i \geqslant t_i$ ，即判断资源是否足够
  - **占用值**: 若资源足够，则实际分配时修改信号量 $S_i = S_i - d_i$；释放资源时修改 $S_i = S_i + d_i$

  ```c
  SP(S1, t1, d1,..., Sn, tn, dn)
      if S1 >= t1 and ... and Sn >= tn then
          for i := 1 to n do
          Si := Si - di;
          endfor
      else
          wait in Si;
      endif
  
  SV(S1, d1,...,Sn, dn)
      for i := 1 to n do
          Si := Si + di;
          wake waited process
      endfor
  ```

  - SP(S, 1, 1) 互斥信号量
  
- PV操作优缺点: 简单但是容易出现死锁

### 管程 (Monitor)

**管程**: 将**共享数据**和**操作这些数据的方法**封装在一起，并保证**同一时间只有一个线程/进程能执行管程内的代码**。例如Java中的 `synchronized` ，主要包含:

- 局部控制变量 (临界资源)
- 初始化代码: 对控制变量进行初始化的代码
- 操作原语 (互斥): 对控制变量和临界资源进行操作的一组原语过程，是访问该管程的唯一途径
- 条件变量 (同步): 每个条件变量表示一种等待原因，当定义一个条件变量x时，系统就建立一个相应的等待队列。访问条件变量必须拥有管程的锁
  - wait(x): 把调用者进程放入x的等待队列，直到条件满足
  - signal(x): 唤醒x等待队列中的一个进程

**避免管程中有两个活跃的进程**:

- Hoare管程 (阻塞式条件变量): 当一个进程执行 `signal(x)` 时，唤醒的进程会立即获得管程的控制权，而执行`signal(x)`的进程则被挂起
- Mesa管程 (非阻塞式条件变量): 执行 `signal(x)` 的进程会继续执行，之后才会执行被唤醒的进程
- Hansen管程: `signal(x)` 操作是管程中最后一个操作，执行 `signal(x)` 的进程会立即退出管程，而被唤醒的进程也会在管程退出时立即获得控制权

## 进程通信

### 管道 (Pipe)

管道是单向的，数据只能从一个进程流向另一个进程，如果想双方通信，需要建立起两个管道

- **匿名管道**: 用于有亲缘关系的进程 (如父子进程)，数据通过管道从写端传送到读端
- **命名管道(FIFO)**: 可以在没有亲缘关系的进程之间通信，且可以在不同的进程中访问

管道接口: 可以通过系统调用 (如 `pipe()`、`read()`、`write()` 等) 实现

工作原理: 

1. 当一个进程向管道写入数据时，数据将进入管道的缓冲区
2. 另一个进程从管道读取数据，数据将从缓冲区中移出
3. 如果管道的缓冲区已满，写入操作会被阻塞，直到有空间可用；同样，如果管道为空，读取操作会阻塞，直到有数据可读

### 共享内存

同一块物理内存被映射到进程A、B各自的进程地址空间，允许多个进程共享一个内存区域。需要使用同步机制来防止数据竞争和保证一致性

### 消息系统

消息通过队列传递，进程通过发送消息到队列或者从队列中读取消息来进行通信

<img src="./images_buaa/%E6%88%AA%E5%B1%8F2025-04-14%2020.30.47.png" alt="截屏2025-04-14 20.30.47" style="zoom:33%;" />

## 经典的同步互斥问题

### 生产者-消费者问题 🍖

若干进程通过有限的共享缓冲区交换数据，任何时刻只能有一个进程对共享缓冲区进行操作

![截屏2025-04-16 21.03.26](./images_buaa/%E6%88%AA%E5%B1%8F2025-04-16%2021.03.26.png)

信号量设置:

```c
semaphore mutex = 1;
semaphore empty = N;	// 空闲数量
semaphore full = 0;		// 产品数量
```

`full+empty=N` 是缓冲区大小；`mutex` 互斥锁保证一次只有一个进程访问缓冲区

生产者: 

```c
P(empty);
P(mutex);
	one >> buffer;
V(mutex);
V(full);
```

消费者:

```c
P(full);
P(mutex);
	one << buffer;
V(mutex);
V(empty);
```

不能将P交换顺序，V可以

```c
P(mutex);	// wrong
P(full);
	one << buffer;
V(empty);
V(mutex);
```

当消费者拿到mutex锁，并检测到 `full=0` 时，会挂起，等生产者生产产品，但此时生产者无法拿到mutex锁了！形成死锁

**完整的代码实现:**

```c
semaphore full = 0;
semaphore empty = n;
semaphore mutex = 1;
ItemType buffer[0...n-1];
int in = 0, out = 0;

producer() {
    while (true) {
        生产产品nextp
        P(empty);
        P(mutex);
        buffer[in] = nextp;
        in = (in + 1) % n;
        V(mutex);
        V(full);
    }
}

consumer() {
    while (true) {
        P(full);
        P(mutex);
        nextc = buffer[out];
        out = (out + 1) % n;
        V(mutex);
        V(empty);
        消费nextc产品
    }
}
```

### 读者-写者问题 ✍️

对共享资源的读写操作，任一时刻写者最多只允许一个，读者允许多个

**第一种写法 (读者优先策略):**

```c
semaphore wmutex = 1;	// 写者互斥写
semaphore mutex = 1;	// 对readcount的互斥操作
int readcount = 0;	// 正在读的进程数

writer() {
    P(wmutex);
    Write....
    V(wmutex);
}

reader() {
    P(mutex);	// 申请修改readcount
    if readcount == 0 then P(wmutex);	// 第一个读者,锁住写者(防止写者进入)
    readcount := readcount + 1;
    V(mutex);
    
    Read...
        
	P(mutex);	// 申请修改readcount
    readcount := readcount - 1;
    if readcount == 0 then V(wmutex);	// 最后一个读者,允许写者进入
    V(mutex);
}
```

想象有一把访问资源的锁，第一个读者和写者竞争这把锁，读者抢到后帮其他读者打开门 (不需要他们再获取锁)

算法对读者有利，因为只要有读者想读就允许他们并发读，即使有写者在等待

**另一种写法 (写者优先算法):**

```c
semaphore rcmutex = 1;
semaphore wmutex = 1;
semaphore readTry = 1;
semaphore wcmutex = 1;
int readcount = 0;
int writecount = 0;

reader() {
    P(readTry);
    P(rcmutex);
    readcount++;
    if (readcount == 1) P(wmutex);
    V(rcmutex);
    V(readTry);
    
    Read...
	
	P(rcmutex);
    readcount--;
    if (readcount == 0) V(wmutex);
    V(rcmutex);
}

writer() {
    P(wcmutex);
    writecount++;
    if (writecount == 1) P(readTry);	// 写者出现后,后续读者不允许read
    V(wcmutex);
    
    P(wmutex);
    Write...
	V(wmutex);
    
    P(wcmutex);
    writecount--;
    if (writecount == 0) V(readTry);
    V(wcmutex);
}
```

另一种写法 (读写公平的算法):

```c
semaphore rwmutex = 1;
semaphore wmutex = 1;
semaphore rcmutex = 1;	// 保护readcount

reader() {
    P(rwmutex);	// 排队进入
    P(rcmutex);
    if (readcount == 0) P(wmutex);
    readcount++;
    V(rcmutex);
    V(rwmutex);	// 把 rwmutex 放到 Read 后? 读读会互斥,NO
    
    Read...
	
	P(rcmutex);
    readcount--;
    if (readcount == 0) V(wmutex);
    V(rcmutex);
}

writer() {
    P(rwmutex);	// 排队进入
    P(wmutex);
    
    Write...
        
	V(wmutex);     
    V(rwmutex);
}
```

### 哲学家进餐 🍽️

5个哲学家围绕一张圆桌而坐，桌子上放着5支筷子，每两个哲学家之间放一支。哲学家的动作包括思考和进餐:

- 进餐时需要同时拿起他左边和右边的两支筷子
- 思考时则同时将两支筷子放回原处

如何才能不出现相邻者同时进餐，并且不出现有人永远无法进餐？

方案1: 至多只允许四个哲学家同时拿起筷子进餐，保证至少有一个哲学家能够进餐

```c
eater() {
    P(max_diners);       
    P(left_chopstick);
    P(right_chopstick);
    
    Eat...
        
    V(left_chopstick);
    V(right_chopstick);
    V(max_diners);
}
```

方案2: 每个人都先拿编号小的筷子，再拿编号大的

```c
if (left < right) {
    P(chopstick[left]);
    P(chopstick[right]);
} else {
    P(chopstick[right]);
    P(chopstick[left]);
}
```

方案3: 同时拿起两根筷子，否则不拿起

```c
Varchopstick : array[0..4] of semaphore;
think;
SP(chopstick[(i+1)mod 5], chopstick[i]);
eat;
SV(chopstick[(i+1)mod 5], chopstick[i]);
```

### 理发师问题 💇‍♀️

理发店里有 1 位理发师、1 把理发椅和 n 把供等候理发的顾客坐的椅子；如果没有顾客，理发师便在理发椅上睡觉，当一个顾客到来时，叫醒理发师；如果理发师正在理发时，又有顾客来到，则如果有空椅子可坐，就坐下来等待，否则就离开。

![截屏2025-05-18 16.58.35](./images_buaa/%E6%88%AA%E5%B1%8F2025-05-18%2016.58.35.png)

<img src="./images_buaa/%E6%88%AA%E5%B1%8F2025-05-18%2017.08.30.png" alt="截屏2025-05-18 17.08.30" style="zoom: 33%;" />

![截屏2025-05-18 17.12.17](./images_buaa/%E6%88%AA%E5%B1%8F2025-05-18%2017.12.17.png)

```c
semaphore mutex = 1;	// 保护waiting
semaphore customers = 0;	// 等待理发的顾客数量
semaphore barber = 0;
int waiting = 0;

barber() {
    while (true) {
        P(customers);
        P(mutex);
        waiting--;
        V(mutex);
        Cut hair;
        V(barber);	// 理发师唤醒下一位等待的顾客           
    }
}

customer_i() {
    P(mutex);
    if (waiting < N) {
        waiting++;
        V(mutex);
        V(customers);	// 增加顾客,唤醒理发师
        P(barber);	// 申请理发师
        Cut hair;
    } else {
        V(mutex);
    }
}
```

### 小结

设 `block = semaphore(0)` 用于阻塞
