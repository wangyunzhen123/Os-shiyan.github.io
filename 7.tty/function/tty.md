## 键盘中断



![image-20220605210305081](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220605210305081.png)


![image-20220605210316318](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220605210316318.png)





下面以按下F12功能键流程：



###  keyboard.S / _keyboard_interrupt（76行）

![image-20220605201239152](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220605201239152.png)

```assembly
//// 键盘中断int 21h处理程序入口点。
_keyboard_interrupt:
	push eax
	push ebx
	push ecx
	push edx
	push ds
	push es
	mov eax,10h // 将ds、es 段寄存器置为内核数据段。
	mov ds,ax
	mov es,ax
	xor al,al /* %eax is scan code */ /* eax 中是扫描码 */
	in al,60h // 读取扫描码->al。
	cmp al,0e0h // 该扫描码是0xe0 吗？如果是则跳转到设置e0 标志代码处。
	je set_e0
	cmp al,0e1h // 扫描码是0xe1 吗？如果是则跳转到设置e1 标志代码处。
	je set_e1
	call key_table[eax*4] // 调用键处理程序ker_table + eax * 4（参见下面502 行）。
	mov e0,0 // 复位e0 标志。
// 下面这段代码(55-65 行)是针对使用8255A 的PC 标准键盘电路进行硬件复位处理。端口0x61 是
// 8255A 输出口B 的地址，该输出端口的第7 位(PB7)用于禁止和允许对键盘数据的处理。
// 这段程序用于对收到的扫描码做出应答。方法是首先禁止键盘，然后立刻重新允许键盘工作。
e0_e1: 
	in al,61h // 取PPI 端口B 状态，其位7 用于允许/禁止(0/1)键盘。
	jmp l1 // 延迟一会。
l1: jmp l2
l2: or al,80h // al 位7 置位(禁止键盘工作)。
	jmp l3 // 再延迟一会。
l3: jmp l4
l4: out 61h,al // 使PPI PB7 位置位。
	jmp l5 // 延迟一会。
l5: jmp l6
l6: and al,7Fh // al 位7 复位。
	out 61h,al // 使PPI PB7 位复位（允许键盘工作）。
	mov al,20h // 向8259 中断芯片发送EOI(中断结束)信号。
	out 20h,al
	push 0 // 控制台tty 号=0，作为参数入栈。
	call _do_tty_interrupt // 将收到的数据复制成规范模式数据并存放在规范字符缓冲队列中。
	add esp,4 // 丢弃入栈的参数，弹出保留的寄存器，并中断返回。
	pop es
	pop ds
	pop edx
	pop ecx
	pop ebx
	pop eax
	iretd
set_e0: 
	mov e0,1 // 收到扫描前导码0xe0 时，设置e0 标志（位0）。
	jmp e0_e1
set_e1: 
	mov e0,2 // 收到扫描前导码0xe1 时，设置e1 标志（位1）。
	jmp e0_e1
```





call key_table[eax*4] 

###  keyboard.S / key_table（610行）

每个按键都会对于一段处理程序

```assembly
*/
/* 下面是一张子程序地址跳转表。当取得扫描码后就根据此表调用相应的扫描码处理子程序。
* 大多数调用的子程序是do_self，或者是none，这起决于是按键(make)还是释放键(break)。
*/
typedef void (*kf)();
kf key_table[]={
 none,   do_self,do_self,do_self, /* 00-03 s0 esc 1 2 */
 do_self,do_self,do_self,do_self, /* 04-07 3 4 5 6 */
 do_self,do_self,do_self,do_self, /* 08-0B 7 8 9 0 */
 do_self,do_self,do_self,do_self, /* 0C-0F + ' bs tab */
 do_self,do_self,do_self,do_self, /* 10-13 q w e r */
 do_self,do_self,do_self,do_self, /* 14-17 t y u i */
 do_self,do_self,do_self,do_self, /* 18-1B o p } ^ */
 do_self,ctrl,   do_self,do_self, /* 1C-1F enter ctrl a s */
 do_self,do_self,do_self,do_self, /* 20-23 d f g h */
 do_self,do_self,do_self,do_self, /* 24-27 j k l | */
 do_self,do_self,lshift, do_self, /* 28-2B { para lshift , */
 do_self,do_self,do_self,do_self, /* 2C-2F z x c v */
 do_self,do_self,do_self,do_self, /* 30-33 b n m , */
 do_self,minus,  rshift, do_self, /* 34-37 . - rshift * */
 alt,    do_self,caps,   func, /* 38-3B alt sp caps f1 */
 func,   func,   func,   func, /* 3C-3F f2 f3 f4 f5 */
 func,   func,   func,   func, /* 40-43 f6 f7 f8 f9 */
 func,   num,    scroll, cursor, /* 44-47 f10 num scr home */
 cursor, cursor, do_self,cursor, /* 48-4B up pgup - left */
 cursor, cursor, do_self,cursor, /* 4C-4F n5 right + end */
 cursor, cursor, cursor, cursor, /* 50-53 dn pgdn ins del */
 none,   none,   do_self,func, /* 54-57 sysreq ? < f11 */
 func,   none,   none,   none, /* 58-5B f12 ? ? ? */
 none,none,none,none, /* 5C-5F ? ? ? ? */
 none,none,none,none, /* 60-63 ? ? ? ? */
 none,none,none,none, /* 64-67 ? ? ? ? */
 none,none,none,none, /* 68-6B ? ? ? ? */
 none,none,none,none, /* 6C-6F ? ? ? ? */
 none,none,none,none, /* 70-73 ? ? ? ? */
 none,none,none,none, /* 74-77 ? ? ? ? */
 none,none,none,none, /* 78-7B ? ? ? ? */
 none,none,none,none, /* 7C-7F ? ? ? ? */
 none,none,none,none, /* 80-83 ? br br br */
 none,none,none,none, /* 84-87 br br br br */
 none,none,none,none, /* 88-8B br br br br */
 none,none,none,none, /* 8C-8F br br br br */
 none,none,none,none, /* 90-93 br br br br */
 none,none,none,none, /* 94-97 br br br br */
 none,none,none,none, /* 98-9B br br br br */
 none,unctrl,none,none, /* 9C-9F br unctrl br br */
 none,none,none,none, /* A0-A3 br br br br */
 none,none,none,none, /* A4-A7 br br br br */
 none,none,unlshift,none, /* A8-AB br br unlshift br */
 none,none,none,none, /* AC-AF br br br br */
 none,none,none,none, /* B0-B3 br br br br */
 none,none,unrshift,none, /* B4-B7 br br unrshift br */
 unalt,none,uncaps,none, /* B8-BB unalt br uncaps br */
 none,none,none,none, /* BC-BF br br br br */
 none,none,none,none, /* C0-C3 br br br br */
 none,none,none,none, /* C4-C7 br br br br */
 none,none,none,none, /* C8-CB br br br br */
 none,none,none,none, /* CC-CF br br br br */
 none,none,none,none, /* D0-D3 br br br br */
 none,none,none,none, /* D4-D7 br br br br */
 none,none,none,none, /* D8-DB br ? ? ? */
 none,none,none,none, /* DC-DF ? ? ? ? */
 none,none,none,none, /* E0-E3 e0 e1 ? ? */
 none,none,none,none, /* E4-E7 ? ? ? ? */
 none,none,none,none, /* E8-EB ? ? ? ? */
 none,none,none,none, /* EC-EF ? ? ? ? */
 none,none,none,none, /* F0-F3 ? ? ? ? */
 none,none,none,none, /* F4-F7 ? ? ? ? */
 none,none,none,none, /* F8-FB ? ? ? ? */
 none,none,none,none  /* FC-FF ? ? ? ? */
};
```



