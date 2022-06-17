![image-20220606154048303](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220606154048303.png)





### 重要数据结构

```c
struct task_struct
{
    ...
	struct file *filp[NR_OPEN];
    ...

};

// 文件结构（用于在文件句柄与i 节点之间建立关系）
struct file
{
  unsigned short f_mode;	// 文件操作模式（RW 位）
  unsigned short f_flags;	// 文件打开和控制的标志。
  unsigned short f_count;	// 对应文件句柄（文件描述符）数。
  struct m_inode *f_inode;	// 指向对应i 节点。
  off_t f_pos;			// 文件位置（读写偏移值）。
};


// 磁盘上的索引节点(i 节点)数据结构。
struct d_inode
{
  unsigned short i_mode;	// 文件类型和属性(rwx 位)。
  unsigned short i_uid;		// 用户id（文件拥有者标识符）。
  unsigned long i_size;		// 文件大小（字节数）。
  unsigned long i_time;		// 修改时间（自1970.1.1:0 算起，秒）。
  unsigned char i_gid;		// 组id(文件拥有者所在的组)。
  unsigned char i_nlinks;	// 链接数（多少个文件目录项指向该i 节点）。
  unsigned short i_zone[9];	// 直接(0-6)、间接(7)或双重间接(8)逻辑块号。
// zone 是区的意思，可译成区段，或逻辑块。
};

// 这是在内存中的i 节点结构。前7 项与d_inode 完全一样。
struct m_inode
{
  unsigned short i_mode;	// 文件类型和属性(rwx 位)。
  unsigned short i_uid;		// 用户id（文件拥有者标识符）。
  unsigned long i_size;		// 文件大小（字节数）。
  unsigned long i_mtime;	// 修改时间（自1970.1.1:0 算起，秒）。
  unsigned char i_gid;		// 组id(文件拥有者所在的组)。
  unsigned char i_nlinks;	// 文件目录项链接数。
  unsigned short i_zone[9];	// 直接(0-6)、间接(7)或双重间接(8)逻辑块号。
/* these are in memory also */
  struct task_struct *i_wait;	// 等待该i 节点的进程。
  unsigned long i_atime;	// 最后访问时间。
  unsigned long i_ctime;	// i 节点自身修改时间。
  unsigned short i_dev;		// i 节点所在的设备号。
  unsigned short i_num;		// i 节点号。
  unsigned short i_count;	// i 节点被使用的次数，0 表示该i 节点空闲。
  unsigned char i_lock;		// 锁定标志。
  unsigned char i_dirt;		// 已修改(脏)标志。
  unsigned char i_pipe;		// 管道标志。
  unsigned char i_mount;	// 安装标志。
  unsigned char i_seek;		// 搜寻标志(lseek 时)。
  unsigned char i_update;	// 更新标志。
};



//Linux/fs.h

// 文件目录项结构。
struct dir_entry
{
  unsigned short inode;		// i 节点。
  char name[NAME_LEN];		// 文件名。
};
```





### 初始化，挂载根目录

```c
void main(void) {
    ...
    move_to_user_mode();
    if (!fork()) {
        init();
    }
    for(;;) pause();
}

void init(void) {
    setup((void *) &drive_info);
    ...
}



//kernel/hd.c
int sys_setup(void * BIOS) {
    ...
    mount_root();
}


//// 安装根文件系统。
// 该函数是在系统开机初始化设置时(sys_setup())调用的。( kernel/blk_drv/hd.c, 157 )
struct m_inode * mi;
void mount_root(void) {
    //就是读取硬盘的超级块信息到内存中来
    p=read_super(0);
    ...
    //读取根 inode 信息
    mi=iget(0,1);
    ...
    //然后把该 inode 设置为当前进程（也就是进程 1）的当前工作目录和根目录
    current->pwd = mi;
    current->root = mi;
    ...
   // 然后根据逻辑块位图中相应比特位的占用情况统计出空闲块数。这里宏函数set_bit()只是在测试
  // 比特位，而非设置比特位。"i&8191"用于取得i 节点号在当前块中的偏移值。"i>>13"是将i 除以
  // 8192，也即除一个磁盘块包含的比特位数。
     i=p->s_nzones;
    while (-- i >= 0)
    set_bit(i&8191, p->s_zmap[i>>13]->b_data);
    
    // 然后根据i 节点位图中相应比特位的占用情况计算出空闲i 节点数
     i=p->s_ninodes+1;
    while (-- i >= 0)
        set_bit(i&8191, p->s_imap[i>>13]->b_data);
}

```

