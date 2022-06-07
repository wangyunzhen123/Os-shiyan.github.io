linux/kernel/sched.c



```c
/*
把当前任务置为不可中断的等待状态，并让睡眠队列头指针指向当前任务。只有明确地唤醒时才会返回。该函数提供了进程与中断处理程序之间的同步机制。函数参数 p 是等待任务队列头指针。指针是含有一个变量地址的变量。这里参数 p 使用了指针的指针形式’** p‘ ，这是因为 C 函数参数只能传值，没有直接的万式让被调用函数改变调用该函数程序中变量的值。但是指针’*p’指向的目标（这里是任务结构）会改变，因此为了能修改调用该函数程序中原来就是指针变量的值，就需要传递指针’’的指针，即’** p ’。参见图8-6中 p 指针的使用情况。
*/
void sleep_on(struct task_struct **p)
{
	struct task_struct *tmp;

/* 
若指针无效，则退出。（指针所指的对象可以是 NULL ，但指针本身不应该为0。另外，如果
当前任务是任务0，则死机。因为任务0的运行不依赖自己的状态，所以内核代码把任务0置
为睡眠状态毫无意义。
*/
	if (!p)
		return;
	if (current == &(init_task.task))
		panic("task[0] trying to sleep");
	tmp = *p;
	*p = current;
	current->state = TASK_UNINTERRUPTIBLE;
	schedule();
	if (tmp)
		tmp->state=0;
}
```





一段较难理解的代码段

```c
tmp = *p;
*p = current;
current->state = TASK_UNINTERRUPTIBLE;
schedule();
//这段执行完后，*p指向等待队列的首任务，tmp执行下一个等待任务，current指向当前运行的程序，这样就形成了一个隐式的链表
```





![image-20220601183737180](C:\Users\dell\AppData\Roaming\Typora\typora-user-images\image-20220601183737180.png)

![image-20220601183812075](C:\Users\dell\AppData\Roaming\Typora\typora-user-images\image-20220601183812075.png)



```c
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

	if (!p)
		return;
	if (current == &(init_task.task))
		panic("task[0] trying to sleep");
	tmp=*p;
	*p=current;
repeat:	current->state = TASK_INTERRUPTIBLE;
	schedule();
	if (*p && *p != current) {
		(**p).state=0;
		goto repeat;
	}
	*p=NULL;
	if (tmp)
		tmp->state=0;
}
```



```c
//唤醒*p指向的任务。*p是任务等待队列头指针。由于新等待任务是插入在等待队列头指针处的，因此唤醒的是最后进入等待
void wake_up(struct task_struct **p)
{
	if (p && *p) {
		(**p).state=0;
		*p=NULL;
	}
}
```

