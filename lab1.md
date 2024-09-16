#lab 0.5
实验0.5主要讲解最小可执行内核和启动流程。我们的内核主要在 Qemu 模拟器上运行，它可以模拟一台 64 位 RISC-V 计算机。为了让我们的内核能够正确对接到 Qemu 模拟器上，需要了解 Qemu 模拟器的启动流程，还需要一些程序内存布局和编译流程（特别是链接）相关知识,以及通过opensbi固件来通过服务。
##知识点
本实验中重要的知识点，与对应OS原理以及对二者的理解。
###重要知识点
####1.
####2.
##练习1: 使用GDB验证启动流程

为了熟悉使用qemu和gdb进行调试工作,使用gdb调试QEMU模拟的RISC-V计算机加电开始运行到执行应用程序的第一条指令（即跳转到0x80200000）这个阶段的执行过程，说明RISC-V硬件加电后的几条指令在哪里？完成了哪些功能？要求在报告中简要写出练习过程和回答。
Lab0.5运行结果截图：
![屏幕截图 2024-09-16 184210](https://github.com/user-attachments/assets/9b5f74d7-9db9-40ab-b49f-04faadcd42eb)


![屏幕截图 2024-09-16 184513](https://github.com/user-attachments/assets/8e4a6471-4363-49ca-b309-e2e99918c31e)
加电后执行的指令：
![屏幕截图 2024-09-16 184625](https://github.com/user-attachments/assets/684570bd-f310-42d6-9e6f-930abeb85acf)
0x1000:auipc t0 , 0x0:将当前程序计数器PC中保存的值与0x0相加，并将结果保存在寄存器t0中
![屏幕截图 2024-09-16 185738](https://github.com/user-attachments/assets/42f46ebb-fadd-41ac-948b-be54282314bc)
执行后可以看到t0寄存器的值被设置为0x1000
0x1004: addi a1, t0, 32：将寄存器t0中的值与立即数32相加，并将结果存储在寄存器a1中。
![屏幕截图 2024-09-16 190020](https://github.com/user-attachments/assets/d0d0844a-2e07-420e-928d-b3c0aa441171)
执行后a1寄存器的值被设置为0x1020
csrr a0, mhartid：mhartid是RISC-V的机器级CSR寄存器，用于存储当前硬件线程ID，本条指令用于从mhartid中读取硬件线程ID并将结果保存在a0寄存器中
![屏幕截图 2024-09-16 190020](https://github.com/user-attachments/assets/4139c333-2c2a-4d02-860a-ba5deb2ab9dc)
执行结果如上
0x100c: ld t0, 24(t0)：用于从存储器中加载一个64位的值存储在寄存器t0中，目标地址为当前t0寄存器中的值加上偏移量24
![屏幕截图 2024-09-16 191231](https://github.com/user-attachments/assets/dc4fc7c5-2118-4930-b97b-04e9976a1621)
0x1010: jr t0：无条件跳转到寄存器t0中存储的地址处，即跳转到0x80000000处。
再执行指令前先查看0x80000000处的指令：
![屏幕截图 2024-09-16 191557](https://github.com/user-attachments/assets/cc32f9f5-34b3-4371-8a62-d77c755d6fe2)
单步调试并查看即将执行的指令：
![屏幕截图 2024-09-16 191710](https://github.com/user-attachments/assets/a3831269-4a12-40e8-affe-734e98d93e81)
可以发现程序正确跳转到了0x80000000处，并即将执行接下来的指令。



#lab 1
##练习1：理解内核启动中的程序入口操作

阅读 kern/init/entry.S内容代码，结合操作系统内核启动流程，说明指令 la sp, bootstacktop 完成了什么操作，目的是什么？ tail kern_init 完成了什么操作，目的是什么？

###la sp, bootstacktop
####完成的操作：
将栈指针（Stack Pointer）设置为bootstacktop的地址。
####目的：
让操作系统在内核启动过程中使用的栈空间被正确初始化。

###tail kern_init
####完成的操作：
跳转到名为kern_init的函数。
####目的：
操作系统内核初始化的入口点，通过跳转到这个函数，操作系统可以执行各种初始化任务。


##练习2：完善中断处理 （需要编程）

请编程完善trap.c中的中断处理函数trap，在对时钟中断进行处理的部分填写kern/trap/trap.c函数中处理时钟中断的部分，使操作系统每遇到100次时钟中断后，调用print_ticks子程序，向屏幕上打印一行文字”100 ticks”，在打印完10行后调用sbi.h中的shut_down()函数关机。

要求完成问题1提出的相关函数实现，提交改进后的源代码包（可以编译执行），并在实验报告中简要说明实现过程和定时器中断中断处理的流程。实现要求的部分代码后，运行整个系统，大约每1秒会输出一次”100 ticks”，输出10行。
###思路
###代码
###结果截图

##扩展练习 Challenge1：描述与理解中断流程

回答：描述ucore中处理中断异常的流程（从异常的产生开始），其中mov a0，sp的目的是什么？SAVE_ALL中寄寄存器保存在栈中的位置是什么确定的？对于任何中断，__alltraps 中都需要保存所有寄存器吗？请说明理由。
###异常的流程
产生->进入入口点->保存上下文->寄存器传参给trap->调用trap处理->执行_trapret->回复上下文->从返回地址出继续执行

###保存位置
将当前的栈指针的值复制到寄存器a0是为了使a0寄存器传递参数给接下来调用的函数trap。

###SAVE_ALL
寄存器保存在栈中的位置根据REGBYTES的大小确定，并且按照顺序以一定偏移量排列。

###__alltraps 
不是，比如一般的时钟中断可能只需要保存一般用途寄存器和程序计数器。也就是说不同类型的中断可能需要不同的寄存器保存策略。有时为了提高性能，可能会采用一种更轻量级的寄存器保存策略。


##扩增练习 Challenge2：理解上下文切换机制

回答：在trapentry.S中汇编代码 csrw sscratch, sp；csrrw s0, sscratch, x0实现了什么操作，目的是什么？save all里面保存了stval scause这些csr，而在restore all里面却不还原它们？那这样store的意义何在呢？

###csrw sscratch, sp
这是保存整个中断上下文的一部分，将栈指针sp的值写入sscratch控制寄存器。保存执行过程中的栈指针，让中断处理程序完成后能够恢复到正确的栈

###csrrw s0, sscratch, x0
将sscratch控制寄存器的值写入通用寄存器s0，在中断处理程序中可以随时访问而不改变原始值；同时将x0的值写入sscratch控制寄存器，此时寄存器清零，发生递归异常时异常向量就会知道它来自内核。

###save all
stval和scause寄存器通常用于传递有关异常或中断的信息，异常或中断处理完成后它们的值就不再具有特定的意义，所以不需要还原它们。

##扩展练习Challenge3：完善异常中断

编程完善在触发一条非法指令异常 mret和，在 kern/trap/trap.c的异常处理函数中捕获，并对其进行处理，简单输出异常类型和异常指令触发地址，即“Illegal instruction caught at 0x(地址)”，“ebreak caught at 0x（地址）”与“Exception type:Illegal instruction"，“Exception type: breakpoint”。

	case CAUSE_ILLEGAL_INSTRUCTION:
	    // 非法指令异常处理
	    /* LAB1 CHALLENGE3   YOUR CODE :2111033 2113826 2113831  */
	    /*(1)输出指令异常类型（ Illegal instruction）
	      *(2)输出异常指令地址
	      *(3)更新 tf->epc寄存器
	    */
	    cprintf("Illegal instruction caught at 0x%08x\n",tf->epc);
	    cprintf("Exception type:Illegal instruction\n");
	    tf->epc += 4;
	    break;
	case CAUSE_BREAKPOINT:
	    //断点异常处理
	    /* LAB1 CHALLLENGE3   YOUR CODE :2111033 2113826 2113831  */
	    /*(1)输出指令异常类型（ breakpoint）
	      *(2)输出异常指令地址
	      *(3)更新 tf->epc寄存器
	    */
	    cprintf("ebreak caught at 0x%08x\n",tf->epc);
	    cprintf("Exception type:breakpoint\n");
	    tf->epc += 4;
	    break;


