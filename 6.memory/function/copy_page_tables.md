# 建立一个进程的段表、页表和页面的故事



从fork开始

```c
int copy_process() {
    ...
    copy_mem();
    ...
}
```









为任务分配线性地址，并建立段表映射

在线性地址中，每个任务分配的地址是64MB

```c
// 设置新任务的代码和数据段基址、限长并复制页表。
// nr 为新任务号；p 是新任务数据结构的指针。
int copy_mem (int nr, struct task_struct *p)
{
	unsigned long old_data_base, new_data_base, data_limit;
	unsigned long old_code_base, new_code_base, code_limit;

    
	code_limit = get_limit (0x0f);	// 取局部描述符表中代码段描述符项中段限长。
	data_limit = get_limit (0x17);	// 取局部描述符表中数据段描述符项中段限长。
	old_code_base = get_base (current->ldt[1]);	// 取原代码段基址。
	old_data_base = get_base (current->ldt[2]);	// 取原数据段基址。
	if (old_data_base != old_code_base)	// 0.11 版不支持代码和数据段分立的情况。
		panic ("We don't support separate I&D");
	if (data_limit < code_limit)	// 如果数据段长度 < 代码段长度也不对。
		panic ("Bad data_limit");
	new_data_base = new_code_base = nr * 0x4000000;	// 新基址=任务号*64Mb(任务大小)。
	p->start_code = new_code_base;
     //将段表项与线性地址建立联系
	set_base (p->ldt[1], new_code_base);	// 设置代码段描述符中基址域。
	set_base (p->ldt[2], new_data_base);	// 设置数据段描述符中基址域。
    
    
	if (copy_page_tables (old_data_base, new_data_base, data_limit))
    {				// 复制代码和数据段。
		free_page_tables (new_data_base, data_limit);	// 如果出错则释放申请的内存。
		return -ENOMEM;
    }
	return 0;
}
```

![图片](https://mmbiz.qpic.cn/mmbiz_png/GLeh42uInXSStwC0tMry7A2Dnm0seVNE3blIEOnr5c4icXUfyq8t0QgmhDKXlllW7t9SRpkDugyZ1Enmxtrx8Qw/640?wx_fmt=png&wxfrom=5&wx_lazy=1&wx_co=1)

最后的结果就是，每个进程自己的LDT，包含数据段和代码段。在进程切换是LDT会变





复制页表并建立物理映射

```c
int copy_page_tables(unsigned long from,unsigned long to,long size) {
     // 源地址和目的地址都需要是在4Mb 的内存边界地址上。否则出错，死机。
	if ((from&0x3fffff) || (to&0x3fffff))
		panic("copy_page_tables called with wrong alignment");
    // 取得源地址和目的地址的目录项(from_dir 和to_dir)。目录项。线性地址的前10位为目录项，左移20位*4(一个目录项4字节），得到目录项地址
	from_dir = (unsigned long *) ((from>>20) & 0xffc); /* _pg_dir = 0 */
	to_dir = (unsigned long *) ((to>>20) & 0xffc);
    
    // 计算要复制的内存块占用的页表数（也即目录项数）。
	size = ((unsigned) (size+0x3fffff)) >> 22;
    
    
    // 下面开始对每个占用的页表依次进行复制操作，页目录包含页表，要找到页表就要先找到页目录，而页目录包同一放在 _pg_dir = 0处，一个进程可能包含多个页目录，这也是外层循环的意义，要把父进程所有的页表拷贝到子进程的页表中
	for( ; size-->0 ; from_dir++,to_dir++) {
		if (1 & *to_dir)// 如果目的目录项指定的页表已经存在(P=1)，则出错，死机。
			panic("copy_page_tables: already exist");
		if (!(1 & *from_dir))// 如果此源目录项未被使用，则不用复制对应页表，跳过。
			continue;
        
        
        // 下面开始对每个占用的页表依次进行复制操作。
	for( ; size-->0 ; from_dir++,to_dir++) {
        
		if (1 & *to_dir)// 如果目的目录项指定的页表已经存在(P=1)，则出错，死机。
			panic("copy_page_tables: already exist");
		if (!(1 & *from_dir))// 如果此源目录项未被使用，则不用复制对应页表，跳过。
			continue;
		// 取当前源目录项中页表的地址 -> from_page_table。
		from_page_table = (unsigned long *) (0xfffff000 & *from_dir);
// 为目的页表取一页空闲内存，如果返回是0 则说明没有申请到空闲内存页面。返回值=-1，退出。
        
        
		if (!(to_page_table = (unsigned long *) get_free_page()))
			return -1;	/* Out of memory, see freeing */
		// 设置目的目录项信息。7 是标志信息，表示(Usr, R/W, Present)。
		*to_dir = ((unsigned long) to_page_table) | 7;
		// 针对当前处理的页表，设置需复制的页面数。如果是在内核空间，则仅需复制头160 页，
		// 否则需要复制1 个页表中的所有1024 页面。
		nr = (from==0)?0xA0:1024;
		// 对于当前页表，开始复制指定数目nr 个内存页面。
		for ( ; nr-- > 0 ; from_page_table++,to_page_table++) {
			this_page = *from_page_table;// 取源页表项内容。
			if (!(1 & this_page))// 如果当前源页面没有使用，则不用复制。
				continue;
// 复位页表项中R/W 标志(置0)。(如果U/S 位是0，则R/W 就没有作用。如果U/S 是1，而R/W 是0，
// 那么运行在用户层的代码就只能读页面。如果U/S 和R/W 都置位，则就有写的权限。)
			this_page &= ~2;
			*to_page_table = this_page;// 将该页表项复制到目的页表中。
// 如果该页表项所指页面的地址在1M 以上，则需要设置内存页面映射数组mem_map[]，于是计算
// 页面号，并以它为索引在页面映射数组相应项中增加引用次数。
			if (this_page > LOW_MEM) {
                
// 下面这句的含义是令源页表项所指内存页也为只读。因为现在开始有两个进程共用内存区了，页表映射的页面相同
// 若其中一个内存需要进行写操作，则可以通过页异常的写保护处理，为执行写操作的进程分配
// 一页新的空闲页面，也即进行写时复制的操作。
				*from_page_table = this_page;// 令源页表项也只读。
				this_page -= LOW_MEM;
				this_page >>= 12;
                //这个页面使用的数量+1，所有共享的进程终止后，这个物理页面才会释放
				mem_map[this_page]++;
			}
		}
	}
	invalidate();// 刷新页变换高速缓冲。
	return 0;
}
}
```

![图片](https://mmbiz.qpic.cn/mmbiz_png/GLeh42uInXSStwC0tMry7A2Dnm0seVNEQJ4hW13xwlRrJMdnEoBzvNcy3J2DicAPShp4mBaJRGaBJkauRr2RLBg/640?wx_fmt=png&wxfrom=5&wx_lazy=1&wx_co=1)

最后的结果是，每个进程在页目录表中都有一个或多个目录项（目录项就是页表的集合，每个目录表中可以表示该进程的4MB，但Linux给每个进程划分的线性地址为64MB），目录表中指向页表，页表的映射关系相同（子线程和父进程地址映射到同一块儿物理空间）





补充：





[一个新进程的诞生（七）透过 fork 来看进程的内存规划 (qq.com)](https://mp.weixin.qq.com/s/d2pHFSbTLb-nv2C_RfKlVA)