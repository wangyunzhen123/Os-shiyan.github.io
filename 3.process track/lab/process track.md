# 在 `Linux0.11` 上实现进程运行轨迹的跟踪



## log文件和写log文件

操作系统启动后先要打开 `/var/process.log`，然后在每个进程发生状态切换的时候向 log 文件内写入一条记录，其过程和用户态的应用程序没什么两样。然而，因为内核状态的存在，使过程中的很多细节变得完全不一样。

打开 log 文件

为了能尽早开始记录，应当在内核启动时就打开 log 文件。内核的入口是 `init/main.c` 中的 `main()`（Windows 环境下是 `start()`），其中一段代码是：

```c
//……
move_to_user_mode();
if (!fork()) {        /* we count on this going ok */
    init();
}
//……
```

这段代码在进程 0 中运行，先切换到用户模式，然后全系统第一次调用 `fork()` 建立进程 1。进程 1 调用 `init()`。

在 init()中：

```c
// ……
//加载文件系统
setup((void *) &drive_info);

// 打开/dev/tty0，建立文件描述符0和/dev/tty0的关联
(void) open("/dev/tty0",O_RDWR,0);

// 让文件描述符1也和/dev/tty0关联
(void) dup(0);

// 让文件描述符2也和/dev/tty0关联
(void) dup(0);

// ……
```

这段代码建立了文件描述符 0、1 和 2，它们分别就是 stdin、stdout 和 stderr。这三者的值是系统标准（Windows 也是如此），不可改变。

可以把 log 文件的描述符关联到 3。文件系统初始化，描述符 0、1 和 2 关联之后，才能打开 log 文件，开始记录进程的运行轨迹。

为了能尽早访问 log 文件，我们要让上述工作在进程 0 中就完成。所以把这一段代码从 `init()` 移动到 `main()` 中，放在 `move_to_user_mode()` 之后（不能再靠前了），同时加上打开 log 文件的代码。

修改后的 main() 如下：

```c
//……
move_to_user_mode();

/***************添加开始***************/
setup((void *) &drive_info);

// 建立文件描述符0和/dev/tty0的关联
(void) open("/dev/tty0",O_RDWR,0);

//文件描述符1也和/dev/tty0关联
(void) dup(0);

// 文件描述符2也和/dev/tty0关联
(void) dup(0);

(void) open("/var/process.log",O_CREAT|O_TRUNC|O_WRONLY,0666);

/***************添加结束***************/

if (!fork()) {        /* we count on this going ok */
    init();
}
//……
```

打开 log 文件的参数的含义是建立只写文件，如果文件已存在则清空已有内容。文件的权限是所有人可读可写。

这样，文件描述符 0、1、2 和 3 就在进程 0 中建立了。根据 `fork()` 的原理，进程 1 会继承这些文件描述符，所以 `init()` 中就不必再 `open()` 它们。此后所有新建的进程都是进程 1 的子孙，也会继承它们。但实际上，`init()` 的后续代码和 `/bin/sh` 都会重新初始化它们。所以只有进程 0 和进程 1 的文件描述符肯定关联着 log 文件，这一点在接下来的写 log 中很重要。









##  寻找状态切换点

关键是根据状态转换函数找出切换点



![image-20220602155329122](C:\Users\dell\AppData\Roaming\Typora\typora-user-images\image-20220602155329122.png)



### 1.**记录一个进程生命期的开始**

第一个例子是看看如何记录一个进程生命期的开始，当然这个事件就是进程的创建函数 `fork()`，由《系统调用》实验可知，`fork()` 功能在内核中实现为 `sys_fork()`，该“函数”在文件 `kernel/system_call.s` 中实现为：

```assembly
sys_fork:
    call find_empty_process
!    ……
! 传递一些参数
    push %gs
    pushl %esi
    pushl %edi
    pushl %ebp
    pushl %eax
! 调用 copy_process 实现进程创建
    call copy_process 
    addl $20,%esp

```

