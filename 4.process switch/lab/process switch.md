## 修改schedule

目前 Linux 0.11 中工作的 schedule() 函数是首先找到下一个进程的数组位置 next，而这个 next 就是 GDT 中的 n，所以这个 next 是用来找到切换后目标 TSS 段的段描述符的，一旦获得了这个 next 值，直接调用上面剖析的那个宏展开 switch_to(next);就能完成如图 TSS 切换所示的切换了。

现在，我们不用 TSS 进行切换，而是采用切换内核栈的方式来完成进程切换，所以在新的 switch_to 中将用到当前进程的 PCB、目标进程的 PCB、当前进程的内核栈、目标进程的内核栈等信息。由于 Linux 0.11 进程的内核栈和该进程的 PCB 在同一页内存上（一块 4KB 大小的内存），其中 PCB 位于这页内存的低地址，栈位于这页内存的高地址；另外，由于当前进程的 PCB 是用一个全局变量 current 指向的，所以只要告诉新 switch_to()函数一个指向目标进程 PCB 的指针就可以了。同时还要将 next 也传递进去，虽然 TSS(next)不再需要了，但是 LDT(next)仍然是需要的，也就是说，现在每个进程不用有自己的 TSS 了，因为已经不采用 TSS 进程切换了，但是每个进程需要有自己的 LDT，地址分离地址还是必须要有的，而进程切换必然要涉及到 LDT 的切换。

`kernal/sched.c` 中）做稍许修改，即将下面的代码：

```c
if ((*p)->state == TASK_RUNNING && (*p)->counter > c)
    c = (*p)->counter, next = i;

//......

switch_to(next);
```

改为

```c
if ((*p)->state == TASK_RUNNING && (*p)->counter > c)
    c = (*p)->counter, next = i, pnext = *p;

//.......

switch_to(pnext, LDT(next));
```





```c
//初始化全局tss，是任务0的tss
struct task_struct *tss = &(init_task.task.tss);
void schedule(void)
{
	int i,next,c;
	struct task_struct ** p;
        //用PCB切换需要声明
        struct task_struct *pnext = NULL;


/* check alarm, wake up any interruptible tasks that have got a signal */

	for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
		if (*p) {
			if ((*p)->alarm && (*p)->alarm < jiffies) {
					(*p)->signal |= (1<<(SIGALRM-1));
					(*p)->alarm = 0;
				}
			if (((*p)->signal & ~(_BLOCKABLE & (*p)->blocked)) &&
			(*p)->state==TASK_INTERRUPTIBLE)
				(*p)->state=TASK_RUNNING;
		}

/* this is the scheduler proper: */

	while (1) {
		c = -1;
		next = 0;
                 
                pnext = task[next];
		i = NR_TASKS;
		p = &task[NR_TASKS];
		while (--i) {
			if (!*--p)
				continue;
			if ((*p)->state == TASK_RUNNING && (*p)->counter > c)
				c = (*p)->counter, next = i, pnext = *p;
		}
		if (c) break;
		for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
			if (*p)
				(*p)->counter = ((*p)->counter >> 1) +
						(*p)->priority;
	}
    
    

	switch_to(pnext, _LDT(next));
}
```





## 实现 switch_to

由于要对内核栈进行精细的操作，所以需要用汇编代码来完成函数 `switch_to` 的编写。

这个函数依次主要完成如下功能：由于是 C 语言调用汇编，所以需要首先在汇编中处理栈帧，即处理 `ebp` 寄存器；接下来要取出表示下一个进程 PCB 的参数，并和 `current` 做一个比较，如果等于 current，则什么也不用做；如果不等于 current，就开始进程切换，依次完成 PCB 的切换、TSS 中的内核栈指针的重写、内核栈的切换、LDT 的切换以及 PC 指针（即 CS:EIP）的切换。



实现如下:

