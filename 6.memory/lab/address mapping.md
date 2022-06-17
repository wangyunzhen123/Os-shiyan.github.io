## 段表



### 查找GDT表得到LDT表物理地址



输入指令sreg得到得到

![image-20220604200121156](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220604200121156.png)

![image-20220604195408553](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220604195408553.png)

![image-20220604195548845](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220604195548845.png)



注意：LDTR寄存器有段选择部分和段表述部分，查LDTR的在GDT表中的位置是用段选择部分,段描述符部分内容就是段描述符的内容（切换进程时复制的）

其中段选择部分如下：

![image-20220604195808038](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220604195808038.png)

![image-20220604200725302](C:\Users\dell\AppData\Roaming\Typora\typora-user-images\image-20220604200725302.png)

可以看到 ldtr 的值是 `0x0068=0000000001101000`（二进制），表示 LDT 表存放在 GDT 表的 1101（二进制）=13（十进制）号位置，后13表示该进程LDT表存放在GDT中的位置





![image-20220604200749970](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220604200749970.png)





而 GDT 的位置已经由 gdtr 明确给出，在物理地址的 `0x00005cb8`



GDT 表中的每一项占 64 位（8 个字节），所以我们要查找的项的地址是 `0x00005cb8+13*8`。

查看GDT的前16项：

![image-20220604201651067](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220604201651067.png)

输入 `xp /2w 0x00005cb8+13*8`，得到如下即为LDT（段表）的物理地址

![image-20220604201819336](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220604201819336.png)

如果想确认是否准确，就看 `sreg` 输出中，ldtr 所在行里，`dl` 和 `dh` 的值，它们是 Bochs 的调试器自动计算出的，你寻找到的必须和它们一致。



“0x**a2d0**0068 0x000082**fa**” 将其中的加粗数字组合为“0x00faa2d0”，这就是 LDT 表的物理地址。为什么这么组合间上图4-13段描述符通用格式，GDT表和LDT表中的格式都是如此





### 查找用DS比对LDT描述符得到线性地址

`xp /8w 0x00faa2d0`，得到LDT 表的前 4 项内容了。

![image-20220604202701041](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220604202701041.png)



在保护模式下，段寄存器有另一个名字，叫段选择子，因为它保存的信息主要是该段在段表里索引值，用这个索引值可以从段表中“选择”出相应的段描述符。

ds，`0x0017=0000000000010111`（二进制），所以 RPL=11，可见是在最低的特权级（因为在应用程序中执行），TI=1，表示查找 LDT 表，索引值为 10（二进制）= 2（十进制），表示找 LDT 表中的第 3 个段描述符（从 0 开始编号）。

LDT 和 GDT 的结构一样，每项占 8 个字节。所以第 3 项 `0x00003fff 0x10c0f300`（上一步骤的最后一个输出结果中） 就是搜寻好久的 ds 的段描述符了



得到线性地址：

费了很大的劲，实际上我们需要的只有段基址一项数据，即段描述符 “0x**0000**3fff 0x**10**c0f3**00**” 中加粗部分组合成的 “0x10000000”。这就是 ds 段在线性地址空间中的起始地址。用同样的方法也可以算算其它段的基址，都是这个数。

段基址+段内偏移，就是线性地址了。所以 ds:0x3004 的线性地址就是：

```txt
0x10000000 + 0x3004 = 0x10003004
```

用 `calc ds:0x3004` 命令可以验证这个结果。











## 页表



### 查页目录得到页表



从线性地址计算物理地址，需要查找页表。线性地址变成物理地址的过程如下：

![img](https://doc.shiyanlou.com/userid19614labid573time1424086691959)

 0x10003004 的页目录号是 64，页号 3，页内偏移是 4。

![image-20220604193803268](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220604193803268.png)

c



creg命令查看页目录位置

![image-20220604204011540](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220604204011540.png)



![image-20220604204203363](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220604204203363.png)

![image-20220604204655854](C:\Users\dell\AppData\Roaming\Typora\typora-user-images\image-20220604204655854.png)

![image-20220604211420505](C:\Users\dell\AppData\Roaming\Typora\typora-user-images\image-20220604211420505.png)



说明页目录表的基址为 0。看看其内容，“xp /68w 0”：

![image-20220604204734556](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220604204734556.png)

页目录表和页表中的内容很简单，是 1024 个 32 位（正好是 4K）数。这 32 位中前 20 位是物理页框号，后面是一些属性信息（其中最重要的是最后一位 P）。其中第 65 个页目录项就是我们要找页表，用“xp /w 0+64*4”查看：

![image-20220604204941949](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220604204941949.png)



### 查页表得到物理地址

其中的 027 是属性，显然 P=1页表所在物理页框号为 0x00fa2，即页表在物理内存的 0x00faa000 位置。从该位置开始查找 3 号页表项，得到（xp /w 0x00fa2000+3*4）：

![image-20220604205930297](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220604205930297.png)

最后在页表中得到页表项0x00f9b067。其中 067 是属性，显然 P=1，应该是这样。0x00f9b000 + 0x004 = 0x00f9b004是最终的物理地址





线性地址 0x10003004 对应的物理页框号为 0x00f9b04，和页内偏移 0x004 接到一起，得到 0x00f9b004，这就是变量 i 的物理地址。可以通过两种方法验证。

第一种方法是用命令 `page 0x10003004`，可以得到信息：

![image-20220604211038176](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220604211038176.png)

第二种方法是用命令 `xp /w 0x00fa7004`，可以看到：

![image-20220604210709408](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220604210709408.png)

这个数值确实是 test.c 中 i 的初值。

现在，通过直接修改内存来改变 i 的值为 0，命令是： setpmem 0x00f9b004  0，表示从 0x00f9b004 地址开始的 4 个字节都设为 0。然后再用“c”命令继续 Bochs 的运行，可以看到 test 退出了，说明 i 的修改成功了，此项实验结束。

![image-20220604211142369](http://fastly.jsdelivr.net/gh/wangyunzhen123/Image@main/img/image-20220604211142369.png)







## 参考资料

[(9条消息) bochs调试方法与指令详解_guozuofeng的博客-CSDN博客](https://blog.csdn.net/guozuofeng/article/details/102646618)

[操作系统原理与实践 - 地址映射与共享 - 蓝桥云课 (lanqiao.cn)](https://www.lanqiao.cn/courses/115/learning/?id=573)

[(9条消息) HIT Linux-0.11 实验七 地址映射与内存共享 实验报告_laoshuyudaohou的博客-CSDN博客_linux共享内存实验报告](https://blog.csdn.net/laoshuyudaohou/article/details/103843023)

[Linux操作系统(哈工大李治军老师)实验楼实验6-地址映射与共享(1)_哔哩哔哩_bilibili](https://www.bilibili.com/video/BV1Ph411q7P5?spm_id_from=333.999.0.0)