所以真正实现进程创建的函数是 `copy_process()`，它在 `kernel/fork.c` 中定义为：因此要完成进程运行轨迹的记录要copy_process()` 中添加输出语句。这里要输出两种状态，分别是“N（新建）”和“J（就绪）”。

```c
int copy_process(int nr,……)
{
    struct task_struct *p;
//    ……
// 获得一个 task_struct 结构体空间
    p = (struct task_struct *) get_free_page();  
//    ……
    p->pid = last_pid;
//    ……
// 设置 start_time 为 jiffies
    p->start_time = jiffies;
//  @yyf change
    fprintk(3, "%ld\t%c\t%ld\n", p->pid, 'N', jiffies); 
//       ……
/* 设置进程状态为就绪。所有就绪进程的状态都是
   TASK_RUNNING(0），被全局变量 current 指向的
   是正在运行的进程。*/
    p->state = TASK_RUNNING;    
//  @yyf change
    fprintk(3, "%ld\t%c\t%ld\n", p->pid, 'J', jiffies); 
    
    return last_pid;
}

```



### 2.**记录进入睡眠态的时间**

记录进入睡眠态的时间。sleep_on() 和 interruptible_sleep_on() 让当前进程进入睡眠状态，这两个函数在 `kernel/sched.c`文件中定义如下：

```c
void sleep_on(struct task_struct **p)
{
    struct task_struct *tmp;
//    ……
    tmp = *p;
// 仔细阅读，实际上是将 current 插入“等待队列”头部，tmp 是原来的头部
    *p = current;  
// 切换到睡眠态
    current->state = TASK_UNINTERRUPTIBLE; 
//  @yyf change
    fprintk(3, "%ld\t%c\t%ld\n", current->pid, 'W', jiffies); 
// 让出 CPU
    schedule();  
// 唤醒队列中的上一个（tmp）睡眠进程。0 换作 TASK_RUNNING 更好
// 在记录进程被唤醒时一定要考虑到这种情况，实验者一定要注意!!!
    if (tmp){
        tmp->state=0;
    	//  @yyf change
    	fprintk(3, "%ld\t%c\t%ld\n", tmp->pid, 'J', jiffies); }
}
/* TASK_UNINTERRUPTIBLE和TASK_INTERRUPTIBLE的区别在于不可中断的睡眠
 * 只能由wake_up()显式唤醒，再由上面的 schedule()语句后的
 *
 *   if (tmp) tmp->state=0;
 *
 * 依次唤醒，所以不可中断的睡眠进程一定是按严格从“队列”（一个依靠
 * 放在进程内核栈中的指针变量tmp维护的队列）的首部进行唤醒。而对于可
 * 中断的进程，除了用wake_up唤醒以外，也可以用信号（给进程发送一个信
 * 号，实际上就是将进程PCB中维护的一个向量的某一位置位，进程需要在合
 * 适的时候处理这一位。感兴趣的实验者可以阅读有关代码）来唤醒，如在
 * schedule()中：
 *
 *  for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
 *      if (((*p)->signal & ~(_BLOCKABLE & (*p)->blocked)) &&
 *         (*p)->state==TASK_INTERRUPTIBLE)
 *         (*p)->state=TASK_RUNNING;//唤醒
 *
 * 就是当进程是可中断睡眠时，如果遇到一些信号就将其唤醒。这样的唤醒会
 * 出现一个问题，那就是可能会唤醒等待队列中间的某个进程，此时这个链就
 * 需要进行适当调整。interruptible_sleep_on和sleep_on函数的主要区别就
 * 在这里。
 */