### keyboard.S / func（282行）

```assembly
// 下面子程序处理功能键。
func:
	push eax
	push ecx
	push edx
	call _show_stat // 调用显示各任务状态函数(kernl/sched.c, 37)。
	pop edx
	pop ecx
	pop eax
	sub al,3Bh // 功能键'F1'的扫描码是0x3B，因此此时al 中是功能键索引号。
	jb end_func // 如果扫描码小于0x3b，则不处理，返回。
	cmp al,9 // 功能键是F1-F10？
	jbe ok_func // 是，则跳转。
	sub al,18 // 是功能键F11，F12 吗？
	cmp al,10 // 是功能键F11？
	jb end_func // 不是，则不处理，返回。
	cmp al,11 // 是功能键F12？
	ja end_func // 不是，则不处理，返回。
ok_func:
	cmp ecx,4 /* check that there is enough room */ /* 检查是否有足够空间*/
	jl end_func // 需要放入4 个字符序列，如果放不下，则返回。
	mov eax,func_table[eax*4] // 取功能键对应字符序列。
	xor ebx,ebx
	jmp put_queue // 放入缓冲队列中。
end_func:
	ret

/*
* 功能键发送的扫描码，F1 键为：'esc [ [ A'， F2 键为：'esc [ [ B'等。
*/
func_table:
 DD 415b5b1bh,425b5b1bh,435b5b1bh,445b5b1bh
 DD 455b5b1bh,465b5b1bh,475b5b1bh,485b5b1bh
 DD 495b5b1bh,4a5b5b1bh,4b5b5b1bh,4c5b5b1bh
```





kernerl /sche.c /show_state

```c
void show_stat (void)
{
	int i;

	for (i = 0; i < NR_TASKS; i++)// NR_TASKS 是系统能容纳的最大进程（任务）数量（64 个），
		if (task[i])		// 定义在include/kernel/sched.h 第4 行。
			show_task (i, task[i]);
}

// 显示所有任务的任务号、进程号、进程状态和内核堆栈空闲字节数（大约）。
void show_stat (void)
{
	int i;

	for (i = 0; i < NR_TASKS; i++)// NR_TASKS 是系统能容纳的最大进程（任务）数量（64 个），
		if (task[i])		// 定义在include/kernel/sched.h 第4 行。
			show_task (i, task[i]);
}// 显示所有任务的任务号、进程号、进程状态和内核堆栈空闲字节数（大约）。
```





### kernerl/Keyboard/put_queue

```c
/*
* 下面该子程序把ebx:eax 中的最多8 个字符添入缓冲队列中。(edx 是
* 所写入字符的顺序是al,ah,eal,eah,bl,bh...直到eax 等于0。
*/
put_queue:
	push ecx // 保存ecx，edx 内容。
	push edx // 取控制台tty 结构中读缓冲队列指针。
	mov edx,_table_list // read-queue for console
	mov ecx,head[edx] // 取缓冲队列中头指针->ecx。
l7: mov buf[edx+ecx],al // 将al 中的字符放入缓冲队列头指针位置处。
	inc ecx // 头指针前移1 字节。
	and ecx,bsize-1 // 以缓冲区大小调整头指针(若超出则返回缓冲区开始)。
	cmp ecx,tail[edx] // buffer full - discard everything
// 头指针==尾指针吗(缓冲队列满)？
	je l9 // 如果已满，则后面未放入的字符全抛弃。
	shrd eax,ebx,8 // 将ebx 中8 位比特位右移8 位到eax 中，但ebx 不变。
	je l8 // 还有字符吗？若没有(等于0)则跳转。
	shr ebx,8 // 将ebx 中比特位右移8 位，并跳转到标号l7 继续操作。
	jmp l7
l8: mov head[edx],ecx // 若已将所有字符都放入了队列，则保存头指针。
	mov ecx,proc_list[edx] // 该队列的等待进程指针？
	test ecx,ecx // 检测任务结构指针是否为空(有等待该队列的进程吗？)。
	je l9 // 无，则跳转；
	mov dword ptr [ecx],0 // 有，则置该进程为可运行就绪状态(唤醒该进程)。
l9: pop edx // 弹出保留的寄存器并返回。
	pop ecx
	ret
```



### kernel\chr_drv\tty_io.c\do_tty_interrupt

