### \fs\read_write.c\sys_write

```c
int sys_write (unsigned int fd, char *buf, int count)
{
	struct file *file;
	struct m_inode *inode;

// 如果文件句柄值大于程序最多打开文件数NR_OPEN，或者需要写入的字节计数小于0，或者该句柄
// 的文件结构指针为空，则返回出错码并退出。
// 注意这里的file含有读写的位置和打开的inode，有了偏移量才能从知道在哪一个块中
	if (fd >= NR_OPEN || count < 0 || !(file = current->filp[fd]))
		return -EINVAL;
// 若需读取的字节数count 等于0，则返回0，退出
	if (!count)
		return 0;
// 取文件对应的i 节点。若是管道文件，并且是写管道文件模式，则进行写管道操作，若成功则返回
// 写入的字节数，否则返回出错码，退出。
	inode = file->f_inode;
	if (inode->i_pipe)
		return (file->f_mode & 2) ? write_pipe (inode, buf, count) : -EIO;
// 如果是字符型文件，则进行写字符设备操作，返回写入的字符数，退出。
	if (S_ISCHR (inode->i_mode))
		return rw_char (WRITE, inode->i_zone[0], buf, count, &file->f_pos);
// 如果是块设备文件，则进行块设备写操作，并返回写入的字节数，退出。
	if (S_ISBLK (inode->i_mode))
		return block_write (inode->i_zone[0], &file->f_pos, buf, count);
// 若是常规文件，则执行文件写操作，并返回写入的字节数，退出。
	if (S_ISREG (inode->i_mode))
		return file_write (inode, file, buf, count);
// 否则，显示对应节点的文件模式，返回出错码，退出。
	printk ("(Write)inode->i_mode=%06o\n\r", inode->i_mode);
	return -EINVAL;
}
```



### fs\file_dev.c\file_write

```c
//// 文件写函数- 根据i 节点和文件结构信息，将用户数据写入指定设备。
// 由i 节点可以知道设备号，由filp 结构可以知道文件中当前读写指针位置。buf 指定用户态中
// 缓冲区的位置，count 为需要写入的字节数。返回值是实际写入的字节数，或出错号(小于0)。
int file_write(struct m_inode * inode, struct file * filp, char * buf, int count)
{
    
	off_t pos;
	int block,c;
	struct buffer_head * bh;
	char * p;
	int i=0;
    ...
    //用file知道文件流写的字符区间，从哪儿到哪儿
    // 如果是要向文件后添加数据，则将文件读写指针移到文件尾部。否则就将在文件读写指针处写入。
	if (filp->f_flags & O_APPEND)
		pos = inode->i_size;
	else
        //上次读写的位置
		pos = filp->f_pos;
 
    
    
    
    // 若已写入字节数i 小于需要写入的字节数count，则循环执行以下操作。
	while (i<count) {
// 创建数据块号(pos/BLOCK_SIZE)在设备上对应的逻辑块，并返回在设备上的逻辑块号。如果逻辑
// 块号=0，则表示创建失败，退出循环。
		if (!(block = create_block(inode,pos/BLOCK_SIZE)))
			break;
// 根据该逻辑块号读取设备上的相应数据块，若出错则退出循环。
// 放入“电梯”队列
		if (!(bh=bread(inode->i_dev,block)))
			break;        
// 求出文件读写指针在数据块中的偏移值c，将p 指向读出数据块缓冲区中开始读取的位置。置该
// 缓冲区已修改标志。
		c = pos % BLOCK_SIZE;
		p = c + bh->b_data;
		bh->b_dirt = 1;
// 从开始读写位置到块末共可写入c=(BLOCK_SIZE-c)个字节。若c 大于剩余还需写入的字节数
// (count-i)，则此次只需再写入c=(count-i)即可。
		c = BLOCK_SIZE-c;
		if (c > count-i) c = count-i;
// 文件读写指针前移此次需写入的字节数。如果当前文件读写指针位置值超过了文件的大小，则
// 修改i 节点中文件大小字段，并置i 节点已修改标志。
		pos += c;
		if (pos > inode->i_size) {
			inode->i_size = pos;
			inode->i_dirt = 1;
		}
// 已写入字节计数累加此次写入的字节数c。从用户缓冲区buf 中复制c 个字节到高速缓冲区中p
// 指向开始的位置处。然后释放该缓冲区。
		i += c;
		while (c-->0)
			*(p++) = get_fs_byte(buf++);
		brelse(bh);

        
        

        
//// 更改文件修改时间为当前时间。
	inode->i_mtime = CURRENT_TIME;
// 如果此次操作不是在文件尾添加数据，则把文件读写指针调整到当前读写位置，并更改i 节点修改
// 时间为当前时间。
	if (!(filp->f_flags & O_APPEND)) {
		filp->f_pos = pos;
		inode->i_ctime = CURRENT_TIME;
	}
// 返回写入的字节数，若写入字节数为0，则返回出错号-1。
	return (i?i:-1);        
	}
```



### fs\inode.c\creat_block

creat_block算盘块，文件抽象的核心