void interruptible_sleep_on(struct task_struct **p)
{
    struct task_struct *tmp;
       …
    tmp=*p;
    *p=current;
repeat:    current->state = TASK_INTERRUPTIBLE;
    	   //  @yyf change
    		/*0号进程是守护进程，cpu空闲的时候一直在waiting，输出它的话是不会通过脚本检查的哦*/
    		if(current->pid != 0)
    		fprintk(3, "%ld\t%c\t%ld\n", current->pid, 'W', jiffies); 
    schedule();
// 如果队列头进程和刚唤醒的进程 current 不是一个，
// 说明从队列中间唤醒了一个进程，需要处理
    if (*p && *p != current) { 
 // 将队列头唤醒，并通过 goto repeat 让自己再去睡眠
        (**p).state=0;
        //  @yyf change
    	fprintk(3, "%ld\t%c\t%ld\n", (*p)->pid, 'J', jiffies); 
        goto repeat;
    }
    *p=NULL;
//作用和 sleep_on 函数中的一样
    if (tmp){
        tmp->state=0;
    	//  @yyf change
    	fprintk(3, "%ld\t%c\t%ld\n", tmp->pid, 'J', jiffies); }
}

int sys_pause(void)
{
	current->state = TASK_INTERRUPTIBLE;
    //  @yyf change
    if(current->pid != 0)
    	fprintk(3, "%ld\t%c\t%ld\n", current->pid, 'W', jiffies);
	schedule();
	return 0;
}
//exit.c中
int sys_waitpid(pid_t pid,unsigned long * stat_addr, int options)
{
	int flag, code;
	struct task_struct ** p;

	verify_area(stat_addr,4);
repeat:
	flag=0;
	for(p = &LAST_TASK ; p > &FIRST_TASK ; --p) {
		if (!*p || *p == current)
			continue;
		if ((*p)->father != current->pid)
			continue;
		if (pid>0) {
			if ((*p)->pid != pid)
				continue;
		} else if (!pid) {
			if ((*p)->pgrp != current->pgrp)
				continue;
		} else if (pid != -1) {
			if ((*p)->pgrp != -pid)
				continue;
		}
		switch ((*p)->state) {
			case TASK_STOPPED:
				if (!(options & WUNTRACED))
					continue;
				put_fs_long(0x7f,stat_addr);
				return (*p)->pid;
			case TASK_ZOMBIE:
				current->cutime += (*p)->utime;
				current->cstime += (*p)->stime;
				flag = (*p)->pid;
				code = (*p)->exit_code;
				release(*p);
				put_fs_long(code,stat_addr);
				return flag;
			default:
				flag=1;
				continue;
		}
	}
	if (flag) {
		if (options & WNOHANG)
			return 0;
		current->state=TASK_INTERRUPTIBLE;
        //  @yyf change
        /*0号进程是守护进程，cpu空闲的时候一直在waiting，输出它的话是不会通过脚本检查的哦*/
        if(current->pid != 0)
        	fprintk(3, "%ld\t%c\t%ld\n", current->pid, 'W', jiffies); 
		schedule();
		if (!(current->signal &= ~(1<<(SIGCHLD-1))))
			goto repeat;
		else
			return -EINTR;
	}
	return -ECHILD;
}
```



### 3.调度算法

调度程序：为所有处于TASK_RUNNING状态的进程分配CPU运行时间的管理代码。在 Linux 0.11 中采用了基于优先级排队的调度策略。

TASK_RUNNING不只是运行，还有就绪状态。

Linux 进程虽然是抢占式的，但被抢占的进程仍然处于TASK_RUNNING 状态，只是暂时没有被 CPU 运行。
算法细节

基于优先级的排队策略。通过比较每个TASK_RUNNING任务的 运行时间 递减滴答计数 counter 的值来确定当前哪个进程运行的时间最少。 若counter=0，重新计算所有进程(包括睡眠)，counter = counter / 2 + priority。
根据上述原则选择出一个进程，最后调用 switch_to()执行实际的进程切换操作。
schedule() 找到的 next 进程是接下来要运行的进程（注意，一定要分析清楚 next 是什么）。如果 next 恰好是当前正处于运行态的进程，switch_to(next) 也会被调用。这种情况下相当于当前进程的状态没变。

如果不一样，则将当前进程状态置为就绪，而next进程状态置为运行。


```c
//后文有schedule算法的详细代码，这里仅仅说一下改动了什么
/* check alarm, wake up any interruptible tasks that have got a signal */
for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
		if (*p) {
			if ((*p)->alarm && (*p)->alarm < jiffies) {
					(*p)->signal |= (1<<(SIGALRM-1));
					(*p)->alarm = 0;
            }
			if (((*p)->signal & ~(_BLOCKABLE & (*p)->blocked)) &&
			(*p)->state==TASK_INTERRUPTIBLE){
				(*p)->state=TASK_RUNNING;
            	fprintk(3, "%ld\t%c\t%ld\n", (*p)->pid, 'J', jiffies); 
            }	
		}