```c
// 内存中磁盘超级块结构。
struct super_block
{
  unsigned short s_ninodes;	// 节点数。
  unsigned short s_nzones;	// 逻辑块数。
  unsigned short s_imap_blocks;	// i 节点位图所占用的数据块数。
  unsigned short s_zmap_blocks;	// 逻辑块位图所占用的数据块数。
  unsigned short s_firstdatazone;	// 第一个数据逻辑块号。
  unsigned short s_log_zone_size;	// log(数据块数/逻辑块)。（以2 为底）。
  unsigned long s_max_size;	// 文件最大长度。
  unsigned short s_magic;	// 文件系统魔数。
/* These are only in memory */
  struct buffer_head *s_imap[8];	// i 节点位图缓冲块指针数组(占用8 块，可表示64M)。
  struct buffer_head *s_zmap[8];	// 逻辑块位图缓冲块指针数组（占用8 块）。
  unsigned short s_dev;		// 超级块所在的设备号。
  struct m_inode *s_isup;	// 被安装的文件系统根目录的i 节点。(isup-super i)
  struct m_inode *s_imount;	// 被安装到的i 节点。
  unsigned long s_time;		// 修改时间。
  struct task_struct *s_wait;	// 等待该超级块的进程。
  unsigned char s_lock;		// 被锁定标志。
  unsigned char s_rd_only;	// 只读标志。
  unsigned char s_dirt;		// 已修改(脏)标志。
};
```



![image-20220606170246157](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220606170246157.png)



初始化之后，通过fork创建新进程后，每个进程PCB中的root，即根目录都为mi了（第一个inode索引结点）







### 打开文件函数Linux/fs/open.c/sys_open

```c
//// 打开（或创建）文件系统调用函数。
// 参数filename 是文件名，flag 是打开文件标志：只读O_RDONLY、只写O_WRONLY 或读写O_RDWR，
// 以及O_CREAT、O_EXCL、O_APPEND 等其它一些标志的组合，若本函数创建了一个新文件，则mode
// 用于指定使用文件的许可属性，这些属性有S_IRWXU(文件宿主具有读、写和执行权限)、S_IRUSR
// (用户具有读文件权限)、S_IRWXG(组成员具有读、写和执行权限)等等。对于新创建的文件，这些
// 属性只应用于将来对文件的访问，创建了只读文件的打开调用也将返回一个可读写的文件句柄。
// 若操作成功则返回文件句柄(文件描述符)，否则返回出错码。(参见sys/stat.h, fcntl.h)
int sys_open(const char * filename,int flag,int mode)
{
	struct m_inode * inode;
	struct file * f;
	int i,fd;

// 将用户设置的模式与进程的模式屏蔽码相与，产生许可的文件模式。
	mode &= 0777 & ~current->umask;
    
    
// 搜索进程结构中文件结构指针数组，查找一个空闲项，若已经没有空闲项，则返回出错码。
	for(fd=0 ; fd<NR_OPEN ; fd++)
		if (!current->filp[fd])
			break;
	if (fd>=NR_OPEN)
		return -EINVAL;
    
    
   
// 设置执行时关闭文件句柄位图，复位对应比特位。
	current->close_on_exec &= ~(1<<fd);
    
    
    
// 搜索进程结构中文件结构指针数组，查找一个空闲项，若已经没有空闲项，则返回出错码。
	for(fd=0 ; fd<NR_OPEN ; fd++)
		if (!current->filp[fd])
			break;
	if (fd>=NR_OPEN)
		return -EINVAL;
// 设置执行时关闭文件句柄位图，复位对应比特位。
	current->close_on_exec &= ~(1<<fd);
// 令f 指向文件表数组开始处。搜索空闲文件结构项(句柄引用计数为0 的项)，若已经没有空闲
// 文件表结构项，则返回出错码。
	f=0+file_table;
	for (i=0 ; i<NR_FILE ; i++,f++)
		if (!f->f_count) break;
	if (i>=NR_FILE)
		return -EINVAL;
// 让进程的对应文件句柄的文件结构指针指向搜索到的文件结构，并令句柄引用计数递增1。
	(current->filp[fd]=f)->f_count++;
    
    

// 调用函数执行打开操作，若返回值小于0，则说明出错，释放刚申请到的文件结构，返回出错码。
	if ((i=open_namei(filename,flag,mode,&inode))<0) {
		current->filp[fd]=NULL;
		f->f_count=0;
		return i;
	}
    
    
    
    
/* ttys 有些特殊（ttyxx 主号==4，tty 主号==5）*/
// 如果是字符设备文件，那么如果设备号是4 的话，则设置当前进程的tty 号为该i 节点的子设备号。
// 并设置当前进程tty 对应的tty 表项的父进程组号等于进程的父进程组号。
	if (S_ISCHR(inode->i_mode))
		if (MAJOR(inode->i_zone[0])==4) {
			if (current->leader && current->tty<0) {
				current->tty = MINOR(inode->i_zone[0]);
				tty_table[current->tty].pgrp = current->pgrp;
			}
// 否则如果该字符文件设备号是5 的话，若当前进程没有tty，则说明出错，释放i 节点和申请到的
// 文件结构，返回出错码。
		} else if (MAJOR(inode->i_zone[0])==5)
			if (current->tty<0) {
				iput(inode);
				current->filp[fd]=NULL;
				f->f_count=0;
				return -EPERM;
			}
/* 同样对于块设备文件：需要检查盘片是否被更换 */
// 如果打开的是块设备文件，则检查盘片是否更换，若更换则需要是高速缓冲中对应该设备的所有
// 缓冲块失效。
	if (S_ISBLK(inode->i_mode))
		check_disk_change(inode->i_zone[0]);
// 初始化文件结构。置文件结构属性和标志，置句柄引用计数为1，设置i 节点字段，文件读写指针
// 初始化为0。返回文件句柄。
	f->f_mode = inode->i_mode;
	f->f_flags = flag;
	f->f_count = 1;
	f->f_inode = inode;
	f->f_pos = 0;
	return (fd);
} 
   
```





