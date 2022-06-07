fork创建一个进程详解

根据前一个实验系统调用的知识，fork会展开为一个int80中断，跳到systemcall中

```assembly
.align 2
bad_sys_call:
	movl $-1,%eax
	iret
.align 2

#重新执行调度程序入口。调度程序schedule在（kenerl/sche.c104）
#当调度程序返回时就从ret_from_sys_call处返回
reschedule:
	pushl $ret_from_sys_call
	jmp schedule


system_call:
	cmpl $nr_system_calls-1,%eax
	ja bad_sys_call
	push %ds
	push %es
	push %fs
	pushl %edx
	pushl %ecx		# push %ebx,%ecx,%edx as parameters
	pushl %ebx		# to the system call
	movl $0x10,%edx		# set up ds,es to kernel space
	mov %dx,%ds
	mov %dx,%es
	movl $0x17,%edx		# fs points to local data space
	mov %dx,%fs
	#下面这句操作数的含义是：调用地址＝[ sys call _ table +% eax *。参见程序后的说明。
    # sys _ call _ table 是一个指针数组，定义在 inelude / linux / sys . h 中。该指针数组中设置了＃所有72个系统调用 C 处理函数的地址，这里就会调用sys_fork
	call sys_call_table(,%eax,4)
	pushl %eax
	movl current,%eax
	cmpl $0,state(%eax)		# state
	jne reschedule
	cmpl $0,counter(%eax)		# counter
	je reschedule
	
	#下面查看当前任务的运行状态。如果不在就绪状态（ state 不等于0）就去执行调度程序。如果该任务在就绪状态，但其时间片已用完（ counter =0)，则也去执行调度程序。例如当后台进程组中的进程执行控制终端读写操作时，那么默认条件下该后台进程组所有进程会收到 SIGTTIN 或 SIGTTOU 信号，导致进程组中所有进程处于停止状态。而当前进程则会立刻返回。
	movl current,%eax
	cmpl $0,state(%eax)		# state
	jne reschedule
	cmpl $0,counter(%eax)		# counter
	je reschedule
	
	
	#以下这段代码执行从系统调用 C 函数返回后，对信号进行识别处理。其他中断服务程序退出时也＃将跳转到这里进行处理后才退出中断过程，
	ret_from_sys_call:
	movl current,%eax		# task[0] cannot have signals
	cmpl task,%eax
	je 3f
	cmpw $0x0f,CS(%esp)		# was old code segment supervisor ?
	jne 3f
	cmpw $0x17,OLDSS(%esp)		# was stack segment = 0x17 ?
	jne 3f
	movl signal(%eax),%ebx
	movl blocked(%eax),%ecx
	notl %ecx
	andl %ebx,%ecx
	bsfl %ecx,%ecx
	je 3f
	btrl %ecx,%ebx
	movl %ebx,signal(%eax)
	incl %ecx
	pushl %ecx
	call do_signal
	popl %eax
3:	popl %eax
	popl %ebx
	popl %ecx
	popl %edx
	pop %fs
	pop %es
	pop %ds
	iret

```



sys_fork

```assembly
#sys_fork（）调用，用于创建子进程，是systemcall的功能，原形在inclulde/linux/sys.h中
#首先调用C函数find_empty_process(),取得一个进程号pid。若返回负数则说明目前任务数组已满。然后调用copy_process（）复制进程

_sys_fork:
       call _find_empty_process         #调用find_empty_process()  （kernel/fork.c,135）
       test1 %eax,%eax                      #在eax中范围进程进程号pid。若1返回负数则退出
       js 1f
       push %gs
       pushl %esi
       push1 &edi
       pushl %ebp
       push1 %eax
       call _copy_process                  #调用C函数copy_process(kernel/fork.c, 68)
       add1 $20,%esp                       #丢弃这里所有压栈内容
1:     ret

```





_find_empty_process