while (1) {
    c = -1; next = 0; i = NR_TASKS; p = &task[NR_TASKS];

// 找到 counter 值最大的就绪态进程
    while (--i) {
        if (!*--p)    continue;
        if ((*p)->state == TASK_RUNNING && (*p)->counter > c)
            c = (*p)->counter, next = i;
    }       

// 如果有 counter 值大于 0 的就绪态进程，则退出
    if (c) break;  

// 如果没有：
// 所有进程的 counter 值除以 2 衰减后再和 priority 值相加，
// 产生新的时间片
    for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
          if (*p) (*p)->counter = ((*p)->counter >> 1) + (*p)->priority;  
}
//@yyf change  切换到相同的进程不输出
if(current != task[next]){
    if(current->state == TASK_RUNNING){
        fprintk(3, "%ld\t%c\t%ld\n", current->pid, 'J', jiffies); 
    }
    fprintk(3, "%ld\t%c\t%ld\n", task[next]->pid, 'R', jiffies); 
}
// 切换到 next 进程
switch_to(next);  

```



### 4.**睡眠到就绪**

```c
void wake_up(struct task_struct **p)
{
	if (p && *p) {
        if((**p).state != TASK_RUNNING){  
            (**p).state=TASK_RUNNING;
            //  @yyf change
            fprintk(3, "%ld\t%c\t%ld\n", (*p)->pid, 'J', jiffies); 
        }
    }
}

```



### 5.进程退出

```c
//改动exit.c
int do_exit(long code)
{
	int i;
	free_page_tables(get_base(current->ldt[1]),get_limit(0x0f));
	free_page_tables(get_base(current->ldt[2]),get_limit(0x17));
	for (i=0 ; i<NR_TASKS ; i++)
		if (task[i] && task[i]->father == current->pid) {
			task[i]->father = 1;
			if (task[i]->state == TASK_ZOMBIE)
				/* assumption task[1] is always init */
				(void) send_sig(SIGCHLD, task[1], 1);
		}
	for (i=0 ; i<NR_OPEN ; i++)
		if (current->filp[i])
			sys_close(i);
	iput(current->pwd);
	current->pwd=NULL;
	iput(current->root);
	current->root=NULL;
	iput(current->executable);
	current->executable=NULL;
	if (current->leader && current->tty >= 0)
		tty_table[current->tty].pgrp = 0;
	if (last_task_used_math == current)
		last_task_used_math = NULL;
	if (current->leader)
		kill_session();
	current->state = TASK_ZOMBIE;
    //  @yyf change
    fprintk(3, "%ld\t%c\t%ld\n", current->pid, 'E', jiffies); 
	current->exit_code = code;
	tell_father(current->father);
	schedule();
	return (-1);	/* just to suppress warnings */
}

int sys_exit(int error_code)
{
	return do_exit((error_code&0xff)<<8);
}

