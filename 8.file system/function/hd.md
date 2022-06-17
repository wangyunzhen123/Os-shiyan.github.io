### fs\buffer.c\bread

```c
//// 从指定设备上读取指定的数据块。
struct buffer_head * bread(int dev,int block)
{
	struct buffer_head * bh;

// 在高速缓冲中申请一块缓冲区。如果返回值是NULL 指针，表示内核出错，死机。
	if (!(bh=getblk(dev,block)))
		panic("bread: getblk returned NULL\n");
// 如果该缓冲区中的数据是有效的（已更新的）可以直接使用，则返回。
	if (bh->b_uptodate)
		return bh;
// 否则调用ll_rw_block()函数，产生读设备块请求。并等待缓冲区解锁。
	ll_rw_block(READ,bh);
	wait_on_buffer(bh);
// 如果该缓冲区已更新，则返回缓冲区头指针，退出。
	if (bh->b_uptodate)
		return bh;
// 否则表明读设备操作失败，释放该缓冲区，返回NULL 指针，退出。
	brelse(bh);
	return NULL;
}
```



```c
//// 低层读写数据块函数。
// 该函数主要是在fs/buffer.c 中被调用。实际的读写操作是由设备的request_fn()函数完成。
// 对于硬盘操作，该函数是do_hd_request()。（kernel/blk_drv/hd.c,294）
void ll_rw_block (int rw, struct buffer_head *bh)
{
	unsigned int major;		// 主设备号（对于硬盘是3）。

// 如果设备的主设备号不存在或者该设备的读写操作函数不存在，则显示出错信息，并返回。
	if ((major = MAJOR (bh->b_dev)) >= NR_BLK_DEV ||
		!(blk_dev[major].request_fn))
	{
		printk ("Trying to read nonexistent block-device\n\r");
		return;
	}
	make_request (major, rw, bh);	// 创建请求项并插入请求队列。
}

```





/make_request

```c

//// 创建请求项并插入请求队列。参数是：主设备号major，命令rw，存放数据的缓冲区头指针bh。
static void
make_request (int major, int rw, struct buffer_head *bh)
{
	struct request *req;
	int rw_ahead;

/* WRITEA/READA 是特殊的情况 - 它们并不是必要的，所以如果缓冲区已经上锁，*/
/* 我们就不管它而退出，否则的话就执行一般的读/写操作。 */
// 这里'READ'和'WRITE'后面的'A'字符代表英文单词Ahead，表示提前预读/写数据块的意思。
// 当指定的缓冲区正在使用，已被上锁时，就放弃预读/写请求。
	if (rw_ahead = (rw == READA || rw == WRITEA))
	{
		if (bh->b_lock)
			return;
		if (rw == READA)
			rw = READ;
		else
			rw = WRITE;
	}
// 如果命令不是READ 或WRITE 则表示内核程序有错，显示出错信息并死机。
	if (rw != READ && rw != WRITE)
		panic ("Bad block dev command, must be R/W/RA/WA");
// 锁定缓冲区，如果缓冲区已经上锁，则当前任务（进程）就会睡眠，直到被明确地唤醒。
	lock_buffer (bh);
// 如果命令是写并且缓冲区数据不脏，或者命令是读并且缓冲区数据是更新过的，则不用添加
// 这个请求。将缓冲区解锁并退出。
	if ((rw == WRITE && !bh->b_dirt) || (rw == READ && bh->b_uptodate))
	{
		unlock_buffer (bh);
		return;
	}
repeat:
/* 我们不能让队列中全都是写请求项：我们需要为读请求保留一些空间：读操作

* 是优先的。请求队列的后三分之一空间是为读准备的。
  */
  // 请求项是从请求数组末尾开始搜索空项填入的。根据上述要求，对于读命令请求，可以直接
  // 从队列末尾开始操作，而写请求则只能从队列的2/3 处向头上搜索空项填入。
  if (rw == READ)
  	req = request + NR_REQUEST;	// 对于读请求，将队列指针指向队列尾部。
  else
  	req = request + ((NR_REQUEST * 2) / 3);	// 对于写请求，队列指针指向队列2/3 处。
  /* 搜索一个空请求项 */
  // 从后向前搜索，当请求结构request 的dev 字段值=-1 时，表示该项未被占用。
  while (--req >= request)
  	if (req->dev < 0)
  		break;
  /* 如果没有找到空闲项，则让该次新请求睡眠：需检查是否提前读/写 */
  // 如果没有一项是空闲的（此时request 数组指针已经搜索越过头部），则查看此次请求是否是
  // 提前读/写（READA 或WRITEA），如果是则放弃此次请求。否则让本次请求睡眠（等待请求队列
  // 腾出空项），过一会再来搜索请求队列。
  if (req < request)
  {				// 如果请求队列中没有空项，则
  	if (rw_ahead)
  	{			// 如果是提前读/写请求，则解锁缓冲区，退出。
  		unlock_buffer (bh);
  		return;
  	}
  	sleep_on (&wait_for_request);	// 否则让本次请求睡眠，过会再查看请求队列。
  	goto repeat;
  }
  /* 向空闲请求项中填写请求信息，并将其加入队列中 */
  // 请求结构参见（kernel/blk_drv/blk.h,23）。
  req->dev = bh->b_dev;		// 设备号。
  req->cmd = rw;		// 命令(READ/WRITE)。
  req->errors = 0;		// 操作时产生的错误次数。
  req->sector = bh->b_blocknr << 1;	// 起始扇区。(1 块=2 扇区)
  req->nr_sectors = 2;		// 读写扇区数。
  req->buffer = bh->b_data;	// 数据缓冲区。
  req->waiting = NULL;		// 任务等待操作执行完成的地方。
  req->bh = bh;			// 缓冲区头指针。
  req->next = NULL;		// 指向下一请求项。
  add_request (major + blk_dev, req);	// 将请求项加入队列中(blk_dev[major],req)。
  }


```



