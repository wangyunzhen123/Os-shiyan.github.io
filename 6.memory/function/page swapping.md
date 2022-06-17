故事从缺页中断开始，当MMU发现请求页面不在内存中时，就会发送14号中断

![image-20220605150822558](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220605150822558.png)

缺页中断处理函数

```assembly
.code
_page_fault:
	xchg ss:[esp],eax	;// 取出错码到eax。
	push ecx
	push edx
	push ds
	push es
	push fs
	
	mov edx,10h		;// 置内核数据段选择符。
	mov ds,dx
	mov es,dx
	mov fs,dx
	
	;//注意cr2寄存器的作用，这里压栈传参
	mov edx,cr2			;// 取引起页面异常的线性地址
	push edx			;// 将该线性地址和出错码压入堆栈，作为调用函数的参数。
	push eax
	
	test eax,1			;// 测试标志P，如果不是缺页引起的异常则跳转。
	jne l1
	call _do_no_page	;// 调用缺页处理函数（mm/memory.c,365 行）。
	jmp l2			
l1:	call _do_wp_page	;// 调用写保护处理函数（mm/memory.c,247 行）。
l2:	add esp,8		;// 丢弃压入栈的两个参数。
	pop fs
	pop es
	pop ds
	pop edx
	pop ecx
	pop eax
	iretd
end
```



缺页处理函数

```c
// 页异常中断处理调用的函数。处理缺页异常情况。在page.s 程序中被调用。
// 参数error_code 是由CPU 自动产生，address 是页面线性地址。

void do_no_page(unsigned long error_code,unsigned long address)
{
	int nr[4];
	unsigned long tmp;
	unsigned long page;
	int block,i;

	address &= 0xfffff000;// 页面地址。
// 首先算出指定线性地址在进程空间中相对于进程基址的偏移长度值。
	tmp = address - current->start_code;
// 若当前进程的executable 空，或者指定地址超出代码+数据长度，则申请一页物理内存，并映射
// 影射到指定的线性地址处。executable 是进程的i 节点结构。该值为0，表明进程刚开始设置，
// 需要内存；而指定的线性地址超出代码加数据长度，表明进程在申请新的内存空间，也需要给予。
// 因此就直接调用get_empty_page()函数，申请一页物理内存并映射到指定线性地址处即可。
// start_code 是进程代码段地址，end_data 是代码加数据长度。对于linux 内核，它的代码段和
// 数据段是起始基址是相同的。
	if (!current->executable || tmp >= current->end_data) {
		get_empty_page(address);
		return;
	}
// 如果尝试共享页面成功，则退出。
	if (share_page(tmp))
		return;
// 取空闲页面，如果内存不够了，则显示内存不够，终止进程。
	if (!(page = get_free_page()))       //get_free_page()函数中可以加页面置换算法
		oom()
        
        ;
/* 记住，（程序）头要使用1 个数据块 */
// 首先计算缺页所在的数据块项。BLOCK_SIZE = 1024 字节，因此一页内存需要4 个数据块。
	block = 1 + tmp/BLOCK_SIZE;
// 根据i 节点信息，取数据块在设备上的对应的逻辑块号。
	for (i=0 ; i<4 ; block++,i++)
		nr[i] = bmap(current->executable,block);
// 读设备上一个页面的数据（4 个逻辑块）到指定物理地址page 处。
	bread_page(page,current->executable->i_dev,nr);
    
// 在增加了一页内存后，该页内存的部分可能会超过进程的end_data 位置。下面的循环即是对物理
// 页面超出的部分进行清零处理。
	i = tmp + 4096 - current->end_data;
	tmp = page + 4096;
	while (i-- > 0) {
		tmp--;
		*(char *)tmp = 0;
	}
    
    
// 如果把物理页面映射到指定线性地址的操作成功，就返回。否则就释放内存页，显示内存不够。
	if (put_page(page,address))
		return;
	free_page(page);
	oom();
}
```

地址映射参考

![image-20220605162126173](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220605162126173.png)

![image-20220605162131370](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220605162131370.png)

```c
/*
 * 下面函数将一内存页面放置在指定地址处。它返回页面的物理地址，如果
 * 内存不够(在访问页表或页面时)，则返回0。
 */
//// 把一物理内存页面映射到指定的线性地址处。
// 主要工作是在页目录和页表中设置指定页面的信息。若成功则返回页面地址。
unsigned long put_page(unsigned long page,unsigned long address)
{
	unsigned long tmp, *page_table;

/* 注意!!!这里使用了页目录基址_pg_dir=0 的条件 */

// 如果申请的页面位置低于LOW_MEM(1Mb)或超出系统实际含有内存高端HIGH_MEMORY，则发出警告。
	if (page < LOW_MEM || page >= HIGH_MEMORY)
		printk("Trying to put page %p at %p\n",page,address);
	// 如果申请的页面在内存页面映射字节图中没有置位，则显示警告信息。
	if (mem_map[(page-LOW_MEM)>>12] != 1)
		printk("mem_map disagrees with %p at %p\n",page,address);
    
	// 计算指定地址在页目录表中对应的目录项指针。注意address>>20 & 0xffc包括下面这样的位运算，就是取特定的区域的字节，& 0xffc选择值为1的字节区域
	page_table = (unsigned long *) ((address>>20) & 0xffc);
// 如果该目录项有效(P=1)(也即指定的页表在内存中)，则从中取得指定页表的地址 -> page_table。
	if ((*page_table)&1)
		page_table = (unsigned long *) (0xfffff000 & *page_table);
	else {
// 否则，申请空闲页面给页表使用，并在对应目录项中置相应标志7（User, U/S, R/W）。然后将
// 该页表的地址 -> page_table。
		if (!(tmp=get_free_page()))
			return 0;
		*page_table = tmp|7;
		page_table = (unsigned long *) tmp;
	}
    
    
	// 在页表中设置指定地址的物理内存页面的页表项内容。每个页表共可有1024 项(0x3ff)。页表项中的项包括物理地址page和其他信息|7就是把这个信息加上
	page_table[(address>>12) & 0x3ff] = page | 7;
    
    
/* 不需要刷新页变换高速缓冲 */
	return page;// 返回页面地址。
}
```





流程：执行load[addr]根据段表映射变成虚拟地址，虚拟地址访问地址页表发现没有映射，MMU发现并发出中断信息，然后执行中断处理函数_page_fault，其中调用 _do_no_page缺页处理函数，其中该函数主要做两个事情：1）读设备上一个页面的数据（4 个逻辑块）到指定物理地址page 处。2）物理页面映射到指定线性地址put_page(page,address)，填写页表。

执行完成后再次执行load[adrr]后就可以找到相应的物理地址了

![image-20220605152754133](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220605152754133.png)





[15种位运算的妙用，你都知道吗？ - 知乎 (zhihu.com)](https://zhuanlan.zhihu.com/p/54946559)
