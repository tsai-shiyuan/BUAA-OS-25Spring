# Lab0-Linux

## 思考题

### 0.1

```bash
init > RE.txt > Untracked.txt > write RE.txt & git add > Stage.txt > git commit > modify RE.txt > Modified.txt 
```

我们创建新的文件后查看 `status` ，会提示我们使用 `git add` 跟踪；已暂存后再修改文件，提示"尚未暂存以备提交的变更"

### 0.2

<img src="./images_report/%E6%88%AA%E5%B1%8F2025-02-19%2011.15.16(2).png" alt="截屏2025-02-19 11.15.16(2)" style="zoom: 50%;" />Thingking 0.2

### 0.3

1. `print.c` 被误删时，应当使用什么命令将其恢复？

   ```bash
   $ git restore print.c
   ```

2. `print.c` 被错误删除后，执行了 `git rm print.c` 命令，此时应当使用什么命令将其恢复？

   如果还未 `commit` :

   ```bash
   $ git restore
   ```

3. 无关文件 `hello.txt` 已经被添加到暂存区时，如何在不删除此文件的前提下将其移出暂存区?

   ```bash
   $ git rm --cached hello.txt
   ```

### 0.4

经过三次修改和三次提交后，`git log` 会显示:

```bash
commit 2d9428eec7ff1fafb1acc6d249d29a950e03e491 (HEAD -> newBranch)
Author: Chase <23373253@buaa.edu.cn>
Date:   Wed Mar 12 14:43:22 2025 +0800

    3

commit cdf9504433b9541644e5fd8dbb77e0748061b7b9
Author: Chase <23373253@buaa.edu.cn>
Date:   Wed Mar 12 14:42:49 2025 +0800

    2

commit ebf278d3abf3fe74c5e52c8fd4ab5d84ef8aa9b2
Author: Chase <23373253@buaa.edu.cn>
Date:   Wed Mar 12 14:42:26 2025 +0800

    1
```

如果用 `git reset --hard HEAD^` 或 `git reset --hard <hash>` 退回之前的提交，其后的提交记录会消失

```bash
commit ebf278d3abf3fe74c5e52c8fd4ab5d84ef8aa9b2 (HEAD -> newBranch)
Author: Chase <23373253@buaa.edu.cn>
Date:   Wed Mar 12 14:42:26 2025 +0800

    1
```

这时可以通过 `git reset --hard <hash>` 去到未来

### 0.5

```bash
$ echo first
$ echo second > output.txt
$ echo third > output.txt
$ echo forth >> output.txt
```

`>` 是覆盖，`>>` 追加

### 0.6

`command`:

```bash
#!/bin/bash
echo "echo Shell Start..." > test
echo "echo set a = 1" >> test
echo "a=1" >> test
echo "echo set b = 2" >> test
echo "b=2" >> test
echo "echo set c = a+b" >> test
echo 'c=$[$a+$b]' >> test
echo 'echo c = $c' >> test
echo "echo save c to ./file1" >> test
echo 'echo $c>file1' >> test
echo "echo save b to ./file2" >> test
echo 'echo $b>file2' >> test
echo "echo save a to ./file3" >> test
echo 'echo $a>file3' >> test
echo "echo save file1 file2 file3 to file4" >> test
echo "cat file1>file4" >> test
i=2
while (($i < 4))
do
        echo "cat file$i>>file4" >> test 
        i=$((i+1))
done
echo "echo save file4 to ./result" >> test
echo "cat file4>>result" >> test
```

生成 `test` :

```bash
echo Shell Start...
echo set a = 1
a=1
echo set b = 2
b=2
echo set c = a+b
c=$[$a+$b]
echo c = $c
echo save c to ./file1
echo $c>file1
echo save b to ./file2
echo $b>file2
echo save a to ./file3
echo $a>file3
echo save file1 file2 file3 to file4
cat file1>file4
cat file2>>file4
cat file3>>file4
echo save file4 to ./result
cat file4>>result
```

运行 `test` 并输出到 `result` :

```bash
$ bash test > result
```

`result` 内容如下:

```bash
Shell Start...
set a = 1
set b = 2
set c = a+b
c = 3
save c to ./file1
save b to ./file2
save a to ./file3
save file1 file2 file3 to file4
save file4 to ./result
3
2
1
```

- `c=$[$a+$b]` : `$[]` 用于整数运算，`(())` 和 `let` 也有整数运算的功能

- ```bash
  echo echo Shell Start > test
  echo `echo Shell Start` >> test
  ```

  最终写入:

  ```bash
  echo Shell Start
  Shell Start
  ```

  第二条命令中使用 \` 进行了**命令替换**，会先执行 `echo Shell Start` ，结果为 `Shell Start` ，再将其作为参数传递给外层的 `echo` ，`$()` 也有此功能

- 单引号 `'$1'` 会解析为字符串 `$1`，只有在 `""` 内才会解析为参数

## 难点分析

- 命令本身是第 `0` 个参数

- 控制语句的写法，例如:

  ```bash
  while [ $a -le 100 ]
  do
          if [ $a -gt 70 ]           #if loop variable is greater than 70
          then
          rm -r file$a
          elif [ $a -gt 40 ]         # else if loop variable is great than 40
          then
          mv file$a newfile$a
          fi
          a=$((a+1))                 #don't forget change the loop variable
  done
  ```

- `make -C ./code` 中的 `-C` 参数指定了Makefile文件所在目录，即 `./code` 下

- `-I` : 当使用 `#include "xxx.h"` 引入头文件时，GCC 会在该路径下查找，例如:

  ```bash
  fibo.o: 
  	gcc -c fibo.c -I ./../include -o fibo.o
  ```

- ```assembly
  #include <asm/asm.h>
  
  .text
  EXPORT(_start)		# 提示程序的入口点
  .set at				# 设置$1
  .set reorder		# 启动指令重排
  
  	mtc0    zero, CP0_STATUS
  	li      sp, 0x80400000
  	jal     main
  	j       m_halt
  ```

## 实验体会

之前断断续续地了解过命令行的知识，Lab 0 让我更深入且系统地认识命令行。此外，教程还介绍了很多工具，在刚接触的时候觉得很难记很麻烦，但是慢慢会发现很好用，比如Vim。希望今后也能不断尝试探索！还有多运用这些工具！