### 解析目录 Linux/fs/namei.c/open_namei

```c
/*
 *	open_namei()
 *
 * open()所使用的namei 函数- 这其实几乎是完整的打开文件程序。
 */
//// 文件打开namei 函数。
// 参数：pathname - 文件路径名；flag - 文件打开标志；mode - 文件访问许可属性；
// 返回：成功返回0，否则返回出错码；res_inode - 返回的对应文件路径名的的i 节点指针。
int open_namei(const char * pathname, int flag, int mode,
	struct m_inode ** res_inode)
{
    ...
// 根据路径名寻找到对应的i 节点，以及最顶端文件名及其长度。
	if (!(dir = dir_namei(pathname,&namelen,&basename)))
		return -ENOENT;
    ...
} 


/*
 *	dir_namei()
 *
 * dir_namei()函数返回指定目录名的i 节点指针，以及在最顶层目录的名称。
 */
// 参数：pathname - 目录路径名；namelen - 路径名长度。
// 返回：指定目录名最顶层目录的i 节点指针和最顶层目录名及其长度。
static struct m_inode * dir_namei(const char * pathname,
	int * namelen, const char ** name)
{
	char c;
	const char * basename;
	struct m_inode * dir;

// 取指定路径名最顶层目录的i 节点，若出错则返回NULL，退出。
	if (!(dir = get_dir(pathname)))
		return NULL;
....
	return dir;
}




/*
 *	get_dir()
 *
 * 该函数根据给出的路径名进行搜索，直到达到最顶端的目录。
 * 如果失败则返回NULL。
 */
//// 搜寻指定路径名的目录。
// 参数：pathname - 路径名。
// 返回：目录的i 节点指针。失败时返回NULL。
static struct m_inode * get_dir(const char * pathname)
{
	char c;
	const char * thisname;
	struct m_inode * inode;
	struct buffer_head * bh;
	int namelen,inr,idev;
	struct dir_entry * de;
......
// 如果用户指定的路径名的第1 个字符是'/'，则说明路径名是绝对路径名。则从根i 节点开始操作。
	if ((c=get_fs_byte(pathname))=='/') {
		inode = current->root;
		pathname++;
// 否则若第一个字符是其它字符，则表示给定的是相对路径名。应从进程的当前工作目录开始操作。
// 则取进程当前工作目录的i 节点。
	} else if (c)
		inode = current->pwd;
......
// 将取得的i 节点引用计数增1。
	inode->i_count++;
	while (1) {
......
// 调用查找指定目录和文件名的目录项函数，在当前处理目录中寻找子目录项。如果没有找到，
// 则释放该i 节点，并返回NULL，退出。
		if (!(bh = find_entry(&inode,thisname,namelen,&de))) {
			iput(inode);
			return NULL;
		}
.....
// 取节点号inr 的i 节点信息，若失败，则返回NULL，退出。否则继续以该子目录的i 节点进行操作。
		if (!(inode = iget(idev,inr)))
			return NULL;
	}
}
```

![image-20220606164253513](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220606164253513.png)





### 获取目录项\fs\namei.c