###  将磁盘请求加到电梯队列中kernel\blk_drv\ll_rw_blk.c\add_request

```c
/*
* 下面的定义用于电梯算法：注意读操作总是在写操作之前进行。
* 这是很自然的：读操作对时间的要求要比写严格得多。
*/
#define IN_ORDER(s1,s2) \
((s1)->cmd<(s2)->cmd || (s1)->cmd==(s2)->cmd && \
((s1)->dev < (s2)->dev || ((s1)->dev == (s2)->dev && \
(s1)->sector < (s2)->sector)))

/*
* add-request()向连表中加入一项请求。它关闭中断，
* 这样就能安全地处理请求连表了 
*/
//// 向链表中加入请求项。参数dev 指定块设备，req 是请求的结构信息。
  static void
add_request (struct blk_dev_struct *dev, struct request *req)
{
	struct request *tmp;

	req->next = NULL;
	cli ();			// 关中断。
	if (req->bh)
		req->bh->b_dirt = 0;	// 清缓冲区“脏”标志。
// 如果dev 的当前请求(current_request)子段为空，则表示目前该设备没有请求项，本次是第1 个
// 请求项，因此可将块设备当前请求指针直接指向请求项，并立刻执行相应设备的请求函数。
	if (!(tmp = dev->current_request))
	{
		dev->current_request = req;
		sti ();			// 开中断。
		(dev->request_fn) ();	// 执行设备请求函数，对于硬盘(3)是do_hd_request()。
		return;
	}
// 如果目前该设备已经有请求项在等待，则首先利用电梯算法搜索最佳位置，然后将当前请求插入
// 请求链表中。
	for (; tmp->next; tmp = tmp->next)
	if ((IN_ORDER (tmp, req) ||
		!IN_ORDER (tmp, tmp->next)) && IN_ORDER (req, tmp->next))
		break;
	req->next = tmp->next;
	tmp->next = req;
	sti ();
}
```