```assembly

.align 2
switch_to:

    
	pushl %ebp
	movl %esp,%ebp
	pushl %ecx
	pushl %ebx
	pushl %eax

	# (1)判断要切换的进程和当前进程是否是同一个进程
	movl 8(%ebp),%ebx	/*%ebp+8就是从右往左数起第二个参数，也就是*pnext*/
	cmpl %ebx,current	/* 如果当前进程和要切换的进程是同一个进程,就不切换了 */
	je 1f
	/*先得到目标进程的pcb，然后进行判断
    如果目标进程的pcb(存放在ebp寄存器中) 等于   当前进程的pcb => 不需要进行切换，直接退出函数调用
    如果目标进程的pcb(存放在ebp寄存器中) 不等于 当前进程的pcb => 需要进行切换，直接跳到下面去执行*/


	# (2)切换PCB
	movl %ebx,%eax
	xchgl %eax,current
	/*ebx是下一个进程的PCB首地址，current是当前进程PCB首地址*/


	# (3)TSS中的内核栈指针的重写
	movl tss,%ecx		/*%ecx里面存的是tss段的首地址，在后面我们会知道，tss段的首地址就是进程0的tss的首地址，
	根据这个tss段里面的内核栈指针找到内核栈，所以在切换时就要更新这个内核栈指针。也就是说，
	任何正在运行的进程内核栈都被进程0的tss段里的某个指针指向，我们把该指针叫做内核栈指针。*/
    addl $4096,%ebx           /* 未加4KB前，ebx指向下一个进程的PCB首地址，加4096后，相当于为该进程开辟了一个“进程页”，ebx此时指向进程页的最高地址*/
    movl %ebx,ESP0(%ecx)        /* 将内核栈底指针放进tss段的偏移为ESP0（=4）的地方，作为寻找当前进程的内核栈的依据*/
	/* 由上面一段代码可以知道们的“进程页”是这样的，PCB由低地址向上扩展，栈由上向下扩展。
	也可以这样理解，一个进程页就是PCB，我们把内核栈放在最高地址，其它的task_struct从最低地址开始扩展*/


	# (4)切换内核栈
	# KERNEL_STACK代表kernel_stack在PCB表的偏移量，意思是说kernel_stack位于PCB表的第KERNEL_STACK个字节处，注意:PCB表就是task_struct
	movl %esp,KERNEL_STACK(%eax)	/* eax就是上个进程的PCB首地址，这句话是将当前的esp压入旧PCB的kernel_stack。所以该句就是保存旧进程内核栈的操作。*/
	movl 8(%ebp),%ebx		/*%ebp+8就是从左往右数起第一个参数，也就是ebx=*pnext ,pnext就是下一个进程的PCB首地址。至于为什么是8，请查看附录(I)*/
	movl KERNEL_STACK(%ebx),%esp	/*将下一个进程的内核栈指针加载到esp*/


	# (5)切换LDT
	movl 12(%ebp),%ecx         /* %ebp+12就是从左往右数起第二个参数，对应_LDT(next) */
	lldt %cx                /*用新任务的LDT修改LDTR寄存器*/
	/*下一个进程在执行用户态程序时使用的映射表就是自己的 LDT 表了，地址空间实现了分离*/

	# (6)重置一下用户态内存空间指针的选择符fs
	movl $0x17,%ecx
	mov %cx,%fs
	/*通过 fs 访问进程的用户态内存，LDT 切换完成就意味着切换了分配给进程的用户态内存地址空间，
	所以前一个 fs 指向的是上一个进程的用户态内存，而现在需要执行下一个进程的用户态内存，
	所以就需要用这两条指令来重取 fs。但还存在一个问题，就是为什么固定是0x17呢?详见附录(II)*/
	

# 和后面的 clts 配合来处理协处理器，由于和主题关系不大，此处不做论述
    cmpl %eax,last_task_used_math
    jne 1f
    clts


1:  popl %eax
	popl %ebx
	popl %ecx
	popl %ebp
ret
```

注意先要开放switch_to

**开放switch_to**

在system_call.s的头部附近添加以下代码

```c
.globl switch_to
```

即可使switch_to被外面访问到。







### TSS中的内核栈指针的重写

为什么要对TSS进行修改以及要加的信息