```c
//复制进程，该函数是进入系统调用中断处理过程（system_call.s）开始，逐步压入栈的各寄存器的值：
//1）CPU执行中毒那指令压入的用户栈地址ss和esp、标志寄存器flags和返回地址cs和eip
//2）第83-88行在刚进入system_cal时压入栈的段寄存器ds、es、fs和edx、ecx、ebx
//3）di94行调用sys_call_table中sys_fork函数时压入栈的gs、esi、edi、ebp和eax(nr)值
int copy_process(int nr, long ebp,long edi,long esi, long gs, long none, long ebx, long ecx, long edx,long fs, long es, long ds, long eip, long cd ,long eflag, long es, long ds, long eip, long cd, long flags, long esp, long ss) {
      struct task_struck *p;
     int i;
     struck file *f  
     
     //首先为新任务数据结构分配内存。如果内存分配出错，则返回出错码并退出。然后新任务结构指针放入任务数组的nr项中。其中nr为任务        号，由前面find_empty_process()返回。接着把当前进程任务结构内容复制到刚申请的内存页面p开始处
     p = (struct task_struct *) get_free_page();
	if (!p)
		return -EAGAIN;
    *p = *current //注意！这样做不会复制超级用户栈（只会复制进程结构）
   
    /*随后对复制来的进程结构内容进行一些修改，作为新进程的任务结构。先将新进程的状态设置为不可中断等待状态，以防止内核调度其执行       然后设置新进程的进程号pid和父进程号father，并初始化进程运行时间片值等于其priority值（一般为15个嘀嗒）。接着复位新进程的       信号位图、报警定时器，会话（session）和领导标志leader、进程及其子进程在内核和用户态运行时间统计值，还会设置进程开始运行       的系统时间start_time*/
     
    p->state = TASK_UNINTERRUPTIBLE;  
	p->pid = last_pid;             //新进程号，也由find_empty_process()得到
	p->father = current->pid;      //设置父进程号
	p->counter = p->priority;      //运行时间片值
	p->signal = 0;                 //信号位图置为0
	p->alarm = 0;                  //报警定时器（滴答数）
	p->leader = 0;		/* process leadership doesn't inherit 进程的领导权是不能继承的*/
	p->utime = p->stime = 0;       //用户态时间和核心态运行时间
	p->cutime = p->cstime = 0;     //子进程用户态和核心态运行时间
	p->start_time = jiffies;       //进程开始运行时间（当前时间滴答数）
    
/*再修改任务状态段TSS数据。由于系统给任务结构p分配了1页新内存，所以（PAGE_SIZE + (long)p）让esp0正好执行该页顶端。 ss0：esp0用作程序在内核态执行时的栈。另外，每个任务在GDT表中都有两个段描述符，一个是任务的TSS段描述符，另一个是任务的LDT表段描述符，下面一个语句就是把GDT中本任务LDT段描述符的选择符保存在本任务的TSS段中。当CPU执行切换任务时，就会自动从TSS中把LDT段描述符的选择符加载到ldtr寄存器中*/
    p->tss.back_link = 0;
	p->tss.esp0 = PAGE_SIZE + (long) p;   //任务内核态指针
	p->tss.ss0 = 0x10;       //内核态栈的段选择符（与内核数据相同）
	p->tss.eip = eip;        //指令代码指针
	p->tss.eflags = eflags;  //标志寄存器
	p->tss.eax = 0;          //这是fork()返回新进程会返回0的原因所在
	p->tss.ecx = ecx;
	p->tss.edx = edx;
	p->tss.ebx = ebx;
	p->tss.esp = esp;
	p->tss.ebp = ebp;
	p->tss.esi = esi;
	p->tss.edi = edi;
	p->tss.es = es & 0xffff;   //段寄存器仅有16位有效
	p->tss.cs = cs & 0xffff;
	p->tss.ss = ss & 0xffff;
	p->tss.ds = ds & 0xffff;
	p->tss.fs = fs & 0xffff;
	p->tss.gs = gs & 0xffff;
	p->tss.ldt = _LDT(nr);   //任务局部表描述符的选择符（LDT描述符表在GDT中）
	p->tss.trace_bitmap = 0x80000000;
   
    
    if (last_task_used_math == current)
		__asm__("clts ; fnsave %0"::"m" (p->tss.i387));
    
    /*接下来复制进程页表。即在线性地址空间中设置新任务代码段和数据段描述符中的基址
      和限长，并复制页表。如果出错（返回值不是0)，则复位任务数组中相应项并释放为该新任务分配的用于任务结构的内存页。*/
    if (copy_mem(nr,p)) {
		task[nr] = NULL;
		free_page((long) p);
		return -EAGAIN;
	}
    
    /*如果父进程中有文件是打开的，则将对应文件的打开次数增1。因为这里创建的子进程会与父进程共享这些打开的文件。将当前进程（父进程）的 pwd , root 和 executable 引用次数均增1。与上面同样的道理，子进程也引用了这些 i 节点。*/
    for (i=0; i<NR_OPEN;i++)
		if ((f=p->filp[i]))
			f->f_count++;
	if (current->pwd)
		current->pwd->i_count++;
	if (current->root)
		current->root->i_count++;
	if (current->executable)
		current->executable->i_count++;
    
   /*随后在 GDT 表中设置新任务 TSS 段和 LDT 段描述符项。这两个段的限长均被设置成104
     1／字节。 set _ tss _ desc O 和 set _ ldt _ desc O 的定义参见 include / asm / system . h 文件/52-66行代码。“      gdt +( nr <<1) FIRST _ TSS _ ENTRY ”是任务 nr 的 TSS 描述符项在全局1／表中的地址。因为每个任务占用 GDT 表中2项，因      此上式中要包括’( nr くく1)’。程序然后把新进程设置成就绪态。另外在任务切换时，任务寄存器 tr 由 CPU 自动加载。最后返回新进      程号。*/
    set_tss_desc(gdt+(nr<<1)+FIRST_TSS_ENTRY,&(p->tss));
	set_ldt_desc(gdt+(nr<<1)+FIRST_LDT_ENTRY,&(p->ldt));
	p->state = TASK_RUNNING;	/* do this last, just in case 最后才将新任务设置为就绪态，以防万一*/
    
	return last_pid;
}
```

重要部分：

![image](https://api2.mubu.com/v3/document_image/e074b2ec-0c93-472e-af10-e8d8c11da2a1-6803441.jpg)
