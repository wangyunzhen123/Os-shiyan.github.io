

procfs介绍
正式的 Linux 内核实现了 procfs，它是一个虚拟文件系统，通常被 mount（挂载） 到 /proc 目录上，通过虚拟文件和虚拟目录的方式提供访问系统参数的机会，所以有人称它为 “了解系统信息的一个窗口”。

这些虚拟的文件和目录并没有真实地存在在磁盘上，而是内核中各种数据的一种直观表示。虽然是虚拟的，但它们都可以通过标准的系统调用（open()、read() 等）访问。

例如，/proc/meminfo 中包含内存使用的信息，可以用 cat 命令显示其内容：

![image-20220101134655104](https://img-blog.csdnimg.cn/img_convert/b4a691877b2ee1e129898008d2e75a8d.png)



## 1.修改include/sys/stat.h文件

增加新文件类型，在此文件内新增*proc*文件的宏定义以及测试宏。

```c

//已有的宏定义
#define S_IFMT 00170000 //文件类型(都是8进制表示)
#define S_IFREG 0100000	//普通文件
#define S_IFCHAR 0020000 //字符设备文件
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG) //测试m是否是普通文件
#define S_ISCHAR(m) (((m) & S_IFMT) == S_IFCHAR) //测试m是否是字符设备文件

//proc文件的宏定义/宏函数,新加的
#define S_IFPROC 0030000
#define S_ISPROC(m) (((m) & S_IFMT) ==  S_IFPROC) //测试m是否是proc文件
```

![image-20220617152624221](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220617152624221.png)



## 2.修改namei.c文件

文件/proc/psinfo以及/proc/hdinfo索引节点需要通过mknod()系统调用建立，这里需要让它支持新的文件类型。可直接修改fs/namei.c文件中的sys_mknod()函数的一行代码，在其中增加关于proc文件系统的判断:

```c
if (S_ISBLK(mode) || S_ISCHR(mode) || S_ISPROC(mode))
     inode->i_zone[0] = dev;
// 文件系统初始化
inode->i_mtime = inode->i_atime = CURRENT_TIME;
inode->i_dirt = 1;
bh = add_entry(dir,basename,namelen,&de);
```

![image-20220617160959627](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220617160959627.png)

## 3.修改init/main.c文件

main（）函数在init后直接挂载了根文件系统，挂载之后就可以创建proc文件了，首先创建/proc文件目录，然后再建立该目录下的各个proc文件节点。在建立这些节点和目录时需要调用系统调用mkdir和mknod，因为初始化时在用户态了，所以不能直接调用，必须在初始化代码所在的文件中实现这两个系统调用的用户态接口。修改init/main.c，新增两个系统调用用户接口并接着修改init函数实现对其的调用：

```c

static inline _syscall0(int,fork)
static inline _syscall0(int,pause)
static inline _syscall1(int,setup,void *,BIOS)
static inline _syscall0(int,sync)
/*新增mkdir和mknode系统调用*/
_syscall2(int,mkdir,const char*,name,mode_t,mode)
_syscall3(int,mknod,const char *,filename,mode_t,mode,dev_t,dev)
    
//.......   
    
	setup((void *) &drive_info);
	(void) open("/dev/tty0",O_RDWR,0);
	(void) dup(0);
	(void) dup(0);
	mkdir("/proc",0755);
	mknod("/proc/psinfo",S_IFPROC|0444,0);
	mknod("/proc/hdinfo",S_IFPROC|0444,1);
	mknod("/proc/inodeinfo",S_IFPROC|0444,2);
```





![image-20220617153303950](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220617153303950.png)









![image-20220617153210526](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220617153210526.png)









mkdir()时mode参数的值可以是“0755”（rwxr-xr-x），表示只允许root用户改写此目录，其它人只能进入和读取此目录。
procfs是一个只读文件系统，所以用mknod()建立psinfo结点时，必须通过mode参数将其设为只读。建议使用S_IFPROC|0444做为mode值，表示这是一个proc文件，权限为0444（r--r--r--），对所有用户只读。
mknod()的第三个参数dev用来说明结点所代表的设备编号。对于procfs来说，此编号可以完全自定义。proc文件的处理函数将通过这个编号决定对应文件包含的信息是什么。例如，可以把0对应psinfo，1对应hdinfo，2对应inodeinfo。
现在可以重新编译运行系统，使用*ll /proc*可观察到下面的结果：

![img](https://img2020.cnblogs.com/blog/1992812/202006/1992812-20200603113948060-2087861151.png)

这些信息说明内核在对 psinfo 进行读操作时不能正确处理，向 cat 返回了 EINVAL 错误。因为还没有实现处理函数，所以这是很正常的。这些信息至少说明，psinfo被正确open()了。所以我们不需要对sys_open()动任何手脚，唯一要打补丁的，是sys_read()。





## 4.修改fs/read_write.c文件

为了让proc文件可读，修改fs/read_write.c添加extern，表示proc_read函数是从外部调用的。

```c
/*新增proc_read函数外部调用*/
extern int proc_read(int dev,char* buf,int count,unsigned long *pos);
```









## 5.新增/fs/proc.c文件

proc文件的处理函数的功能是根据设备编号，把不同的内容写入到用户空间的buf。写入的数据要从 f_pos 指向的位置开始，每次最多写count个字节，并根据实际写入的字节数调整 f_pos 的值，最后返回实际写入的字节数。当设备编号表明要读的是psinfo的内容时，就要按照 psinfo 的形式组织数据。在fs目录下新增proc.c文件，文件信息如下：

```c
#include <linux/kernel.h>
#include <linux/sched.h>
#include <asm/segment.h>
#include <linux/fs.h>
#include <stdarg.h>
#include <unistd.h>

#define set_bit(bitnr,addr) ({ \
register int __res ; \
__asm__("bt %2,%3;setb %%al":"=a" (__res):"a" (0),"r" (bitnr),"m" (*(addr))); \
__res; })

char proc_buf[4096] ={'\0'};

extern int vsprintf(char * buf, const char * fmt, va_list args);

//Linux0.11没有sprintf()，该函数是用于输出结果到字符串中的，所以就实现一个，这里是通过vsprintf()实现的。
int sprintf(char *buf, const char *fmt, ...)
{
	va_list args; int i;
	va_start(args, fmt);
	i=vsprintf(buf, fmt, args);
	va_end(args);
	return i;
}

int get_psinfo()
{
	int read = 0;
	read += sprintf(proc_buf+read,"%s","pid\tstate\tfather\tcounter\tstart_time\n");
	struct task_struct **p;
	for(p = &FIRST_TASK ; p <= &LAST_TASK ; ++p)
 	if (*p != NULL)
 	{
 		read += sprintf(proc_buf+read,"%d\t",(*p)->pid);
 		read += sprintf(proc_buf+read,"%d\t",(*p)->state);
 		read += sprintf(proc_buf+read,"%d\t",(*p)->father);
 		read += sprintf(proc_buf+read,"%d\t",(*p)->counter);
 		read += sprintf(proc_buf+read,"%d\n",(*p)->start_time);
 	}
 	return read;
}

/*
*  参考fs/super.c mount_root()函数
*/
int get_hdinfo()
{
	int read = 0;
	int i,used;
	struct super_block * sb;
	sb=get_super(0x301);  /*磁盘设备号 3*256+1*/
	/*Blocks信息*/
	read += sprintf(proc_buf+read,"Total blocks:%d\n",sb->s_nzones);
	used = 0;
	i=sb->s_nzones;
	while(--i >= 0)
	{
		if(set_bit(i&8191,sb->s_zmap[i>>13]->b_data))
			used++;
	}
	read += sprintf(proc_buf+read,"Used blocks:%d\n",used);
	read += sprintf(proc_buf+read,"Free blocks:%d\n",sb->s_nzones-used);
	/*Inodes 信息*/
	read += sprintf(proc_buf+read,"Total inodes:%d\n",sb->s_ninodes);
	used = 0;
	i=sb->s_ninodes+1;
	while(--i >= 0)
	{
		if(set_bit(i&8191,sb->s_imap[i>>13]->b_data))
			used++;
	}
	read += sprintf(proc_buf+read,"Used inodes:%d\n",used);
	read += sprintf(proc_buf+read,"Free inodes:%d\n",sb->s_ninodes-used);
 	return read;
}

int get_inodeinfo()
{
	int read = 0;
	int i;
	struct super_block * sb;
	struct m_inode *mi;
	sb=get_super(0x301);  /*磁盘设备号 3*256+1*/
	i=sb->s_ninodes+1;
	i=0;
	while(++i < sb->s_ninodes+1)
	{
		if(set_bit(i&8191,sb->s_imap[i>>13]->b_data))
		{
			mi = iget(0x301,i);
			read += sprintf(proc_buf+read,"inr:%d;zone[0]:%d\n",mi->i_num,mi->i_zone[0]);
			iput(mi);
		}
		if(read >= 4000) 
		{
			break;
		}
	}
 	return read;
}

int proc_read(int dev, unsigned long * pos, char * buf, int count)
{
	
 	int i;
	if(*pos % 1024 == 0)
	{
		if(dev == 0)
			get_psinfo();
		if(dev == 1)
			get_hdinfo();
		if(dev == 2)
			get_inodeinfo();
	}
 	for(i=0;i<count;i++)
 	{
 		if(proc_buf[i+ *pos ] == '\0')  
          break; 
 		put_fs_byte(proc_buf[i+ *pos],buf + i+ *pos);
 	}
 	*pos += i;
 	return i;
}
```





## 6.修改fs/Makefile文件

```bash
OBJS=	open.o read_write.o inode.o file_table.o buffer.o super.o \
	block_dev.o char_dev.o file_dev.o stat.o exec.o pipe.o namei.o \
	bitmap.o fcntl.o ioctl.o truncate.o proc.o
//......
### Dependencies:
proc.o : proc.c ../include/linux/kernel.h ../include/linux/sched.h \
  ../include/linux/head.h ../include/linux/fs.h ../include/sys/types.h \
  ../include/linux/mm.h ../include/signal.h ../include/asm/segment.h
```





## 7.实验结果

![image-20220617162117090](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220617162117090.png)



![image-20220617161800592](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220617161800592.png)

![image-20220617162144645](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220617162144645.png)



### 参考资料

[(9条消息) 操作系统实验九 proc文件系统的实现(哈工大李治军)_Casten-Wang的博客-CSDN博客_proc文件系统的实现](https://blog.csdn.net/leoabcd12/article/details/122268183)

[操作系统实验08-proc文件系统的实现 - mirage_mc - 博客园 (cnblogs.com)](https://www.cnblogs.com/mirage-mc/p/13036570.html)