TSS 中的内核栈指针的重写可以用下面三条指令完成，其中宏 `ESP0 = 4`，`struct tss_struct *tss = &(init_task.task.tss);` 也是定义了一个全局变量，和 current 类似，用来指向那一段 0 号进程的 TSS 内存。前面已经详细论述过，在中断的时候，要找到内核栈位置，并将用户态下的 `SS:ESP`，`CS:EIP` 以及 `EFLAGS` 这五个寄存器压到内核栈中，这是沟通用户栈（用户态）和内核栈（内核态）的关键桥梁，**而找到内核栈位置就依靠 TR 指向的当前 TSS**。现在虽然不使用 TSS 进行任务切换了，但是 Intel 的这态中断处理机制还要保持，所以仍然需要有一个当前 TSS，这个 TSS 就是我们定义的那个全局变量 tss，即 0 号进程的 tss，所有进程都共用这个 tss，任务切换时不再发生变化。

```c
movl tss,%ecx      
addl $4096,%ebx      
movl %ebx,ESP0(%ecx)   //定义 ESP0 = 4 是因为 TSS 中内核栈指针 esp0 就放在偏移为 4 的地方，看一看 tss 的结构体定义就明白了。
        
// 任务状态段数据结构（参见列表后的信息）。
struct tss_struct
{
	long back_link;		/* 16 high bits zero */
	long esp0;
	long ss0;			/* 16 high bits zero */
	long esp1;
	.....
};
```

注意要声明tss全局变量（已经在schedule中声明了），定义宏变量ESP0

```c
/*在system_call.s下*/
ESP0 = 4	#此处新添
KERNEL_STACK = 12	#此处新添

state	= 0		# these are offsets into the task-struct.
counter	= 4
priority = 8
signal	= 16	#此处修改
sigaction = 20	#此处修改
blocked = (37*16)	#此处修改

```





### 内核栈的切换

完成内核栈的切换也非常简单，和我们前面给出的论述完全一致，将寄存器 esp（内核栈使用到当前情况时的栈顶位置）的值保存到当前 PCB 中，再从下一个 PCB 中的对应位置上取出保存的内核栈栈顶放入 esp 寄存器，这样处理完以后，再使用内核栈时使用的就是下一个进程的内核栈了。**由于现在的 Linux 0.11 的 PCB 定义中没有保存内核栈指针这个域（kernelstack），所以需要加上**，而宏 `KERNEL_STACK` 就是你加的那个位置，当然将 kernelstack 域加在 task_struct 中的哪个位置都可以，但是在某些汇编文件中（主要是在 `kernal/system_call.s` 中）有些关于操作这个结构一些汇编硬编码，所以一旦增加了 kernelstack，**这些硬编码需要跟着修改**，由于第一个位置，即 long state 出现的汇编硬编码很多，所以 kernelstack 千万不要放置在 task_struct 中的第一个位置，当放在其他位置时，修改 `kernal/system_call.s` 中的那些硬编码就可以了。



```C
// 在 include/linux/sched.h 中
struct task_struct {
    long state;
    long counter;
    long priority;
    long kernelstack;
//......
    
/*
由于这里将 PCB 结构体的定义改变了，所以在产生 0 号进程的 PCB 初始化时也要跟着一起变化，需要将原来的 #define INIT_TASK { 0,15,15, 0,{{},},0,... 修改为 #define INIT_TASK { 0,15,15,PAGE_SIZE+(long)&init_task, 0,{{},},0,...，即在 PCB 的第四项中增加关于内核栈栈指针的初始化。
*/
```







### LDT的切换

再下一个切换就是 LDT 的切换了，指令 `movl 12(%ebp),%ecx` 负责取出对应 LDT(next)的那个参数，指令 `lldt %cx` 负责修改 LDTR 寄存器，一旦完成了修改，下一个进程在执行用户态程序时使用的映射表就是自己的 LDT 表了，地址空间实现了分离。

这里还有一个地方需要格外注意，那就是 switch_to 代码中在切换完 LDT 后的两句，即：

```
! 切换 LDT 之后
movl $0x17,%ecx
mov %cx,%fs
```

