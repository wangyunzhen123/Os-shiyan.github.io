# 实验需要掌握的知识点：

如何使用用户函数调用系统函数、_syscall宏的含义以及作用、管理使用linux系统的基本知识

实验过程需要用到知识：内嵌汇编，makefile，内存特权





# 实验中需要添加和修改的文件：

（1）添加了：
who.c(路径：oslab/linux-0.11/kernel)

test.c(路径：oslab/hdc/usr/root)

(2)修改了

unistd.h(路径：oslab/hdc/include/unistd.h)
system_call.s(路径：oslab/linux-0.11/kernel/system_call.s)
sys.h(路径：oslab/linux-0.11/include/linux/sys.h)
Makefile(路径：oslab/linux-0.11/kernel/Makefile)



# 实验内容：



## 1.在内核的include/unistd.h添加系统调用号

## 

先看看一个系统调用的过程，以close为例子，看看为什么要加系统调用号



```c
// lib/close.c,close()的API

#define __LIBRARY__
#include <unistd.h>
_syscall1(int, close, int, fd)
    
//其中 _syscall1 是一个宏，在 include/unistd.h 中定义
#define _syscall1(type,name,atype,a) \        
type name(atype a) \            //type、name、atpe、a宏展开后分别对应int, close,int,fd
{ \
long __res; \
__asm__ volatile ("int $0x80" \  //调用系统中断0x80,后面的输入是参数
    : "=a" (__res) \             //返回值->eax(_res)
    : "0" (__NR_##name),"b" ((long)(a))); \  //输入为系统调用号__NR_name(宏)
if (__res >= 0) \      //如果返回值>=0，则直接返回该值
    return (type) __res; \
errno = -__res; \     //否则置出错号，并返回-1
return -1; \
}

//clos展开后
int close(int fd) {
   long __res;
   __asm_ volatitle("int $0x80"
          : "=a"(__res)
          : "0"(__NR_close), "b"(long)(fd)));
  if(_res >= 0)
      return (int) __res;
  errno = -__res
  return -1;
)
}


//这就是 API 的定义。它先将宏 __NR_close 存入 EAX，将参数 fd 存入 EBX，然后进行 0x80 中断调用。调用返回后，从 EAX 取出返回值，存入 __res，再通过对 __res 的判断决定传给 API 的调用者什么样的返回值。

//其中 __NR_close 就是系统调用的编号，在 include/unistd.h 中定义：

#define __NR_close    6
/*
所以添加系统调用时需要修改include/unistd.h文件，
使其包含__NR_whoami和__NR_iam。
*/
```

```c
/*
而在应用程序中，要有：
*/

/* 有它，_syscall1 等才有效。详见unistd.h */
#define __LIBRARY__

/* 有它，编译器才能获知自定义的系统调用的编号 */
#include "unistd.h"

/* iam()在用户空间的接口函数 */
_syscall1(int, iam, const char*, name);

/* whoami()在用户空间的接口函数 */
_syscall2(int, whoami,char*,name,unsigned int,size);

```



`/home/shiyanlou/oslab/hdc/include/unistd.h`中操作

```c
#define __NR_close    6
/*
所以添加系统调用时需要修改include/unistd.h文件，
使其包含__NR_whoami和__NR_iam。
*/
//我的添加:
#define __NR_whoami   72
#define __NR_iam      73


```



这一部分没有什么问题，比较顺利，注意在编写test.c时要加上_syscall1(int, iam, const char*, name)和_syscall2(int, whoami,char*,name,unsigned int,size)







## 2.在内核的include/unistd.h添加系统调用号



系统调用号时什么？

操作系统给用户的API最终会展开一个int 80，现在int 80发生了什么



先来看看操作初始过程，这个过程需要填写IDL（中断描述符表）

```c
/*int 0x80 触发后，接下来就是内核的中断处理了。先了解一下 0.11 处理 0x80 号中断的过程。

在内核初始化时，主函数（在 init/main.c 中，Linux 实验环境下是 main()，Windows 下因编译器兼容性问题被换名为 start()）调用了 sched_init() 初始化函数：
*/
void main(void)
{
//    ……
    time_init();
    sched_init();
    buffer_init(buffer_memory_end);
//    ……
}

//sched_init() 在 kernel/sched.c 中定义为
void sched_init(void)
{
//    ……
    set_system_gate(0x80,&system_call);
}
```



```c
//set_system_gate 是个宏，在 include/asm/system.h 中定义为：
#define set_system_gate(n,addr) \
    _set_gate(&idt[n],15,3,addr)
```



`_set_gate` 的定义是：