```c
static struct buffer_head * find_entry(struct m_inode ** dir,
	const char * name, int namelen, struct dir_entry ** res_dir)
{
    
    ...
// 计算本目录中目录项项数entries。置空返回目录项结构指针。
	entries = (*dir)->i_size / (sizeof (struct dir_entry));
    ...
// 如果该i 节点所指向的第一个直接磁盘块号为0，则返回NULL，退出。
	if (!(block = (*dir)->i_zone[0]))
		return NULL;
// 从节点所在设备读取指定的目录项数据块，如果不成功，则返回NULL，退出。
	if (!(bh = bread((*dir)->i_dev,block)))
		return NULL;
// 在目录项数据块中搜索匹配指定文件名的目录项，首先让de 指向数据块，并在不超过目录中目录项数
// 的条件下，循环执行搜索。
	i = 0;
	de = (struct dir_entry *) bh->b_data;
	while (i < entries) {
// 如果当前目录项数据块已经搜索完，还没有找到匹配的目录项，则释放当前目录项数据块。
		if ((char *)de >= BLOCK_SIZE+bh->b_data) {
			brelse(bh);
			bh = NULL;
// 在读入下一目录项数据块。若这块为空，则只要还没有搜索完目录中的所有目录项，就跳过该块，
// 继续读下一目录项数据块。若该块不空，就让de 指向该目录项数据块，继续搜索。
			if (!(block = bmap(*dir,i/DIR_ENTRIES_PER_BLOCK)) ||
			    !(bh = bread((*dir)->i_dev,block))) {
				i += DIR_ENTRIES_PER_BLOCK;
				continue;
			}
			de = (struct dir_entry *) bh->b_data;
		}
// 如果找到匹配的目录项的话，则返回该目录项结构指针和该目录项数据块指针，退出。
		if (match(namelen,name,de)) {
			*res_dir = de;
			return bh;
		}
// 否则继续在目录项数据块中比较下一个目录项。
		de++;
		i++;
	}
// 若指定目录中的所有目录项都搜索完还没有找到相应的目录项，则释放目录项数据块，返回NULL。
	brelse(bh);
	return NULL;
   
}
```







### 获取inode结点linux0.11\fs\inode.c\iget

```c
//// 从设备上读取指定节点号的i 节点。
// nr - i 节点号。
struct m_inode * iget(int dev,int nr)
{
	struct m_inode * inode, * empty;
   ...
// 从i 节点表中取一个空闲i 节点。
	empty = get_empty_inode();
// 扫描i 节点表。寻找指定节点号的i 节点。并递增该节点的引用次数。
	inode = inode_table;
	while (inode < NR_INODE+inode_table) {
// 如果当前扫描的i 节点的设备号不等于指定的设备号或者节点号不等于指定的节点号，则继续扫描。
		if (inode->i_dev != dev || inode->i_num != nr) {
			inode++;
			continue;
		}
// 找到指定设备号和节点号的i 节点，等待该节点解锁（如果已上锁的话）。
		wait_on_inode(inode);
// 在等待该节点解锁的阶段，节点表可能会发生变化，所以再次判断，如果发生了变化，则再次重新
// 扫描整个i 节点表。
		if (inode->i_dev != dev || inode->i_num != nr) {
			inode = inode_table;
			continue;
		}
// 将该i 节点引用计数增1。
		inode->i_count++;
		if (inode->i_mount) {
			int i;

    inode->i_dev = dev;
	inode->i_num = nr;
	read_inode(inode);
	return inode;
}
```



```c
//// 从设备上读取指定i 节点的信息到内存中（缓冲区中）。
static void read_inode(struct m_inode * inode)
{
	struct super_block * sb;
	struct buffer_head * bh;
	int block;

// 首先锁定该i 节点，取该节点所在设备的超级块。
	lock_inode(inode);
	if (!(sb=get_super(inode->i_dev)))
		panic("trying to read inode without dev");
// 该i 节点所在的逻辑块号= (启动块+超级块) + i 节点位图占用的块数+ 逻辑块位图占用的块数+
// (i 节点号-1)/每块含有的i 节点数。
	block = 2 + sb->s_imap_blocks + sb->s_zmap_blocks +
		(inode->i_num-1)/INODES_PER_BLOCK;
// 从设备上读取该i 节点所在的逻辑块，并将该inode 指针指向对应i 节点信息。
	if (!(bh=bread(inode->i_dev,block)))
		panic("unable to read i-node block");
	*(struct d_inode *)inode =
		((struct d_inode *)bh->b_data)
			[(inode->i_num-1)%INODES_PER_BLOCK];
// 最后释放读入的缓冲区，并解锁该i 节点。
	brelse(bh);
	unlock_inode(inode);
}
```













[第32回 | 加载根文件系统 (qq.com)](https://mp.weixin.qq.com/s/ruFNEgIBzlM5H-G9XR0ctw)