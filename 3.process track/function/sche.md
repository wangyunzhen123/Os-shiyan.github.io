include\linux\sched.h







```c
/*
 * 'schedule()'是调度函数。这是个很好的代码！没有任何理由对它进行修改，因为它可以在所有的
 * 环境下工作（比如能够对IO-边界处理很好的响应等）。只有一件事值得留意，那就是这里的信号
 * 处理代码。
 * 注意！！任务0 是个闲置('idle')任务，只有当没有其它任务可以运行时才调用它。它不能被杀
 * 死，也不能睡眠。任务0 中的状态信息'state'是从来不用的。
 */
void schedule (void)
{
	int i, next, c;
	struct task_struct **p;	// 任务结构指针的指针。

/* 检测alarm（进程的报警定时值），唤醒任何已得到信号的可中断任务 */

// 从任务数组中最后一个任务开始检测alarm。
	for (p = &LAST_TASK; p > &FIRST_TASK; --p)
		if (*p)
		{
// 如果任务的alarm 时间已经过期(alarm<jiffies),则在信号位图中置SIGALRM 信号，然后清alarm。
// jiffies 是系统从开机开始算起的滴答数（10ms/滴答）。定义在sched.h 第139 行。
			if ((*p)->alarm && (*p)->alarm < jiffies)
			{
				(*p)->signal |= (1 << (SIGALRM - 1));
				(*p)->alarm = 0;
			}
// 如果信号位图中除被阻塞的信号外还有其它信号，并且任务处于可中断状态，则置任务为就绪状态。
// 其中'~(_BLOCKABLE & (*p)->blocked)'用于忽略被阻塞的信号，但SIGKILL 和SIGSTOP 不能被阻塞。
			if (((*p)->signal & ~(_BLOCKABLE & (*p)->blocked)) &&
					(*p)->state == TASK_INTERRUPTIBLE)
				(*p)->state = TASK_RUNNING;	//置为就绪（可执行）状态。
		}

  /* 这里是调度程序的主要部分 */

	while (1)
	{
		c = -1;
		next = 0;
		i = NR_TASKS;
		p = &task[NR_TASKS];
// 这段代码也是从任务数组的最后一个任务开始循环处理，并跳过不含任务的数组槽。比较每个就绪
// 状态任务的counter（任务运行时间的递减滴答计数）值，哪一个值大，运行时间还不长，next 就
// 指向哪个的任务号。
		while (--i)
		{
			if (!*--p)
				continue;
			if ((*p)->state == TASK_RUNNING && (*p)->counter > c)
				c = (*p)->counter, next = i;
		}
      // 如果比较得出有counter 值大于0 的结果，则退出124 行开始的循环，执行任务切换（141 行）。
		if (c)
			break;
      // 否则就根据每个任务的优先权值，更新每一个任务的counter 值，然后回到125 行重新比较。
      // counter 值的计算方式为counter = counter /2 + priority。[右边counter=0??]
		for (p = &LAST_TASK; p > &FIRST_TASK; --p)
			if (*p)
				(*p)->counter = ((*p)->counter >> 1) + (*p)->priority;
	}
	switch_to (next);		// 切换到任务号为next 的任务，并运行之。
}
```



上下文切换switch_to(n)宏展开

```assembly
/* switch _ to ( n ）将切换当前任务到任务 nr ，即 n 。首先检测任务 n 不是当前任务，*如果是则什么也不做退出。如果我们切换到的任务最近（上次运行）使用过数学协处理器的话，则还需复位控制寄存器cr0中的 TS 标志。
跳转到一个任务的 TSS 段选择符组成的地址处会造成 CPU 进行任务切换操作
输入：
%0-指向_tmp; 
%1﹣指向＿ tmp.b 处，用于存放新 TSS 的选择符；
dx ﹣新任务 n 的 TSS 段选择符；
ecx ﹣新任务 n 的任务结构指针 task [ n ]

其中临时数据结构 tmp 用于组建1远跳转（ far jump ）指令的操作数。该操作数由4字节偏移
地址和2字节的段选择符组成。因此 tmp 中 a 的值是32位偏移值，而 b 的低2字节是新 TSS 段的
选择符（高2字节不用）。跳转到 TSS 段选择符会造成任务切换到该 TSS 对应的进程。对于造成任务
切换的长跳转， a 值无用。177行上的内存间接跳转指令使用6字节操作数作为跳转目的地的长指针，
其格式为： jmp 16位段选择符：32位偏移值。但在内存中操作数的表示顺序与这里正好相反。
*/

#define switch_to(n) {\
struct {long a,b;} __tmp; \
__asm__("cmpl %%ecx,current\n\t" \  #任务n是当前任务吗？（current == task[n]?）
	"je 1f\n\t" \   #是，则什么都不做，退出
	"movw %%dx,%1\n\t" \   #将新任务TSS的16位选择符存入_tmp.b中
	"xchgl %%ecx,current\n\t" \  #current = task[n];ecx = 被切换的任务
	"ljmp *%0\n\t" \  #执行长跳转到*&tmp,造成任务切换
	                  #切换回来后才会继续执行下面的语句
	"cmpl %%ecx,last_task_used_math\n\t" \  #原任务上次使用过协处理器吗？   
	"jne 1f\n\t" \                    
	"clts\n" \
	"1:" \
	::"m" (*&__tmp.a),"m" (*&__tmp.b), \
	"d" (_TSS(n)),"c" ((long) task[n])); \
}
```

![image-20220601213934093](C:\Users\dell\AppData\Roaming\Typora\typora-user-images\image-20220601213934093.png)