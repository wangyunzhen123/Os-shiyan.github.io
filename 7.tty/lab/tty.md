## 实验任务

实验任务：修改 `Linux 0.11` 的终端设备处理代码，对键盘输入和字符显示进行非常规的控制。在初始状态，一切如常。用户按一次`F12` 后，把应用程序向终端输出所有字母都替换为`“*”`。用户再按一次`F12`，又恢复正常。第三次按 `F12`，再进行输出替换。依此类推。





## 实现



### 修改kernel/chr_drv/keyboard.S文件

修改key_table代码如下

在key_table中每个按键都会跳到一个相应的处理函数，只要把该函数的功能写出即可

```c
/* .long func,none,none,none		58-5B f12 ? ? ? */
.long press_f12_handle,none,none,none

```



### 修改kernel/chr_drv/tty_io.c文件

实现f12的开关功能

```c
int switch_show_char_flag = 0;
void press_f12_handle(void)
{
	if (switch_show_char_flag == 0)
	{
		switch_show_char_flag = 1;
	}
	else if (switch_show_char_flag == 1)
	{
		switch_show_char_flag = 0;
	}
}

```





### 修改/include/linux/tty.h文件

函数声明添加

```c
extern int switch_show_char_flag;
void press_f12_handle(void);
```



### 修改linux-0.11/kernel/chr_drv/console.c文件，修改con_write函数

最终会显示c中的内容，故当功能开启（第一次按下f12时），将所有的值都变为*即可

```c
case 0:
				if (c>31 && c<127) {
					if (x>=video_num_columns) {
						x -= video_num_columns;
						pos -= video_size_row;
						lf();
					}

  				if (switch_show_char_flag == 1)
  				{
  					if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9'))
  						c = '*';
  				}
					
					__asm__("movb attr,%%ah\n\t"
						"movw %%ax,%1\n\t"
						::"a" (c),"m" (*(short *)pos)
						);
					pos += 2;
					x++;

```





## 实验结果

按下f12后，所有字符都变*

![image-20220607163056815](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220607163056815.png)



再次按下f12后，恢复原样

![image-20220607163206776](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220607163206776.png)



## 参考资料

[(9条消息) 操作系统实验八 终端设备的控制(哈工大李治军)_Casten-Wang的博客-CSDN博客](https://blog.csdn.net/leoabcd12/article/details/121816239)