```







总的来说，Linux 0.11 支持四种进程状态的转移：就绪到运行、运行到就绪、运行到睡眠和睡眠到就绪，此外还有新建和退出两种情况。其中就绪与运行间的状态转移是通过 schedule()（它亦是调度算法所在）完成的；运行到睡眠依靠的是 sleep_on() 和 interruptible_sleep_on()，还有进程主动睡觉的系统调用 sys_pause() 和 sys_waitpid()；睡眠到就绪的转移依靠的是 wake_up()。所以只要在这些函数的适当位置插入适当的处理语句就能完成进程运行轨迹的全面跟踪了。




- **为了让生成的 log 文件更精准，以下几点请注意：**
- 进程退出的最后一步是通知父进程自己的退出，目的是唤醒正在等待此事件的父进程。从时序上来说，应该是子进程先退出，父进程才醒来。



- `schedule()` 找到的 next 进程是接下来要运行的进程（注意，一定要分析清楚 next 是什么）。如果 next 恰好是当前正处于运行态的进程，`swith_to(next)` 也会被调用。这种情况下相当于当前进程的状态没变。

```c
//@yyf change  切换到相同的进程不输出
if(current != task[next]){
    if(current->state == TASK_RUNNING){
        fprintk(3, "%ld\t%c\t%ld\n", current->pid, 'J', jiffies); 
    }
    fprintk(3, "%ld\t%c\t%ld\n", task[next]->pid, 'R', jiffies); 
}
// 切换到 next 进程
switch_to(next);  
```



- 系统无事可做的时候，进程 0 会不停地调用 `sys_pause()`，以激活调度算法。此时它的状态可以是等待态，等待有其它可运行的进程；也可以叫运行态，因为它是唯一一个在 CPU 上运行的进程，只不过运行的效果是等待。

```c
   /*0号进程是守护进程，cpu空闲的时候一直在waiting，输出它的话是不会通过脚本检查的哦*/
        if(current->pid != 0)
        	fprintk(3, "%ld\t%c\t%ld\n", current->pid, 'W', jiffies); 
```







注意上面的三点，也是实验最容易出错的地方，不会通过脚本







最后的process.log文件

![image-20220602161611347](C:\Users\dell\AppData\Roaming\Typora\typora-user-images\image-20220602161611347.png)



















时间片1：

![image-20220602145000022](C:\Users\dell\AppData\Roaming\Typora\typora-user-images\image-20220602145000022.png)





时间片15：

![image-20220602141805318](C:\Users\dell\AppData\Roaming\Typora\typora-user-images\image-20220602141805318.png)



时间片50：

![image-20220602145342364](C:\Users\dell\AppData\Roaming\Typora\typora-user-images\image-20220602145342364.png)



时间片100：

![image-20220602151857446](C:\Users\dell\AppData\Roaming\Typora\typora-user-images\image-20220602151857446.png)

时间片150：

![image-20220602151501494](C:\Users\dell\AppData\Roaming\Typora\typora-user-images\image-20220602151501494.png)

时间片200：

![image-20220602151207571](C:\Users\dell\AppData\Roaming\Typora\typora-user-images\image-20220602151207571.png)













## 参考资料

[sunym1993/flash-linux0.11-talk: 你管这破玩意叫操作系统源码 — 像小说一样品读 Linux 0.11 核心代码 (github.com)](https://github.com/sunym1993/flash-linux0.11-talk)

[(8条消息) linux-0.11中进程睡眠函数sleep_on()解析_wu5795175的博客-CSDN博客](https://blog.csdn.net/wu5795175/article/details/9156535)

[(8条消息) 哈工大-操作系统-HitOSlab-李治军-实验3-进程运行的轨迹跟踪与统计_garbage_man的博客-CSDN博客_哈工大操作系统实验3](https://blog.csdn.net/qq_42518941/article/details/119061014)

[(8条消息) (浓缩+精华)哈工大-操作系统-MOOC-李治军教授-实验3-进程运行轨迹的跟踪与统计_a634238158的博客-CSDN博客](https://blog.csdn.net/a634238158/article/details/100081197?spm=1001.2014.3001.5501)