这两句代码的含义是重新取一下段寄存器 fs 的值，这两句话必须要加、也必须要出现在切换完 LDT 之后，这是因为在实践项目 2 中曾经看到过 fs 的作用——通过 fs 访问进程的用户态内存，LDT 切换完成就意味着切换了分配给进程的用户态内存地址空间，所以前一个 fs 指向的是上一个进程的用户态内存，而现在需要执行下一个进程的用户态内存，所以就需要用这两条指令来重取 fs。



不过，细心的读者可能会发现：fs 是一个选择子，即 fs 是一个指向描述符表项的指针，这个描述符才是指向实际的用户态内存的指针，所以上一个进程和下一个进程的 fs 实际上都是 0x17，真正找到不同的用户态内存是因为两个进程查的 LDT 表不一样，所以这样重置一下 `fs=0x17` 有用吗，有什么用？要回答这个问题就需要对段寄存器有更深刻的认识，实际上段寄存器包含两个部分：显式部分和隐式部分，如下图给出实例所示，就是那个著名的 `jmpi 0, 8`，虽然我们的指令是让 `cs=8`，但在执行这条指令时，会在段表（GDT）中找到 8 对应的那个描述符表项，取出基地址和段限长，除了完成和 eip 的累加算出 PC 以外，还会将取出的基地址和段限长放在 cs 的隐藏部分，即图中的基地址 0 和段限长 7FF。为什么要这样做？下次执行 `jmp 100` 时，由于 cs 没有改过，仍然是 8，所以可以不再去查 GDT 表，而是直接用其隐藏部分中的基地址 0 和 100 累加直接得到 PC，增加了执行指令的效率。现在想必明白了为什么重新设置 fs=0x17 了吧？而且为什么要出现在切换完 LDT 之后？

![图片描述信息](https://doc.shiyanlou.com/userid19614labid571time1424053856897)

### 关于PC的切换

最后一个切换是关于 PC 的切换，和前面论述的一致，依靠的就是 `switch_to` 的最后一句指令 ret，虽然简单，但背后发生的事却很多：`schedule()` 函数的最后调用了这个 `switch_to` 函数，所以这句指令 ret 就返回到下一个进程（目标进程）的 `schedule()` 函数的末尾，遇到的是}，继续 ret 回到调用的 `schedule()` 地方，是在中断处理中调用的，所以回到了中断处理中，就到了中断返回的地址，再调用 iret 就到了目标进程的用户态程序去执行，和书中论述的内核态线程切换的五段论是完全一致的。



**更详细的解释见switch五段论文件**



注意switch编写完成后要把旧的switch注释掉（在include/linux/sched.h中）



## 修改fork函数



在开始修改fork()之前，我们需要明确为什么要修改fork()?

fork()是用来创建子进程的，而创建子进程意味着必须要该线程的一切都配置好了，以后直接切换就可以执行。而由于我们上面修改了进程切换方式，所以原来的配置不管用了。所以我们需要根据切换方式的改变，来改变这些配置