```c
//设置门描述符宏
//根据参数中的中断或异常处理过程地址addr、门描述符类型type和特权信息dpl，设置位于地址
//gate_addr处的门描述符（注意：下面“偏移”值是相对于内核代码或数据段来说）
//参数：gate_addr -描述符地址；type -描述符类型域值；dpl -描述符特权级; addr-偏移地址
//_set_gate 的定义是
#define _set_gate(gate_addr,type,dpl,addr) \
__asm__("movw %%dx,%%ax\n\t"  \     //将偏移地址低字与选择符组合成描述符低4字节（eax）
        "movw %0,%dx\n\t"              \      //将偏移地址低字与选择符组合成描述符高4字节（eax）
         "movl %eax,%1\n\t               \     //分别设置门描述符的低4字节和高4字节
         "movl %edx,%2"                   \    
         :                                           \ 
         : “i”((short) (0x8000+(dpl<<13)+type<<8))) \
           "o" (*((char *) (gate_addr))), \
           "o" (*(4+(char *) (gate_addr))), \
           "d" ((char *) (addr)),"a" (0x00080000))

```

这段代码实现的功能就是填写IDT（中断描述符表），将 `system_call` 函数地址写到 `0x80` 对应的中断描述符中（绑定中断向量和中断处理程序），也就是在中断 `0x80` 发生后，自动调用函数 `system_call`