```c
//// tty 中断处理调用函数 - 执行tty 中断处理。
// 参数：tty - 指定的tty 终端号（0，1 或2）。
// 将指定tty 终端队列缓冲区中的字符复制成规范(熟)模式字符并存放在辅助队列(规范模式队列)中。
// 在串口读字符中断(rs_io.s, 109)和键盘中断(kerboard.S, 69)中调用。
void do_tty_interrupt (int tty)
{
	copy_to_cooked (tty_table + tty);
}


void copy_to_cooked (struct tty_struct *tty)
{
	signed char c;

// 如果tty 的读队列缓冲区不空并且辅助队列缓冲区为空，则循环执行下列代码。
	while (!EMPTY (tty->read_q) && !FULL (tty->secondary))
	{
// 从队列尾处取一字符到c，并前移尾指针。
		GETCH (tty->read_q, c);
// 下面对输入字符，利用输入模式标志集进行处理。
// 如果该字符是回车符CR(13)，则：若回车转换行标志CRNL 置位则将该字符转换为换行符NL(10)；
// 否则若忽略回车标志NOCR 置位，则忽略该字符，继续处理其它字符。
		if (c == 13) {
			if (I_CRNL (tty))
				c = 10;
			else if (I_NOCR (tty))
				continue;
			else;
// 如果该字符是换行符NL(10)并且换行转回车标志NLCR 置位，则将其转换为回车符CR(13)。
		} else if (c == 10 && I_NLCR (tty))
			c = 13;
// 如果大写转小写标志UCLC 置位，则将该字符转换为小写字符。
		if (I_UCLC (tty))
			c = tolower (c);
// 如果本地模式标志集中规范（熟）模式标志CANON 置位，则进行以下处理。
		if (L_CANON (tty))
		{
// 如果该字符是键盘终止控制字符KILL(^U)，则进行删除输入行处理。
			if (c == KILL_CHAR (tty))
			{
/* deal with killing the input line *//* 删除输入行处理 */
// 如果tty 辅助队列不空，或者辅助队列中最后一个字符是换行NL(10)，或者该字符是文件结束字符
// (^D)，则循环执行下列代码。
				while (!(EMPTY (tty->secondary) ||
					(c = LAST (tty->secondary)) == 10 ||
					c == EOF_CHAR (tty)))
				{
// 如果本地回显标志ECHO 置位，那么：若字符是控制字符(值<32)，则往tty 的写队列中放入擦除
// 字符ERASE。再放入一个擦除字符ERASE，并且调用该tty 的写函数。
					if (L_ECHO (tty))
					{
						if (c < 32)
							PUTCH (127, tty->write_q);
						PUTCH (127, tty->write_q);
						tty->write (tty);
					}
// 将tty 辅助队列头指针后退1 字节。
					DEC (tty->secondary.head);
				}
				continue;		// 继续读取并处理其它字符。
			}
// 如果该字符是删除控制字符ERASE(^H)，那么：
			if (c == ERASE_CHAR (tty))
			{
// 若tty 的辅助队列为空，或者其最后一个字符是换行符NL(10)，或者是文件结束符，继续处理
// 其它字符。
				if (EMPTY (tty->secondary) ||
					(c = LAST (tty->secondary)) == 10 || c == EOF_CHAR (tty))
					continue;
// 如果本地回显标志ECHO 置位，那么：若字符是控制字符(值<32)，则往tty 的写队列中放入擦除
// 字符ERASE。再放入一个擦除字符ERASE，并且调用该tty 的写函数。
				if (L_ECHO (tty))
				{
					if (c < 32)
						PUTCH (127, tty->write_q);
					PUTCH (127, tty->write_q);
					tty->write (tty);
				}
// 将tty 辅助队列头指针后退1 字节，继续处理其它字符。
				DEC (tty->secondary.head);
				continue;
			}
//如果该字符是停止字符(^S)，则置tty 停止标志，继续处理其它字符。
			if (c == STOP_CHAR (tty))
			{
				tty->stopped = 1;
				continue;
			}
// 如果该字符是停止字符(^Q)，则复位tty 停止标志，继续处理其它字符。
			if (c == START_CHAR (tty))
			{
				tty->stopped = 0;
				continue;
			}
		}
// 若输入模式标志集中ISIG 标志置位，则在收到INTR、QUIT、SUSP 或DSUSP 字符时，需要为进程
// 产生相应的信号。
		if (L_ISIG (tty))
		{
// 如果该字符是键盘中断符(^C)，则向当前进程发送键盘中断信号，并继续处理下一字符。
			if (c == INTR_CHAR (tty))
			{
				tty_intr (tty, INTMASK);
				continue;
			}
// 如果该字符是键盘中断符(^\)，则向当前进程发送键盘退出信号，并继续处理下一字符。
			if (c == QUIT_CHAR (tty))
			{
				tty_intr (tty, QUITMASK);
				continue;
			}
		}
// 如果该字符是换行符NL(10)，或者是文件结束符EOF(^D)，辅助缓冲队列字符数加1。[??]
		if (c == 10 || c == EOF_CHAR (tty))
			tty->secondary.data++;
// 如果本地模式标志集中回显标志ECHO 置位，那么，如果字符是换行符NL(10)，则将换行符NL(10)
// 和回车符CR(13)放入tty 写队列缓冲区中；如果字符是控制字符(字符值<32)并且回显控制字符标志
// ECHOCTL 置位，则将字符'^'和字符c+64 放入tty 写队列中(也即会显示^C、^H 等)；否则将该字符
// 直接放入tty 写缓冲队列中。最后调用该tty 的写操作函数。
		if (L_ECHO (tty))
		{
			if (c == 10)
			{
				PUTCH (10, tty->write_q);
				PUTCH (13, tty->write_q);
			}
			else if (c < 32)
			{
				if (L_ECHOCTL (tty))
				{
					PUTCH ('^', tty->write_q);
					PUTCH (c + 64, tty->write_q);
				}
			}
			else
				PUTCH (c, tty->write_q);
			tty->write (tty);
		}
// 将该字符放入辅助队列中。
		PUTCH (c, tty->secondary);
	}
// 唤醒等待该辅助缓冲队列的进程（如果有的话）。
	wake_up (&tty->secondary.proc_list);
}
```



### \kernel\chr_drv\console.c\con_write