![这里写图片描述](https://img-blog.csdn.net/20170118213449332?watermark/2/text/aHR0cDovL2Jsb2cuY3Nkbi5uZXQvYWNfZGFvX2Rp/font/5a6L5L2T/fontsize/400/fill/I0JBQkFCMA==/dissolve/70/gravity/SouthEast)

```c

// 块设备结构。
struct blk_dev_struct
{
  void (*request_fn) (void);	// 请求操作的函数指针。
  struct request *current_request;	// 请求信息结构。
};
```





### 将bolock转换成柱面号、磁头号、扇区号kernel\blk_drv\hd.c\do_hd_request

 在add_request里面，如果当前表头dev->current_request为空，则直接调用(dev->request_fn)()。这个函数是启动读写的关键函数。对于每个设备都有一个request_fn函数。这里以硬盘为例。

```c
// 执行硬盘读写请求操作。
void do_hd_request (void)
{
	int i, r;
	unsigned int block, dev;
	unsigned int sec, head, cyl;
	unsigned int nsect;

	INIT_REQUEST;		// 检测请求项的合法性(参见kernel/blk_drv/blk.h,127)。
// 取设备号中的子设备号(见列表后对硬盘设备号的说明)。子设备号即是硬盘上的分区号。
	dev = MINOR (CURRENT->dev);	// CURRENT 定义为(blk_dev[MAJOR_NR].current_request)。
	block = CURRENT->sector;	// 请求的起始扇区。
// 如果子设备号不存在或者起始扇区大于该分区扇区数-2，则结束该请求，并跳转到标号repeat 处
// （定义在INIT_REQUEST 开始处）。因为一次要求读写2 个扇区（512*2 字节），所以请求的扇区号
// 不能大于分区中最后倒数第二个扇区号。
	if (dev >= 5 * NR_HD || block + 2 > hd[dev].nr_sects)
	{
		end_request (0);
		goto repeat;		// 该标号在blk.h 最后面。
	}
	block += hd[dev].start_sect;	// 将所需读的块对应到整个硬盘上的绝对扇区号。
	dev /= 5;			// 此时dev 代表硬盘号（0 或1）。
// 下面嵌入汇编代码用来从硬盘信息结构中根据起始扇区号和每磁道扇区数计算在磁道中的
// 扇区号(sec)、所在柱面号(cyl)和磁头号(head)。
	sec = hd_info[dev].sect;
	_asm {
		mov eax,block
		xor edx,edx
		mov ebx,sec
		div ebx
		mov block,eax
		mov sec,edx
	}
//__asm__ ("divl %4": "=a" (block), "=d" (sec):"" (block), "1" (0),
//	   "r" (hd_info[dev].
//		sect));
	head = hd_info[dev].head;
	_asm {
		mov eax,block
		xor edx,edx
		mov ebx,head
		div ebx
		mov cyl,eax
		mov head,edx
	}
//__asm__ ("divl %4": "=a" (cyl), "=d" (head):"" (block), "1" (0),
//	   "r" (hd_info[dev].
//		head));
	sec++;
	nsect = CURRENT->nr_sectors;	// 欲读/写的扇区数。
// 如果reset 置1，则执行复位操作。复位硬盘和控制器，并置需要重新校正标志，返回。
	if (reset)
	{
		reset = 0;
		recalibrate = 1;
		reset_hd (CURRENT_DEV);
		return;
	}
// 如果重新校正标志(recalibrate)置位，则首先复位该标志，然后向硬盘控制器发送重新校正命令。
	if (recalibrate)
	{
		recalibrate = 0;
		hd_out (dev, hd_info[CURRENT_DEV].sect, 0, 0, 0,
		  WIN_RESTORE, &recal_intr);
		return;
	}
// 如果当前请求是写扇区操作，则发送写命令，循环读取状态寄存器信息并判断请求服务标志
// DRQ_STAT 是否置位。DRQ_STAT 是硬盘状态寄存器的请求服务位（include/linux/hdreg.h，27）。
	if (CURRENT->cmd == WRITE)
	{
		hd_out (dev, nsect, sec, head, cyl, WIN_WRITE, &write_intr);
		for (i = 0; i < 3000 && !(r = inb_p (HD_STATUS) & DRQ_STAT); i++)
// 如果请求服务位置位则退出循环。若等到循环结束也没有置位，则此次写硬盘操作失败，去处理
// 下一个硬盘请求。否则向硬盘控制器数据寄存器端口HD_DATA 写入1 个扇区的数据。
		if (!r)
		{
			bad_rw_intr ();
			goto repeat;		// 该标号在blk.h 最后面，也即跳到301 行。
		}
		port_write (HD_DATA, CURRENT->buffer, 256);
// 如果当前请求是读硬盘扇区，则向硬盘控制器发送读扇区命令。
	}
	else if (CURRENT->cmd == READ)
	{
		hd_out (dev, nsect, sec, head, cyl, WIN_READ, &read_intr);
	}
	else
		panic ("unknown hd-command");
}
```







### 填写硬盘寄存器kernel\blk_drv\hd.c\hd_out



```c
//// 向硬盘控制器发送命令块（参见列表后的说明）。
// 调用参数：drive - 硬盘号(0-1)； nsect - 读写扇区数；
// sect - 起始扇区； head - 磁头号；
// cyl - 柱面号； cmd - 命令码；
// *intr_addr() - 硬盘中断处理程序中将调用的C 处理函数。
static void hd_out (unsigned int drive, unsigned int nsect, unsigned int sect,
		    unsigned int head, unsigned int cyl, unsigned int cmd,
		    void (*intr_addr) (void))
{
	register int port; //asm ("dx");	// port 变量对应寄存器dx。

	if (drive > 1 || head > 15)	// 如果驱动器号(0,1)>1 或磁头号>15，则程序不支持。
		panic ("Trying to write bad sector");
	if (!controller_ready ())	// 如果等待一段时间后仍未就绪则出错，死机。
		panic ("HD controller not ready");
	do_hd = intr_addr;		// do_hd 函数指针将在硬盘中断程序中被调用。
	outb_p (hd_info[drive].ctl, HD_CMD);	// 向控制寄存器(0x3f6)输出控制字节。
	port = HD_DATA;		// 置dx 为数据寄存器端口(0x1f0)。
	outb_p (hd_info[drive].wpcom >> 2, ++port);	// 参数：写预补偿柱面号(需除4)。
	outb_p (nsect, ++port);	// 参数：读/写扇区总数。
	outb_p (sect, ++port);	// 参数：起始扇区。
	outb_p (cyl, ++port);		// 参数：柱面号低8 位。
	outb_p (cyl >> 8, ++port);	// 参数：柱面号高8 位。
	outb_p (0xA0 | (drive << 4) | head, ++port);	// 参数：驱动器号+磁头号。
	outb (cmd, ++port);		// 命令：硬盘控制命令。
}
```







### 硬盘中断程序kernel/system_call.s



```c
// 硬盘系统初始化。
void hd_init (void)
{
	blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST;	// do_hd_request()。
	set_intr_gate (0x2E, &hd_interrupt);	// 设置硬盘中断门向量 int 0x2E(46)。
// hd_interrupt 在(kernel/system_call.s,221)。
	outb_p (inb_p (0x21) & 0xfb, 0x21);	// 复位接联的主8259A int2 的屏蔽位，允许从片
// 发出中断请求信号。
	outb (inb_p (0xA1) & 0xbf, 0xA1);	// 复位硬盘的中断请求屏蔽位（在从片上），允许
// 硬盘控制器发送中断请求信号。
}
```





```assembly
;//// int 46 -- (int 0x2E) 硬盘中断处理程序，响应硬件中断请求IRQ14。
;// 当硬盘操作完成或出错就会发出此中断信号。(参见kernel/blk_drv/hd.c)。
;// 首先向8259A 中断控制从芯片发送结束硬件中断指令(EOI)，然后取变量do_hd 中的函数指针放入edx
;// 寄存器中，并置do_hd 为NULL，接着判断edx 函数指针是否为空。如果为空，则给edx 赋值指向
;// unexpected_hd_interrupt()，用于显示出错信息。随后向8259A 主芯片送EOI 指令，并调用edx 中
;// 指针指向的函数: read_intr()、write_intr()或unexpected_hd_interrupt()。
_hd_interrupt:
	push eax
	push ecx
	push edx
	push ds
	push es
	push fs
	mov eax,10h ;// ds,es 置为内核数据段。
	mov ds,ax
	mov es,ax
	mov eax,17h ;// fs 置为调用程序的局部数据段。
	mov fs,ax
;// 由于初始化中断控制芯片时没有采用自动EOI，所以这里需要发指令结束该硬件中断。
	mov al,20h
	out 0A0h,al ;// EOI to interrupt controller ;//1 ;// 送从8259A。
	jmp l3 ;// give port chance to breathe
l3: jmp l4 ;// 延时作用。
l4: xor edx,edx
	xchg edx,dword ptr _do_hd ;// do_hd 定义为一个函数指针，将被赋值read_intr()或
;// write_intr()函数地址。(kernel/blk_drv/hd.c)
;// 放到edx 寄存器后就将do_hd 指针变量置为NULL。
	test edx,edx ;// 测试函数指针是否为Null。
	jne l5 ;// 若空，则使指针指向C 函数unexpected_hd_interrupt()。
	mov edx,dword ptr _unexpected_hd_interrupt ;// (kernel/blk_drv/hdc,237)。
l5: out 20h,al ;// 送主8259A 中断控制器EOI 指令（结束硬件中断）。
	call edx ;// "interesting" way of handling intr.
	pop fs ;// 上句调用do_hd 指向的C 函数。
	pop es
	pop ds
	pop edx
	pop ecx
	pop eax
	iretd
```

kernel\blk_drv\hd.c

```c
//// 读操作中断调用函数。将在执行硬盘中断处理程序中被调用。
static void read_intr (void)
{
	if (win_result ())
	{				// 若控制器忙、读写错或命令执行错，
		bad_rw_intr ();		// 则进行读写硬盘失败处理
		do_hd_request ();		// 然后再次请求硬盘作相应(复位)处理。
		return;
	}
	port_read (HD_DATA, CURRENT->buffer, 256);	// 将数据从数据寄存器口读到请求结构缓冲区。
	CURRENT->errors = 0;		// 清出错次数。
	CURRENT->buffer += 512;	// 调整缓冲区指针，指向新的空区。
	CURRENT->sector++;		// 起始扇区号加1，
	if (--CURRENT->nr_sectors)
	{				// 如果所需读出的扇区数还没有读完，则
		do_hd = &read_intr;	// 再次置硬盘调用C 函数指针为read_intr()
		return;			// 因为硬盘中断处理程序每次调用do_hd 时
	}				// 都会将该函数指针置空。参见system_call.s
	end_request (1);		// 若全部扇区数据已经读完，则处理请求结束事宜，
	do_hd_request ();		// 执行其它硬盘请求操作。
}

//// 写扇区中断调用函数。在硬盘中断处理程序中被调用。
// 在写命令执行后，会产生硬盘中断信号，执行硬盘中断处理程序，此时在硬盘中断处理程序中调用的
// C 函数指针do_hd()已经指向write_intr()，因此会在写操作完成（或出错）后，执行该函数。
static void write_intr (void)
{
	if (win_result ())
	{				// 如果硬盘控制器返回错误信息，
		bad_rw_intr ();		// 则首先进行硬盘读写失败处理，
		do_hd_request ();		// 然后再次请求硬盘作相应(复位)处理，
		return;			// 然后返回（也退出了此次硬盘中断）。
	}
	if (--CURRENT->nr_sectors)
	{				// 否则将欲写扇区数减1，若还有扇区要写，则
		CURRENT->sector++;	// 当前请求起始扇区号+1，
		CURRENT->buffer += 512;	// 调整请求缓冲区指针，
		do_hd = &write_intr;	// 置硬盘中断程序调用函数指针为write_intr()，
		port_write (HD_DATA, CURRENT->buffer, 256);	// 再向数据寄存器端口写256 字节。
		return;			// 返回等待硬盘再次完成写操作后的中断处理。
	}
	end_request (1);		// 若全部扇区数据已经写完，则处理请求结束事宜，
	do_hd_request ();		// 执行其它硬盘请求操作。
}
```





### 参考资料

[(9条消息) Linux 0.11 块设备文件的使用_ac_dao_di的博客-CSDN博客](https://blog.csdn.net/ac_dao_di/article/details/54615951)