![img](https://img-blog.csdnimg.cn/20201212233556570.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L2x5ajE1OTczNzQwMzQ=,size_16,color_FFFFFF,t_70)

是不是几乎和上面的内核栈一模一样？是那就对了！到最后也是设置好用户态的寄存器然后返回到用户态，原理是一样的。至于为什么多了几个esi、edi、gs寄存器？因为fork()是完全复制父进程的内容到子进程，所以所有寄存器都要复制过去。
综上，经过switch_to和first_return_from_kernel的弹出，一共弹出了eax(0)、ebx、ecx、edx、edi、esi、gs、fs、es、ds10个寄存器，用户态的寄存器内容已经完全弹到相应的寄存器后，就可以执行iret了。

```c
extern void first_return_from_kernel(void);

int copy_process(int nr,long ebp,long edi,long esi,long gs,long none,
		long ebx,long ecx,long edx,
		long fs,long es,long ds,
		long eip,long cs,long eflags,long esp,long ss)
{
        
	struct task_struct *p;
	int i;
	struct file *f;
        long *krnstack;

	p = (struct task_struct *) get_free_page();
	if (!p)
		return -EAGAIN;
	task[nr] = p;
	*p = *current;	/* NOTE! this doesn't copy the supervisor stack */
	p->state = TASK_UNINTERRUPTIBLE;
	p->pid = last_pid;
	p->father = current->pid;
	p->counter = p->priority;
	p->signal = 0;
	p->alarm = 0;
	p->leader = 0;		/* process leadership doesn't inherit */
	p->utime = p->stime = 0;
	p->cutime = p->cstime = 0;
	p->start_time = jiffies;
       
        
	/*p->tss.back_link = 0;
	p->tss.esp0 = PAGE_SIZE + (long) p;
	p->tss.ss0 = 0x10;
	p->tss.eip = eip;
	p->tss.eflags = eflags;
	p->tss.eax = 0;
	p->tss.ecx = ecx;
	p->tss.edx = edx;
	p->tss.ebx = ebx;
	p->tss.esp = esp;
	p->tss.ebp = ebp;
	p->tss.esi = esi;
	p->tss.edi = edi;
	p->tss.es = es & 0xffff;
	p->tss.cs = cs & 0xffff;
	p->tss.ss = ss & 0xffff;
	p->tss.ds = ds & 0xffff;
	p->tss.fs = fs & 0xffff;
	p->tss.gs = gs & 0xffff;
	p->tss.ldt = _LDT(nr);
	p->tss.trace_bitmap = 0x80000000; */
	if (last_task_used_math == current)
		__asm__("clts ; fnsave %0"::"m" (p->tss.i387));
	if (copy_mem(nr,p)) {
		task[nr] = NULL;
		free_page((long) p);
		return -EAGAIN;
	}

        krnstack = (long)(PAGE_SIZE +(long)p);
        *(--krnstack) = ss & 0xffff;
        *(--krnstack) = esp;
        *(--krnstack) = eflags;
        *(--krnstack) = cs & 0xffff;
        *(--krnstack) = eip;
        *(--krnstack) = ds & 0xffff;
        *(--krnstack) = es & 0xffff;
        *(--krnstack) = fs & 0xffff;
        *(--krnstack) = gs & 0xffff;
        *(--krnstack) = esi;
        *(--krnstack) = edi;
        *(--krnstack) = edx;
        *(--krnstack) = (long)first_return_from_kernel;
        *(--krnstack) = ebp;
        *(--krnstack) = ecx;
        *(--krnstack) = ebx;
        *(--krnstack) = 0; //eax，到最后作为fork()的返回值，即子进程的pid
        p->kernelstack = krnstack;


	for (i=0; i<NR_OPEN;i++)
		if ((f=p->filp[i]))
			f->f_count++;
	if (current->pwd)
		current->pwd->i_count++;
	if (current->root)
		current->root->i_count++;
	if (current->executable)
		current->executable->i_count++;
	set_tss_desc(gdt+(nr<<1)+FIRST_TSS_ENTRY,&(p->tss));
	set_ldt_desc(gdt+(nr<<1)+FIRST_LDT_ENTRY,&(p->ldt));
	p->state = TASK_RUNNING;	/* do this last, just in case */
	return last_pid;
}
```

#### first_return_from_kernel的编写



这个first_return_from_kernel对应的是上面的ret_from_sys_call。

```c
/*在system_call.s下*/
.align 2
first_return_from_kernel:
    popl %edx
    popl %edi
    popl %esi
    pop  %gs
    pop  %fs
    pop  %es
    pop  %ds
    iret
```



然后把first_return_from_kernel声明为全局函数

```c
/*在system_call.s头部附近*/
.globl first_return_from_kernel
```



由于在fork()里面调用到first_return_from_kernel，所以声明要使用这个外部函数

```c
/*在fork.c头部附近*/
extern void first_return_from_kernel(void);

```











## 参考资料

[(9条消息) 哈工大操作系统实验四——基于内核栈切换的进程切换（极其详细）_lcxc的博客-CSDN博客_基于内核栈切换的进程切换](https://blog.csdn.net/lyj1597374034/article/details/111033682)

[(9条消息) 哈工大-操作系统-HitOSlab-李治军-实验4-基于内核栈切换的进程切换_garbage_man的博客-CSDN博客](https://blog.csdn.net/qq_42518941/article/details/119182097)