```c
con_write (struct tty_struct *tty)
{
	int nr;
	char c;

// 首先取得写缓冲队列中现有字符数nr，然后针对每个字符进行处理。
	nr = CHARS (tty->write_q);
	while (nr--)
	{
// 从写队列中取一字符c，根据前面所处理字符的状态state 分别处理。状态之间的转换关系为：
// state = 0：初始状态；或者原是状态4；或者原是状态1，但字符不是'['；
// 1：原是状态0，并且字符是转义字符ESC(0x1b = 033 = 27)；
// 2：原是状态1，并且字符是'['；
// 3：原是状态2；或者原是状态3，并且字符是';'或数字。
// 4：原是状态3，并且字符不是';'或数字；
		GETCH (tty->write_q, c);
		switch (state)
		{
		case 0:
// 如果字符不是控制字符(c>31)，并且也不是扩展字符(c<127)，则
			if (c > 31 && c < 127)
			{
// 若当前光标处在行末端或末端以外，则将光标移到下行头列。并调整光标位置对应的内存指针pos。
				if (x >= video_num_columns)
				{
					x -= video_num_columns;
					pos -= video_size_row;
					lf ();
				}
// 将字符c 写到显示内存中pos 处，并将光标右移1 列，同时也将pos 对应地移动2 个字节。
//__asm__ ("movb _attr,%%ah\n\t" "movw %%ax,%1\n\t"::"a" (c), "m" (*(short *) pos):"ax");
				_asm {
					mov al,c;
					mov ah,attr;
					mov ebx,pos
					mov [ebx],ax;
				}
				pos += 2;
				x++;
// 如果字符c 是转义字符ESC，则转换状态state 到1。
			}
			else if (c == 27)
				state = 1;
// 如果字符c 是换行符(10)，或是垂直制表符VT(11)，或者是换页符FF(12)，则移动光标到下一行。
			else if (c == 10 || c == 11 || c == 12)
				lf ();
// 如果字符c 是回车符CR(13)，则将光标移动到头列(0 列)。
			else if (c == 13)
				cr ();
// 如果字符c 是DEL(127)，则将光标右边一字符擦除(用空格字符替代)，并将光标移到被擦除位置。
			else if (c == ERASE_CHAR (tty))
				del ();
// 如果字符c 是BS(backspace,8)，则将光标右移1 格，并相应调整光标对应内存位置指针pos。
			else if (c == 8)
			{
				if (x)
				{
					x--;
					pos -= 2;
				}
// 如果字符c 是水平制表符TAB(9)，则将光标移到8 的倍数列上。若此时光标列数超出屏幕最大列数，
// 则将光标移到下一行上。
			}
			else if (c == 9)
			{
				c = (char)(8 - (x & 7));
				x += c;
				pos += c << 1;
				if (x > video_num_columns)
				{
					x -= video_num_columns;
					pos -= video_size_row;
					lf ();
				}
				c = 9;
// 如果字符c 是响铃符BEL(7)，则调用蜂鸣函数，是扬声器发声。
			}
			else if (c == 7)
				sysbeep ();
			break;
// 如果原状态是0，并且字符是转义字符ESC(0x1b = 033 = 27)，则转到状态1 处理。
		case 1:
			state = 0;
// 如果字符c 是'['，则将状态state 转到2。
			if (c == '[')
				state = 2;
// 如果字符c 是'E'，则光标移到下一行开始处(0 列)。
			else if (c == 'E')
				gotoxy (0, y + 1);
// 如果字符c 是'M'，则光标上移一行。
			else if (c == 'M')
				ri ();
// 如果字符c 是'D'，则光标下移一行。
			else if (c == 'D')
				lf ();
// 如果字符c 是'Z'，则发送终端应答字符序列。
			else if (c == 'Z')
				respond (tty);
// 如果字符c 是'7'，则保存当前光标位置。注意这里代码写错！应该是(c=='7')。
			else if (x == '7')
				save_cur ();
// 如果字符c 是'8'，则恢复到原保存的光标位置。注意这里代码写错！应该是(c=='8')。
			else if (x == '8')
				restore_cur ();
			break;
// 如果原状态是1，并且上一字符是'['，则转到状态2 来处理。
		case 2:
// 首先对ESC 转义字符序列参数使用的处理数组par[]清零，索引变量npar 指向首项，并且设置状态
// 为3。若此时字符不是'?'，则直接转到状态3 去处理，否则去读一字符，再到状态3 处理代码处。
			for (npar = 0; npar < NPAR; npar++)
				par[npar] = 0;
			npar = 0;
			state = 3;
			if (ques = (c == '?'))
				break;
// 如果原来是状态2；或者原来就是状态3，但原字符是';'或数字，则在下面处理。
		case 3:
// 如果字符c 是分号';'，并且数组par 未满，则索引值加1。
			if (c == ';' && npar < NPAR - 1)
			{
				npar++;
				break;
// 如果字符c 是数字字符'0'-'9'，则将该字符转换成数值并与npar 所索引的项组成10 进制数。
			}
			else if (c >= '0' && c <= '9')
			{
				par[npar] = 10 * par[npar] + c - '0';
				break;
// 否则转到状态4。
			}
			else
				state = 4;
// 如果原状态是状态3，并且字符不是';'或数字，则转到状态4 处理。首先复位状态state=0。
		case 4:
			state = 0;
			switch (c)
			{
// 如果字符c 是'G'或'`'，则par[]中第一个参数代表列号。若列号不为零，则将光标右移一格。
			case 'G':
			case '`':
				if (par[0])
					par[0]--;
				gotoxy (par[0], y);
				break;
// 如果字符c 是'A'，则第一个参数代表光标上移的行数。若参数为0 则上移一行。
			case 'A':
				if (!par[0])
					par[0]++;
				gotoxy (x, y - par[0]);
				break;
// 如果字符c 是'B'或'e'，则第一个参数代表光标下移的行数。若参数为0 则下移一行。
			case 'B':
			case 'e':
				if (!par[0])
					par[0]++;
				gotoxy (x, y + par[0]);
				break;
// 如果字符c 是'C'或'a'，则第一个参数代表光标右移的格数。若参数为0 则右移一格。
			case 'C':
			case 'a':
				if (!par[0])
					par[0]++;
				gotoxy (x + par[0], y);
				break;