```c

//// 创建文件数据块block 在设备上对应的逻辑块，并返回设备上对应的逻辑块号。
int create_block(struct m_inode * inode, int block)
{
	return _bmap(inode,block,1);
}


//// 文件数据块映射到盘块的处理操作。(block 位图处理函数，bmap - block map)
// 参数：inode – 文件的i 节点；block – 文件中的数据块号；create - 创建标志。
// 如果创建标志置位，则在对应逻辑块不存在时就申请新磁盘块。
// 返回block 数据块对应在设备上的逻辑块号（盘块号）。
static int _bmap(struct m_inode * inode,int block,int create)
{
	struct buffer_head * bh;
	int i;

// 如果块号小于0，则死机。
	if (block<0)
		panic("_bmap: block<0");
// 如果块号大于直接块数+ 间接块数+ 二次间接块数，超出文件系统表示范围，则死机。
	if (block >= 7+512+512*512)
		panic("_bmap: block>big");
// 如果该块号小于7，则使用直接块表示。
	if (block<7) {
// 如果创建标志置位，并且i 节点中对应该块的逻辑块（区段）字段为0，则向相应设备申请一磁盘
// 块（逻辑块，区块），并将盘上逻辑块号（盘块号）填入逻辑块字段中。然后设置i 节点修改时间，
// 置i 节点已修改标志。最后返回逻辑块号。
		if (create && !inode->i_zone[block])
			if (inode->i_zone[block]=new_block(inode->i_dev)) {
				inode->i_ctime=CURRENT_TIME;
				inode->i_dirt=1;
			}
        //注意inode的zone的值就是逻辑盘快在磁盘上的位置下标
		return inode->i_zone[block];
	}
// 如果该块号>=7，并且小于7+512，则说明是一次间接块。下面对一次间接块进行处理。
	block -= 7;
	if (block<512) {
// 如果是创建，并且该i 节点中对应间接块字段为0，表明文件是首次使用间接块，则需申请
// 一磁盘块用于存放间接块信息，并将此实际磁盘块号填入间接块字段中。然后设置i 节点
// 已修改标志和修改时间。
		if (create && !inode->i_zone[7])
			if (inode->i_zone[7]=new_block(inode->i_dev)) {
				inode->i_dirt=1;
				inode->i_ctime=CURRENT_TIME;
			}
// 若此时i 节点间接块字段中为0，表明申请磁盘块失败，返回0 退出。
		if (!inode->i_zone[7])
			return 0;
// 读取设备上的一次间接块。
		if (!(bh = bread(inode->i_dev,inode->i_zone[7])))
			return 0;
// 取该间接块上第block 项中的逻辑块号（盘块号）。
		i = ((unsigned short *) (bh->b_data))[block];
// 如果是创建并且间接块的第block 项中的逻辑块号为0 的话，则申请一磁盘块（逻辑块），并让
// 间接块中的第block 项等于该新逻辑块块号。然后置位间接块的已修改标志。
		if (create && !i)
			if (i=new_block(inode->i_dev)) {
				((unsigned short *) (bh->b_data))[block]=i;
				bh->b_dirt=1;
			}
// 最后释放该间接块，返回磁盘上新申请的对应block 的逻辑块的块号。
		brelse(bh);
		return i;
	}
// 程序运行到此，表明数据块是二次间接块，处理过程与一次间接块类似。下面是对二次间接块的处理。
// 将block 再减去间接块所容纳的块数(512)。
	block -= 512;
// 如果是新创建并且i 节点的二次间接块字段为0，则需申请一磁盘块用于存放二次间接块的一级块
// 信息，并将此实际磁盘块号填入二次间接块字段中。之后，置i 节点已修改编制和修改时间。
	if (create && !inode->i_zone[8])
		if (inode->i_zone[8]=new_block(inode->i_dev)) {
			inode->i_dirt=1;
			inode->i_ctime=CURRENT_TIME;
		}
// 若此时i 节点二次间接块字段为0，表明申请磁盘块失败，返回0 退出。
	if (!inode->i_zone[8])
		return 0;
// 读取该二次间接块的一级块。
	if (!(bh=bread(inode->i_dev,inode->i_zone[8])))
		return 0;
// 取该二次间接块的一级块上第(block/512)项中的逻辑块号。
	i = ((unsigned short *)bh->b_data)[block>>9];
// 如果是创建并且二次间接块的一级块上第(block/512)项中的逻辑块号为0 的话，则需申请一磁盘
// 块（逻辑块）作为二次间接块的二级块，并让二次间接块的一级块中第(block/512)项等于该二级
// 块的块号。然后置位二次间接块的一级块已修改标志。并释放二次间接块的一级块。
	if (create && !i)
		if (i=new_block(inode->i_dev)) {
			((unsigned short *) (bh->b_data))[block>>9]=i;
			bh->b_dirt=1;
		}
	brelse(bh);
// 如果二次间接块的二级块块号为0，表示申请磁盘块失败，返回0 退出。
	if (!i)
		return 0;
// 读取二次间接块的二级块。
	if (!(bh=bread(inode->i_dev,i)))
		return 0;
// 取该二级块上第block 项中的逻辑块号。(与上511 是为了限定block 值不超过511)
	i = ((unsigned short *)bh->b_data)[block&511];
// 如果是创建并且二级块的第block 项中的逻辑块号为0 的话，则申请一磁盘块（逻辑块），作为
// 最终存放数据信息的块。并让二级块中的第block 项等于该新逻辑块块号(i)。然后置位二级块的
// 已修改标志。
	if (create && !i)
		if (i=new_block(inode->i_dev)) {
			((unsigned short *) (bh->b_data))[block&511]=i;
			bh->b_dirt=1;
		}
// 最后释放该二次间接块的二级块，返回磁盘上新申请的对应block 的逻辑块的块号。
	brelse(bh);
	return i;
}
```







### 参考资料

[Linux0.11缓冲区机制详解 - 知乎 (zhihu.com)](https://zhuanlan.zhihu.com/p/57223746)