填表的具体内容如下![image](https://api2.mubu.com/v3/document_image/fcc97c12-2588-4f11-a91b-a3429779167c-6803441.jpg)

要注意的地方：为了使当前执行程序能进入IDT表，描述符特权器DPL（Descriptor Privilege Level）应该设置为3（_set_gate中参数3的作用），用户当前特权级CPL（Current Privilege Level）也为3可以进入，用户进程调用“int”后cs被设置为8（ox100）,"a"(0x0008000)的作用，cs的后两个字节为特权级，此时特权级为0，就可以进入内核了





接下来看 `system_call`。该函数纯汇编打造，定义在 `kernel/system_call.s` 中：

```assembly

!……
! # 这是系统调用总数。如果增删了系统调用，必须做相应修改
nr_system_calls = 72
!……

.globl system_call
.align 2
system_call:

! # 检查系统调用编号是否在合法范围内
    cmpl \$nr_system_calls-1,%eax
    ja bad_sys_call
    push %ds
    push %es
    push %fs
    pushl %edx
    pushl %ecx

! # push %ebx,%ecx,%edx，是传递给系统调用的参数
    pushl %ebx

! # 让ds, es指向GDT，内核地址空间
    movl $0x10,%edx
    mov %dx,%ds
    mov %dx,%es
    movl $0x17,%edx
! # 让fs指向LDT，用户地址空间
    mov %dx,%fs
    call sys_call_table(,%eax,4)
    pushl %eax
    movl current,%eax
    cmpl $0,state(%eax)
    jne reschedule
    cmpl $0,counter(%eax)
    je reschedule
    
    
 /*
system_call 用 .globl 修饰为其他函数可见。

Windows 实验环境下会看到它有一个下划线前缀，这是不同版本编译器的特质决定的，没有实质区别。

call sys_call_table(,%eax,4) 之前是一些压栈保护，修改段选择子为内核段，call sys_call_table(,%eax,4) 之后是看看是否需要重新调度，这些都与本实验没有直接关系，此处只关心 call sys_call_table(,%eax,4) 这一句。

根据汇编寻址方法它实际上是：call sys_call_table + 4 * %eax，其中 eax 中放的是系统调用号，即 __NR_xxxxxx。*/
```



`sys_call_table` 一定是一个函数指针数组的起始地址，它定义在 `include/linux/sys.h` 中：

![image-20220531175023612](C:\Users\dell\AppData\Roaming\Typora\typora-user-images\image-20220531175023612.png)



增加实验要求的系统调用，需要在这个函数表中增加两个函数引用 ——`sys_iam` 和 `sys_whoami`。当然该函数在 `sys_call_table` 数组中的位置必须和 `__NR_xxxxxx` 的值对应上。

同时还要仿照此文件中前面各个系统调用的写法，加上：

```c
extern int sys_whoami();
extern int sys_iam();
```

![image-20220531175409146](C:\Users\dell\AppData\Roaming\Typora\typora-user-images\image-20220531175409146.png)





## 3.实现sys_iam()和sys_whoami()

自己创建一个文件：`kernel/who.c`

```c
#define __LIBRARY__
#include <asm/segment.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

char msg[24]; //23个字符 +'\0' = 24

int sys_iam(const char * name)
/***
function：将name的内容拷贝到msg,name的长度不超过23个字符
return：拷贝的字符数。如果name的字符个数超过了23,则返回“­-1”,并置errno为EINVAL。
****/
{
	int i;
	//临时存储 输入字符串 操作失败时不影响msg
	char tmp[30];
	for(i=0; i<30; i++)
	{
		//从用户态内存取得数据
		tmp[i] = get_fs_byte(name+i);
		if(tmp[i] == '\0') break;  //字符串结束
	}
	//printk(tmp);
	i=0;
	while(i<30&&tmp[i]!='\0') i++;
	int len = i;
	// int len = strlen(tmp);
	//字符长度大于23个
	if(len > 23)
	{
		// printk("String too long!\n");
		return -(EINVAL);  //置errno为EINVAL  返回“­-1”  具体见_syscalln宏展开
	}
	strcpy(msg,tmp);
	//printk(tmp);
	return i;
}

int sys_whoami(char* name, unsigned int size)
/***
function:将msg拷贝到name指向的用户地址空间中,确保不会对name越界访存(name的大小由size说明)
return: 拷贝的字符数。如果size小于需要的空间,则返回“­-1”,并置errno为EINVAL。
****/
{ 	
	//msg的长度大于 size
	int len = 0;
	for(;msg[len]!='\0';len++);
	if(len > size)
	{
		return -(EINVAL);
	}
	int i = 0;
	//把msg 输出至 name
	for(i=0; i<size; i++)
	{
		put_fs_byte(msg[i],name+i);
		if(msg[i] == '\0') break; //字符串结束
	}
	return i;
}
```

该该函数的编写最主要的地方就是要注意如何在内核态和用户态间传递数据，下面来看看原理：

指针参数传递的是应用程序所在地址空间的逻辑地址，在内核中如果直接访问这个地址，访问到的是内核空间中的数据，不会是用户空间的。所以这里还需要一点儿特殊工作，才能在内核中从用户空间得到数据。

首先看下open的调用过程作为参考：

```c
int open(const char * filename, int flag, ...)
{
//    ……
    __asm__("int $0x80"
            :"=a" (res)
            :"0" (__NR_open),"b" (filename),"c" (flag),
            "d" (va_arg(arg,int)));
//    ……
}
```



可以看出，系统调用是用 `eax、ebx、ecx、edx` 寄存器来传递参数的。

- 其中 eax 传递了系统调用号，而 ebx、ecx、edx 是用来传递函数的参数的
- ebx 对应第一个参数，ecx 对应第二个参数，依此类推。

如 open 所传递的文件名指针是由 ebx 传递的，也即进入内核后，通过 ebx 取出文件名字符串。open 的 ebx 指向的数据在用户空间，而当前执行的是内核空间的代码，如何在用户态和核心态之间传递数据？

接下来我们继续看看 open 的处理：

```c
system_call: //所有的系统调用都从system_call开始
!    ……
    pushl %edx
    pushl %ecx
    pushl %ebx                # push %ebx,%ecx,%edx，这是传递给系统调用的参数
    movl $0x10,%edx            # 让ds,es指向GDT，指向核心地址空间
    mov %dx,%ds
    mov %dx,%es
    movl $0x17,%edx            # 让fs指向的是LDT，指向用户地址空间
    mov %dx,%fs
    call sys_call_table(,%eax,4)    # 即call sys_open
```

```c
/*由上面的代码可以看出，获取用户地址空间（用户数据段）中的数据依靠的就是段寄存器 fs，下面该转到 sys_open 执行了，在 fs/open.c 文件中：*/

int sys_open(const char * filename,int flag,int mode)  //filename这些参数从哪里来？
/*是否记得上面的pushl %edx,    pushl %ecx,    pushl %ebx？
  实际上一个C语言函数调用另一个C语言函数时，编译时就是将要
  传递的参数压入栈中（第一个参数最后压，…），然后call …，
  所以汇编程序调用C函数时，需要自己编写这些参数压栈的代码…*/
{
    ……
    if ((i=open_namei(filename,flag,mode,&inode))<0) {
        ……
    }
    ……
}
```

它将参数传给了 `open_namei()`。

再沿着 `open_namei()` 继续查找，文件名先后又被传给`dir_namei()`、`get_dir()`。

在 `get_dir()` 中可以看到：

```c
static struct m_inode * get_dir(const char * pathname)
{
    ……
    if ((c=get_fs_byte(pathname))=='/') {
        ……
    }
    ……
}
```

处理方法就很显然了：用 `get_fs_byte()` 获得一个字节的用户空间中的数据。

所以，在实现 `iam()` 时，调用 `get_fs_byte()` 即可。

但如何实现 `whoami()` 呢？即如何实现从核心态拷贝数据到用心态内存空间中呢？

猜一猜，是否有 `put_fs_byte()`？有！看一看 `include/asm/segment.h` ：

他俩以及所有 `put_fs_xxx()` 和 `get_fs_xxx()` 都是用户空间和内核空间之间的桥梁

```c
extern inline unsigned char get_fs_byte(const char * addr)
{
    unsigned register char _v;
    __asm__ ("movb %%fs:%1,%0":"=r" (_v):"m" (*addr));
    return _v;
}
```

```c
extern inline void put_fs_byte(char val,char *addr)
{
    __asm__ ("movb %0,%%fs:%1"::"r" (val),"m" (*addr));
}
```





## 4.修改Makefile

要想让我们添加的 `kernel/who.c` 可以和其它 Linux 代码编译链接到一起，必须要修改 Makefile 文件。

Makefile 在代码树中有很多，分别负责不同模块的编译工作。我们要修改的是 `kernel/Makefile`。需要修改两处

第一处：

```c
OBJS  = sched.o system_call.o traps.o asm.o fork.o \
        panic.o printk.o vsprintf.o sys.o exit.o \
        signal.o mktime.o
```

改为：

```c
OBJS  = sched.o system_call.o traps.o asm.o fork.o \
        panic.o printk.o vsprintf.o sys.o exit.o \
        signal.o mktime.o who.o

//添加了 who.o。
```





第二处：

```c
### Dependencies:
exit.s exit.o: exit.c ../include/errno.h ../include/signal.h \
  ../include/sys/types.h ../include/sys/wait.h ../include/linux/sched.h \
  ../include/linux/head.h ../include/linux/fs.h ../include/linux/mm.h \
  ../include/linux/kernel.h ../include/linux/tty.h ../include/termios.h \
  ../include/asm/segment.h

```

改为：

```c
### Dependencies:
who.s who.o: who.c ../include/linux/kernel.h ../include/unistd.h
exit.s exit.o: exit.c ../include/errno.h ../include/signal.h \
  ../include/sys/types.h ../include/sys/wait.h ../include/linux/sched.h \
  ../include/linux/head.h ../include/linux/fs.h ../include/linux/mm.h \
  ../include/linux/kernel.h ../include/linux/tty.h ../include/termios.h \
  ../include/asm/segment.h
    
    
//添加了 who.s who.o: who.c ../include/linux/kernel.h ../include/unistd.h。
```

改了Makefile后，在终端中进入`oslab/linux0.11`，输入`make all`，进行编译。正确的编译最后一行内容为`sync`。重新Makefil，先make clean再make all

![image-20220531231133361](C:\Users\dell\AppData\Roaming\Typora\typora-user-images\image-20220531231133361.png)



这块不太熟悉，还有待学习，当时实验时也是这里卡了半天，一直出现错误，make: *** 没有规则可以创建“who.o”需要的目标，最后重新配置了环境完成，错误原因可能是who.c有问题





5.编写test.c

```c
#include<errno.h>
#define __LIBRARY__
#include<unistd.h>
#include<stdio.h>

_syscall1(int, iam, const char*, name);
_syscall2(int, whoami, char*, name, unsigned int, size);

int main(int argc, char ** argv) {
    char s[30];
    iam(argv[1]);
    whoami(s,30);
    printf("%s\n",s);
    return 0;
}
```

![image-20220531231038138](C:\Users\dell\AppData\Roaming\Typora\typora-user-images\image-20220531231038138.png)



# 参考资源

Linux内核0.11完全注释_v3.0(赵炯)

[(8条消息) 哈工大-操作系统-HitOSlab-李治军-实验2-系统调用_garbage_man的博客-CSDN博客](https://blog.csdn.net/qq_42518941/article/details/119037501)

[操作系统原理与实践 - 系统调用 - 蓝桥云课 (lanqiao.cn)](https://www.lanqiao.cn/courses/115/learning/?id=569)

[Linux操作系统（哈工大李治军老师)实验楼实验2-系统调用(2)_哔哩哔哩_bilibili](https://www.bilibili.com/video/BV16L411n7pa?spm_id_from=333.999.0.0)

[操作系统（哈工大李治军老师）32讲（全）超清_哔哩哔哩_bilibili](https://www.bilibili.com/video/BV1d4411v7u7?p=5&spm_id_from=pageDriver)

[Wangzhike/HIT-Linux-0.11: 网易云课堂选的操作系统课实验的代码及相关记录 (github.com)](https://github.com/Wangzhike/HIT-Linux-0.11)





# 总结

系统调用的分析
**1.初始化：**

![在这里插入图片描述](https://img-blog.csdnimg.cn/3d28801085114958a46671f87b9be9bc.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzQyNTE4OTQx,size_16,color_FFFFFF,t_70#pic_center)

**2.调用流程（以iam()函数为例）：**

![在这里插入图片描述](https://img-blog.csdnimg.cn/34631571b09a4aea806f40bf8a04dd00.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzQyNTE4OTQx,size_16,color_FFFFFF,t_70#pic_center)