// 如果字符c 是'D'，则第一个参数代表光标左移的格数。若参数为0 则左移一格。
			case 'D':
				if (!par[0])
					par[0]++;
				gotoxy (x - par[0], y);
				break;
// 如果字符c 是'E'，则第一个参数代表光标向下移动的行数，并回到0 列。若参数为0 则下移一行。
			case 'E':
				if (!par[0])
					par[0]++;
				gotoxy (0, y + par[0]);
				break;
// 如果字符c 是'F'，则第一个参数代表光标向上移动的行数，并回到0 列。若参数为0 则上移一行。
			case 'F':
				if (!par[0])
					par[0]++;
				gotoxy (0, y - par[0]);
				break;
// 如果字符c 是'd'，则第一个参数代表光标所需在的行号(从0 计数)。
			case 'd':
				if (par[0])
					par[0]--;
				gotoxy (x, par[0]);
				break;
// 如果字符c 是'H'或'f'，则第一个参数代表光标移到的行号，第二个参数代表光标移到的列号。
			case 'H':
			case 'f':
				if (par[0])
					par[0]--;
				if (par[1])
					par[1]--;
				gotoxy (par[1], par[0]);
				break;
// 如果字符c 是'J'，则第一个参数代表以光标所处位置清屏的方式：
// ANSI 转义序列：'ESC [sJ'(s = 0 删除光标到屏幕底端；1 删除屏幕开始到光标处；2 整屏删除)。
			case 'J':
				csi_J (par[0]);
				break;
// 如果字符c 是'K'，则第一个参数代表以光标所在位置对行中字符进行删除处理的方式。
// ANSI 转义字符序列：'ESC [sK'(s = 0 删除到行尾；1 从开始删除；2 整行都删除)。
			case 'K':
				csi_K (par[0]);
				break;
// 如果字符c 是'L'，表示在光标位置处插入n 行(ANSI 转义字符序列'ESC [nL')。
			case 'L':
				csi_L (par[0]);
				break;
// 如果字符c 是'M'，表示在光标位置处删除n 行(ANSI 转义字符序列'ESC [nM')。
			case 'M':
				csi_M (par[0]);
				break;
// 如果字符c 是'P'，表示在光标位置处删除n 个字符(ANSI 转义字符序列'ESC [nP')。
			case 'P':
				csi_P (par[0]);
				break;
// 如果字符c 是'@'，表示在光标位置处插入n 个字符(ANSI 转义字符序列'ESC [n@')。
			case '@':
				csi_at (par[0]);
				break;
// 如果字符c 是'm'，表示改变光标处字符的显示属性，比如加粗、加下划线、闪烁、反显等。
// ANSI 转义字符序列：'ESC [nm'。n = 0 正常显示；1 加粗；4 加下划线；7 反显；27 正常显示。
			case 'm':
				csi_m ();
				break;
// 如果字符c 是'r'，则表示用两个参数设置滚屏的起始行号和终止行号。
			case 'r':
				if (par[0])
					par[0]--;
				if (!par[1])
					par[1] = video_num_lines;
				if (par[0] < par[1] && par[1] <= video_num_lines)
				{
					top = par[0];
					bottom = par[1];
				}
				break;
// 如果字符c 是's'，则表示保存当前光标所在位置。
			case 's':
				save_cur ();
				break;
// 如果字符c 是'u'，则表示恢复光标到原保存的位置处。
			case 'u':
				restore_cur ();
				break;
			}
		}
	}
