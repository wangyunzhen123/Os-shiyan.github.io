linux/kernel/systemcall.s

时钟中断过程



sched_init 的时候，开启了定时器这个定时器每隔一段时间就会向 CPU 发起一个中断信号。



```c
//sched_init 的时候，开启了定时器这个定时器每隔一段时间就会向 CPU 发起一个中断信号。这个间隔时间被设置为 10 ms，也就是 100 Hz。
schedule.c
#define HZ 100
    
//当时钟中断，也就是 0x20 号中断来临时，CPU 会查找中断向量表中 0x20 处的函数地址，即中断处理函数，并跳转过去执行    
schedule.c
set_intr_gate(0x20, &timer_interrupt);
```





systemcall.s/_timer_interrupt

```assembly
_timer_interrupt:
	push ds ;// save ds,es and put kernel data space
	push es ;// into them. %fs is used by _system_call
	push fs
	push edx ;// we save %eax,%ecx,%edx as gcc doesn't
	push ecx ;// save those across function calls. %ebx
	push ebx ;// is saved as we use that in ret_sys_call
	push eax
	mov eax,10h ;// ds,es 置为指向内核数据段。
	mov ds,ax
	mov es,ax
	mov eax,17h ;// fs 置为指向局部数据段（出错程序的数据段）。
	mov fs,ax
	inc dword ptr _jiffies
;// 由于初始化中断控制芯片时没有采用自动EOI，所以这里需要发指令结束该硬件中断。
	mov al,20h ;// EOI to interrupt controller ;//1
	out 20h,al ;// 操作命令字OCW2 送0x20 端口。
;// 下面3 句从选择符中取出当前特权级别(0 或3)并压入堆栈，作为do_timer 的参数。
	mov eax,dword ptr [R_CS+esp]
	and eax,3 ;// %eax is CPL (0 or 3, 0=supervisor)
	push eax
;// do_timer(CPL)执行任务切换、计时等工作，在kernel/shched.c,305 行实现。
	call _do_timer ;// 'do_timer(long CPL)' does everything from
	add esp,4 ;// task switching to accounting ...
	jmp ret_from_sys_call
```

主要内容：

```assembly
_timer_interrupt:
    ...
    // 增加系统滴答数
    incl _jiffies
    ...
    // 调用函数 do_timer
    call _do_timer
    ...
    
/*
  这个函数做了两件事，一个是将系统滴答数这个变量 jiffies 加一，一个是调用了另一个函数 do_timer。
*/
```





sched.c/do_timer

```c
//// 时钟中断C 函数处理程序，在kernel/system_call.s 中的_timer_interrupt（176 行）被调用。
// 参数cpl 是当前特权级0 或3，0 表示内核代码在执行。
// 对于一个进程由于执行时间片用完时，则进行任务切换。并执行一个计时更新工作。
void do_timer (long cpl)
{
	extern int beepcount;		// 扬声器发声时间滴答数(kernel/chr_drv/console.c,697)
	extern void sysbeepstop (void);	// 关闭扬声器(kernel/chr_drv/console.c,691)

  // 如果发声计数次数到，则关闭发声。(向0x61 口发送命令，复位位0 和1。位0 控制8253
  // 计数器2 的工作，位1 控制扬声器)。
	if (beepcount)
		if (!--beepcount)
			sysbeepstop ();

  // 如果当前特权级(cpl)为0（最高，表示是内核程序在工作），则将超级用户运行时间stime 递增；
  // 如果cpl > 0，则表示是一般用户程序在工作，增加utime。
	if (cpl)
		current->utime++;
	else
		current->stime++;

// 如果有用户的定时器存在，则将链表第1 个定时器的值减1。如果已等于0，则调用相应的处理
// 程序，并将该处理程序指针置为空。然后去掉该项定时器。
	if (next_timer)
	{				// next_timer 是定时器链表的头指针(见270 行)。
		next_timer->jiffies--;
		while (next_timer && next_timer->jiffies <= 0)
		{
			void (*fn) ();	// 这里插入了一个函数指针定义！！！??

			fn = next_timer->fn;
			next_timer->fn = NULL;
			next_timer = next_timer->next;
			(fn) ();		// 调用处理函数。
		}
	}
// 如果当前软盘控制器FDC 的数字输出寄存器中马达启动位有置位的，则执行软盘定时程序(245 行)。
	if (current_DOR & 0xf0)
		do_floppy_timer ();
	if ((--current->counter) > 0)
		return;			// 如果进程运行时间还没完，则退出。
	current->counter = 0;
	if (!cpl)
		return;			// 对于超级用户程序，不依赖counter 值进行调度。
	schedule ();
}
```

重要部分：

```c
/* 
  
首先将当先进程的时间片 -1，然后判断：
如果时间片仍然大于零，则什么都不做直接返回。
如果时间片已经为零，则调用 schedule()，很明显，这就是进行进程调度的主干。
*/
void do_timer(long cpl) {
    ...
    // 当前线程还有剩余时间片，直接返回
    if ((--current->counter)>0) return;
    // 若没有剩余时间片，调度
    schedule();
}
```