// 最后根据上面设置的光标位置，向显示控制器发送光标显示位置。
	set_cursor ();
}
```





## 字符输出





![image-20220606100006879](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220606100006879.png)

字符输出流程
无论是输出字符到屏幕上还是文件中，系统最终都会调用write函数来进行，而write函数则会调用sys_write系统调用来实现字符输出，如果是输出到屏幕上，则会调用tty_write函数，而tty_write最终也会调用con_write函数将字符输出；如果是输出到文件中，则会调用file_write函数直接将字符输出到指定的文件缓冲区中。所以无论是键盘输入的回显，还是程序的字符输出，那只需要修改con_write最终写到显存中的字符就可以了。但如果要控制字符输出到文件中，则需要修改file_write函数中输出到文件缓冲区中的字符即可。



### mian.c/printf

```c
static int printf(const char *fmt, ...)
// 产生格式化信息并输出到标准输出设备stdout(1)，这里是指屏幕上显示。参数'*fmt'
// 指定输出将采用的格式，参见各种标准C 语言书籍。该子程序正好是vsprintf 如何使
// 用的一个例子。
// 该程序使用vsprintf()将格式化的字符串放入printbuf 缓冲区，然后用write()
// 将缓冲区的内容输出到标准设备（1--stdout）。
{
	va_list args;
	int i;

	va_start(args, fmt);
    //关键是write中的1，文件描述来确定对那个文件进行操作
	write(1,printbuf,i=vsprintf(printbuf, fmt, args));
	va_end(args);
	return i;
}
```





### fs\read_write.c\sys_wirte

上面的write函数最终展开成带int 80的函数，最后调用sys_write函数

```c
int sys_write (unsigned int fd, char *buf, int count)
{
	struct file *file;
	struct m_inode *inode;

// 如果文件句柄值大于程序最多打开文件数NR_OPEN，或者需要写入的字节计数小于0，或者该句柄
// 的文件结构指针为空，则返回出错码并退出。
	if (fd >= NR_OPEN || count < 0 || !(file = current->filp[fd]))  //得到当前进程的文件描述符
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



![image-20220605213930803](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220605213930803.png)

![image-20220605214223901](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220605214223901.png)





### fs\char_dev.c\rw_char

根据主设备号调用相应种类设备的读写函数

```c
#define MAJOR(a) (((unsigned)(a))>>8)	// 取高字节（主设备号）。
#define MINOR(a) ((a)&0xff)	// 取低字节（次设备号）。


// 定义字符设备读写函数指针类型。
typedef (*crw_ptr)(int rw,unsigned minor,char * buf,int count,off_t * pos);

// 定义系统中设备种数。
#define NRDEVS ((sizeof (crw_table))/(sizeof (crw_ptr)))

// 字符设备读写函数指针表。
static crw_ptr crw_table[]={
	NULL,		/* 无设备(空设备) */
	rw_memory,	/* /dev/mem 等 */
	NULL,		/* /dev/fd 软驱 */
	NULL,		/* /dev/hd 硬盘 */
	rw_ttyx,	/* /dev/ttyx 串口终端 */
	rw_tty,		/* /dev/tty 终端 */
	NULL,		/* /dev/lp 打印机 */
	NULL};		/* 未命名管道 */


//// 字符设备读写操作函数。
// 参数：rw - 读写命令；dev - 设备号；buf - 缓冲区；count - 读写字节数；pos -读写指针。
// 返回：实际读/写字节数。
int rw_char(int rw,int dev, char * buf, int count, off_t * pos)
{
	crw_ptr call_addr;

// 如果设备号超出系统设备数，则返回出错码。
	if (MAJOR(dev)>=NRDEVS)
		return -ENODEV;
// 若该设备没有对应的读/写函数，则返回出错码。
	if (!(call_addr=crw_table[MAJOR(dev)]))
		return -ENODEV;
// 调用对应设备的读写操作函数，并返回实际读/写的字节数。
	return call_addr(rw,MINOR(dev),buf,count,pos);
}
```



### fs\char_dev.c\rw_ttyx

根据子设备开始调用读写函数

```c
//// 串口终端读写操作函数。
// 参数：rw - 读写命令；minor - 终端子设备号；buf - 缓冲区；cout - 读写字节数；
//       pos - 读写操作当前指针，对于终端操作，该指针无用。
// 返回：实际读写的字节数。
static int rw_ttyx(int rw,unsigned minor,char * buf,int count,off_t * pos)
{
	return ((rw==READ)?tty_read(minor,buf,count):
		tty_write(minor,buf,count));
}

```



### kernel\chr_drv\tty_io.c\tty_write

```c
//// tty 写函数。
// 参数：channel - 子设备号；buf - 缓冲区指针；nr - 写字节数。
// 返回已写字节数。
int tty_write (unsigned channel, char *buf, int nr)
{
	static cr_flag = 0;
	struct tty_struct *tty;
	char c, *b = buf;

// 本版本linux 内核的终端只有3 个子设备，分别是控制台(0)、串口终端1(1)和串口终端2(2)。
// 所以任何大于2 的子设备号都是非法的。写的字节数当然也不能小于0 的。
	if (channel > 2 || nr < 0)
		return -1;
// tty 指针指向子设备号对应ttb_table 表中的tty 结构。
	tty = channel + tty_table;
// 字符设备是一个一个字符进行处理的，所以这里对于nr 大于0 时对每个字符进行循环处理。
	while (nr > 0)
	{
// 如果此时tty 的写队列已满，则当前进程进入可中断的睡眠状态。
		sleep_if_full (&tty->write_q);
// 如果当前进程有信号要处理，则退出，返回0。
		if (current->signal)
			break;
// 当要写的字节数>0 并且tty 的写队列不满时，循环执行以下操作。
		while (nr > 0 && !FULL (tty->write_q))
		{
// 从用户数据段内存中取一字节c。
			c = get_fs_byte (b);
// 如果终端输出模式标志集中的执行输出处理标志OPOST 置位，则执行下列输出时处理过程。
			if (O_POST (tty))
			{
// 如果该字符是回车符'\r'(CR，13)并且回车符转换行符标志OCRNL 置位，则将该字符换成换行符
// '\n'(NL，10)；否则如果该字符是换行符'\n'(NL，10)并且换行转回车功能标志ONLRET 置位的话，
// 则将该字符换成回车符'\r'(CR，13)。
				if (c == '\r' && O_CRNL (tty))
					c = '\n';
				else if (c == '\n' && O_NLRET (tty))
					c = '\r';
// 如果该字符是换行符'\n'并且回车标志cr_flag 没有置位，换行转回车-换行标志ONLCR 置位的话，
// 则将cr_flag 置位，并将一回车符放入写队列中。然后继续处理下一个字符。
				if (c == '\n' && !cr_flag && O_NLCR (tty))
				{
					cr_flag = 1;
					PUTCH (13, tty->write_q);
					continue;
				}
// 如果小写转大写标志OLCUC 置位的话，就将该字符转成大写字符。
				if (O_LCUC (tty))
					c = toupper (c);
			}
// 用户数据缓冲指针b 前进1 字节；欲写字节数减1 字节；复位cr_flag 标志，并将该字节放入tty
// 写队列中。
			b++;
			nr--;
			cr_flag = 0;
			PUTCH (c, tty->write_q);
		}
// 若字节全部写完，或者写队列已满，则程序执行到这里。调用对应tty 的写函数，若还有字节要写，
// 则等待写队列不满，所以调用调度程序，先去执行其它任务。
		tty->write (tty);
		if (nr > 0)
		schedule ();
	}
	return (b - buf);		// 返回写入的字节数。
}
```





### \kernel\chr_drv\console.c\con_write



tty-write调用的是函数指针

```c
//include/linux/tty.h

// tty 数据结构。
struct tty_struct
{
  struct termios termios;	// 终端io 属性和控制字符数据结构。
  int pgrp;			// 所属进程组。
  int stopped;			// 停止标志。
  void (*write) (struct tty_struct * tty);	// tty 写函数指针。
  struct tty_queue read_q;	// tty 读队列。
  struct tty_queue write_q;	// tty 写队列。
  struct tty_queue secondary;	// tty 辅助队列(存放规范模式字符序列)，
};				// 可称为规范(熟)模式队列。

extern struct tty_struct tty_table[];	// tty 结构数组。


//对上面数据结构的初始化


// tty 数据结构的tty_table 数组。其中包含三个初始化项数据，分别对应控制台、串口终端1 和
// 串口终端2 的初始化数据。
struct tty_struct tty_table[] = {
{
	{//termios
		ICRNL,			/* 将输入的CR 转换为NL */
		OPOST | ONLCR,		/* 将输出的NL 转CRNL */
		0,				// 控制模式标志初始化为0。
		ISIG | ICANON | ECHO | ECHOCTL | ECHOKE,	// 本地模式标志。
		0,				/* 控制台termio。 */
		INIT_C_CC			// 控制字符数组。
	},
	0,				/* 所属初始进程组。 */
	0,				/* 初始停止标志。 */
	con_write,			// tty 写函数指针。
	{0, 0, 0, 0, ""},		/* console read-queue */// tty 控制台读队列。
	{0, 0, 0, 0, ""},		/* console write-queue */// tty 控制台写队列。
	{0, 0, 0, 0, ""}		/* console secondary queue */// tty 控制台辅助(第二)队列。
}, {
	{
		0,			/* no translation */// 输入模式标志。0，无须转换。
		0,			/* no translation */// 输出模式标志。0，无须转换。
		B2400 | CS8,		// 控制模式标志。波特率2400bps，8 位数据位。
		0,			// 本地模式标志0。
		0,			// 行规程0。
		INIT_C_CC
	},		// 控制字符数组。
	0,			// 所属初始进程组。
	0,			// 初始停止标志。
	rs_write,		// 串口1 tty 写函数指针。
	{0x3f8, 0, 0, 0, ""},	/* rs 1 */// 串行终端1 读缓冲队列。
	{0x3f8, 0, 0, 0, ""},	// 串行终端1 写缓冲队列。
	{0, 0, 0, 0, ""}		// 串行终端1 辅助缓冲队列。
}, {
	{
		0,			/* no translation */// 输入模式标志。0，无须转换。
		0,			/* no translation */// 输出模式标志。0，无须转换。
		B2400 | CS8,	// 控制模式标志。波特率2400bps，8 位数据位。
		0,			// 本地模式标志0。
		0,			// 行规程0。
		INIT_C_CC
	},		// 控制字符数组。
	0,			// 所属初始进程组。
	0,			// 初始停止标志。
	rs_write,		// 串口2 tty 写函数指针。
	{0x2f8, 0, 0, 0, ""},	/* rs 2 */// 串行终端2 读缓冲队列。
	{0x2f8, 0, 0, 0, ""},	// 串行终端2 写缓冲队列。
	{0, 0, 0, 0, ""}	// 串行终端2 辅助缓冲队列。
}
};
```



真正在显示屏上读写的函数，set_cursor展开为汇编函数，发送out指令

```c
// 控制台写函数。
// 从终端对应的tty 写缓冲队列中取字符，并显示在屏幕上。
void
con_write (struct tty_struct *tty)
{
	int nr;
	char c;

// 首先取得写缓冲队列中现有字符数nr，然后针对每个字符进行处理。
	nr = CHARS (tty->write_q);
	while (nr--)
	{
// 从写队列中取一字符c，根据前面所处理字符的状态state 分别处理。状态之间的转换关系为：
// state = 0：初始状态；或者原是状态4；或者原是状态1，但字符不是'['；
// 1：原是状态0，并且字符是转义字符ESC(0x1b = 033 = 27)；
// 2：原是状态1，并且字符是'['；
// 3：原是状态2；或者原是状态3，并且字符是';'或数字。
// 4：原是状态3，并且字符不是';'或数字；
		GETCH (tty->write_q, c);
		switch (state)
		{
		case 0:
// 如果字符不是控制字符(c>31)，并且也不是扩展字符(c<127)，则
			if (c > 31 && c < 127)
			{
// 若当前光标处在行末端或末端以外，则将光标移到下行头列。并调整光标位置对应的内存指针pos。
				if (x >= video_num_columns)
				{
					x -= video_num_columns;
					pos -= video_size_row;
					lf ();
				}
// 将字符c 写到显示内存中pos 处，并将光标右移1 列，同时也将pos 对应地移动2 个字节。
//__asm__ ("movb _attr,%%ah\n\t" "movw %%ax,%1\n\t"::"a" (c), "m" (*(short *) pos):"ax");
				_asm {
					mov al,c;
					mov ah,attr;
					mov ebx,pos
					mov [ebx],ax;
				}
				pos += 2;
				x++;
// 如果字符c 是转义字符ESC，则转换状态state 到1。
			}
			else if (c == 27)
				state = 1;
// 如果字符c 是换行符(10)，或是垂直制表符VT(11)，或者是换页符FF(12)，则移动光标到下一行。
			else if (c == 10 || c == 11 || c == 12)
				lf ();
// 如果字符c 是回车符CR(13)，则将光标移动到头列(0 列)。
			else if (c == 13)
				cr ();
// 如果字符c 是DEL(127)，则将光标右边一字符擦除(用空格字符替代)，并将光标移到被擦除位置。
			else if (c == ERASE_CHAR (tty))
				del ();
// 如果字符c 是BS(backspace,8)，则将光标右移1 格，并相应调整光标对应内存位置指针pos。
			else if (c == 8)
			{
				if (x)
				{
					x--;
					pos -= 2;
				}
// 如果字符c 是水平制表符TAB(9)，则将光标移到8 的倍数列上。若此时光标列数超出屏幕最大列数，
// 则将光标移到下一行上。
			}
			else if (c == 9)
			{
				c = (char)(8 - (x & 7));
				x += c;
				pos += c << 1;
				if (x > video_num_columns)
				{
					x -= video_num_columns;
					pos -= video_size_row;
					lf ();
				}
				c = 9;
// 如果字符c 是响铃符BEL(7)，则调用蜂鸣函数，是扬声器发声。
			}
			else if (c == 7)
				sysbeep ();
			break;
// 如果原状态是0，并且字符是转义字符ESC(0x1b = 033 = 27)，则转到状态1 处理。
		case 1:
			state = 0;
// 如果字符c 是'['，则将状态state 转到2。
			if (c == '[')
				state = 2;
// 如果字符c 是'E'，则光标移到下一行开始处(0 列)。
			else if (c == 'E')
				gotoxy (0, y + 1);
// 如果字符c 是'M'，则光标上移一行。
			else if (c == 'M')
				ri ();
// 如果字符c 是'D'，则光标下移一行。
			else if (c == 'D')
				lf ();
// 如果字符c 是'Z'，则发送终端应答字符序列。
			else if (c == 'Z')
				respond (tty);
// 如果字符c 是'7'，则保存当前光标位置。注意这里代码写错！应该是(c=='7')。
			else if (x == '7')
				save_cur ();
// 如果字符c 是'8'，则恢复到原保存的光标位置。注意这里代码写错！应该是(c=='8')。
			else if (x == '8')
				restore_cur ();
			break;
// 如果原状态是1，并且上一字符是'['，则转到状态2 来处理。
		case 2:
// 首先对ESC 转义字符序列参数使用的处理数组par[]清零，索引变量npar 指向首项，并且设置状态
// 为3。若此时字符不是'?'，则直接转到状态3 去处理，否则去读一字符，再到状态3 处理代码处。
			for (npar = 0; npar < NPAR; npar++)
				par[npar] = 0;
			npar = 0;
			state = 3;
			if (ques = (c == '?'))
				break;
// 如果原来是状态2；或者原来就是状态3，但原字符是';'或数字，则在下面处理。
		case 3:
// 如果字符c 是分号';'，并且数组par 未满，则索引值加1。
			if (c == ';' && npar < NPAR - 1)
			{
				npar++;
				break;
// 如果字符c 是数字字符'0'-'9'，则将该字符转换成数值并与npar 所索引的项组成10 进制数。
			}
			else if (c >= '0' && c <= '9')
			{
				par[npar] = 10 * par[npar] + c - '0';
				break;
// 否则转到状态4。
			}
			else
				state = 4;
// 如果原状态是状态3，并且字符不是';'或数字，则转到状态4 处理。首先复位状态state=0。
		case 4:
			state = 0;
			switch (c)
			{
// 如果字符c 是'G'或'`'，则par[]中第一个参数代表列号。若列号不为零，则将光标右移一格。
			case 'G':
			case '`':
				if (par[0])
					par[0]--;
				gotoxy (par[0], y);
				break;
// 如果字符c 是'A'，则第一个参数代表光标上移的行数。若参数为0 则上移一行。
			case 'A':
				if (!par[0])
					par[0]++;
				gotoxy (x, y - par[0]);
				break;
// 如果字符c 是'B'或'e'，则第一个参数代表光标下移的行数。若参数为0 则下移一行。
			case 'B':
			case 'e':
				if (!par[0])
					par[0]++;
				gotoxy (x, y + par[0]);
				break;
// 如果字符c 是'C'或'a'，则第一个参数代表光标右移的格数。若参数为0 则右移一格。
			case 'C':
			case 'a':
				if (!par[0])
					par[0]++;
				gotoxy (x + par[0], y);
				break;
// 如果字符c 是'D'，则第一个参数代表光标左移的格数。若参数为0 则左移一格。
			case 'D':
				if (!par[0])
					par[0]++;
				gotoxy (x - par[0], y);
				break;
// 如果字符c 是'E'，则第一个参数代表光标向下移动的行数，并回到0 列。若参数为0 则下移一行。
			case 'E':
				if (!par[0])
					par[0]++;
				gotoxy (0, y + par[0]);
				break;
// 如果字符c 是'F'，则第一个参数代表光标向上移动的行数，并回到0 列。若参数为0 则上移一行。
			case 'F':
				if (!par[0])
					par[0]++;
				gotoxy (0, y - par[0]);
				break;
// 如果字符c 是'd'，则第一个参数代表光标所需在的行号(从0 计数)。
			case 'd':
				if (par[0])
					par[0]--;
				gotoxy (x, par[0]);
				break;
// 如果字符c 是'H'或'f'，则第一个参数代表光标移到的行号，第二个参数代表光标移到的列号。
			case 'H':
			case 'f':
				if (par[0])
					par[0]--;
				if (par[1])
					par[1]--;
				gotoxy (par[1], par[0]);
				break;
// 如果字符c 是'J'，则第一个参数代表以光标所处位置清屏的方式：
// ANSI 转义序列：'ESC [sJ'(s = 0 删除光标到屏幕底端；1 删除屏幕开始到光标处；2 整屏删除)。
			case 'J':
				csi_J (par[0]);
				break;
// 如果字符c 是'K'，则第一个参数代表以光标所在位置对行中字符进行删除处理的方式。
// ANSI 转义字符序列：'ESC [sK'(s = 0 删除到行尾；1 从开始删除；2 整行都删除)。
			case 'K':
				csi_K (par[0]);
				break;
// 如果字符c 是'L'，表示在光标位置处插入n 行(ANSI 转义字符序列'ESC [nL')。
			case 'L':
				csi_L (par[0]);
				break;
// 如果字符c 是'M'，表示在光标位置处删除n 行(ANSI 转义字符序列'ESC [nM')。
			case 'M':
				csi_M (par[0]);
				break;
// 如果字符c 是'P'，表示在光标位置处删除n 个字符(ANSI 转义字符序列'ESC [nP')。
			case 'P':
				csi_P (par[0]);
				break;
// 如果字符c 是'@'，表示在光标位置处插入n 个字符(ANSI 转义字符序列'ESC [n@')。
			case '@':
				csi_at (par[0]);
				break;
// 如果字符c 是'm'，表示改变光标处字符的显示属性，比如加粗、加下划线、闪烁、反显等。
// ANSI 转义字符序列：'ESC [nm'。n = 0 正常显示；1 加粗；4 加下划线；7 反显；27 正常显示。
			case 'm':
				csi_m ();
				break;
// 如果字符c 是'r'，则表示用两个参数设置滚屏的起始行号和终止行号。
			case 'r':
				if (par[0])
					par[0]--;
				if (!par[1])
					par[1] = video_num_lines;
				if (par[0] < par[1] && par[1] <= video_num_lines)
				{
					top = par[0];
					bottom = par[1];
				}
				break;
// 如果字符c 是's'，则表示保存当前光标所在位置。
			case 's':
				save_cur ();
				break;
// 如果字符c 是'u'，则表示恢复光标到原保存的位置处。
			case 'u':
				restore_cur ();
				break;
			}
		}
	}
// 最后根据上面设置的光标位置，向显示控制器发送光标显示位置。
	set_cursor ();
}
```



最终转换为out指令

![image-20220606094309922](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220606094309922.png)

![image-20220606094338813](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220606094338813.png)
