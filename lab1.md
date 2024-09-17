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

使用GDB进行调试

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

可以发现程序正确跳转到了0x80000000处，并即将执行接下来的指令。在QEMU模拟的riscv计算机中，在Qemu开始执行指令之前，作为 bootloader 的 OpenSBI.bin 被加载到物理内存以物理地址 0x80000000 开头的区域上，同时内核镜像 os.bin 被加载到以物理地址 0x80200000 开头的区域上，故内核的第一条指令应位于物理地址 0x80200000 处，为此在0x80200000处打断点并执行。

![屏幕截图 2024-09-16 193943](https://github.com/user-attachments/assets/2a782d65-c5ab-41e7-9eeb-73389a61cc71)

至此完成了Lab0.5的GDB调试任务

![屏幕截图 2024-09-16 193955](https://github.com/user-attachments/assets/6835c6e4-4dec-4ff7-869b-89531309d689)






#lab 1
##练习1：理解内核启动中的程序入口操作

阅读 kern/init/entry.S内容代码，结合操作系统内核启动流程，说明指令 la sp, bootstacktop 完成了什么操作，目的是什么？ tail kern_init 完成了什么操作，目的是什么？

###la sp, bootstacktop
####完成的操作：
la是加载地址指令，用于将某一地址加载到相应的寄存器中，sp是栈指针寄存器，指向栈顶，用于管理函数调用和局部变量，bootstacktop是一个标签，表示栈的初始地址，本条指令将 bootstacktop的地址加载到栈指针寄存器。
####目的：
设置内核栈地址，将栈指针指向栈顶，让操作系统在内核启动过程中使用的栈空间被正确初始化。

###tail kern_init
####完成的操作：
tail伪指令是RISC-V中的尾调用优化指令，用于无条件跳转到一个目标函数并且不会返回，kern_init是目标函数标签，此处使用tail伪指令可以使程序跳转到名为kern_init的函数。
####目的：
操作系统内核初始化的入口点，通过跳转到这个函数，操作系统可以执行各种初始化任务。


##练习2：完善中断处理 （需要编程）

请编程完善trap.c中的中断处理函数trap，在对时钟中断进行处理的部分填写kern/trap/trap.c函数中处理时钟中断的部分，使操作系统每遇到100次时钟中断后，调用print_ticks子程序，向屏幕上打印一行文字”100 ticks”，在打印完10行后调用sbi.h中的shut_down()函数关机。

要求完成问题1提出的相关函数实现，提交改进后的源代码包（可以编译执行），并在实验报告中简要说明实现过程和定时器中断中断处理的流程。实现要求的部分代码后，运行整个系统，大约每1秒会输出一次”100 ticks”，输出10行。

###思路

    
###代码

       case IRQ_S_TIMER:
            // "All bits besides SSIP and USIP in the sip register are
            // read-only." -- privileged spec1.9.1, 4.1.4, p59
            // In fact, Call sbi_set_timer will clear STIP, or you can clear it
            // directly.
            // cprintf("Supervisor timer interrupt\n");
             /* LAB1 EXERCISE2   YOUR CODE : 2210585 2210587 */
            /*(1)设置下次时钟中断- clock_set_next_event()
             *(2)计数器（ticks）加一
             *(3)当计数器加到100的时候，我们会输出一个`100ticks`表示我们触发了100次时钟中断，同时打印次数（num）加一
            * (4)判断打印次数，当打印次数为10时，调用<sbi.h>中的关机函数关机
            */
             clock_set_next_event();  
            
            ticks++; 
            if (ticks == TICK_NUM) {
                print_ticks();  
                ticks = 0;  
                count++;  
                if (count == 10) { 
                    sbi_shutdown();
                }
            }
            break;


###结果截图

![屏幕截图 2024-09-16 202156](https://github.com/user-attachments/assets/e710d8d2-a589-4411-a789-a5927b8e4fb0)

![屏幕截图 2024-09-17 101010](https://github.com/user-attachments/assets/4906c39d-549e-4538-956a-ccb9a4aea420)


##扩展练习 Challenge1：描述与理解中断流程

回答：描述ucore中处理中断异常的流程（从异常的产生开始），其中mov a0，sp的目的是什么？SAVE_ALL中寄寄存器保存在栈中的位置是什么确定的？对于任何中断，__alltraps 中都需要保存所有寄存器吗？请说明理由。

###异常的流程
产生->进入入口点->保存上下文->寄存器传参给trap->调用trap处理->恢复上下文->中断返回

异常的产生:当处理器执行指令时，如果遇到异常情况，例如非法指令等，将触发中断异常。

进入入口点：中断异常发生后，处理器将跳转到预设的异常处理函数__alltraps入口点。

保存上下文：__alltraps中定义的SAVE_ALL宏中的指令会将寄存器依次入栈，包括通用寄存器以及sscratch, sstatus, sepc, sbadaddr, scause等与异常处理信息相关的寄存器，保存上下文信息。

寄存器传参给trap：mov a0，sp，将当前的栈指针的值作为参数传递给接下来调用的函数trap。

调用trap处理：根据中断类型，执行相应的中断处理程序。

恢复上下文：通过__trapret中的RESTORE_ALL宏，从栈中恢复之前保存的寄存器。

中断返回：执行sret指令，将控制权返回到中断发生时的位置，继续执行被中断的指令

###保存位置
将当前的栈指针的值复制到寄存器a0是为了使a0寄存器传递参数给接下来调用的函数trap。栈指针sp在保存上下文时指向了保存的寄存器的栈帧，trap函数通过栈指针来访问保存的寄存器及其他状态信息

###SAVE_ALL
寄存器保存在栈中的位置根据栈指针sp以及偏移量的大小确定，偏移量通过寄存器编号和提前预设的固定字节数REGBYTES来计算。通用寄存器按照从 x0 到 x31 的顺序存储在栈中，sscratch, sstatus, sepc, sbadaddr, scause依次保存在通用寄存器之后。

###__alltraps 
不是，比如一般的时钟中断可能只需要保存一般用途寄存器和程序计数器。也就是说不同类型的中断可能需要不同的寄存器保存策略。有时为了提高性能，可能会采用一种更轻量级的寄存器保存策略。


##扩增练习 Challenge2：理解上下文切换机制

回答：在trapentry.S中汇编代码 csrw sscratch, sp；csrrw s0, sscratch, x0实现了什么操作，目的是什么？save all里面保存了stval scause这些csr，而在restore all里面却不还原它们？那这样store的意义何在呢？

###csrw sscratch, sp
这是保存整个中断上下文的一部分，sscratch 寄存器在中断和异常处理中通常用来存储一些重要的临时值，此处将当前栈指针sp的值写入sscratch控制寄存器。通过保存执行过程中的栈指针，可以让中断处理程序完成后能够恢复到正确的栈，并且能够防止在处理可能的递归异常时丢失当前的栈指针。

###csrrw s0, sscratch, x0
将sscratch控制寄存器的值写入通用寄存器s0，在中断处理程序中可以随时访问而不改变原始值；同时将x0的值写入sscratch控制寄存器，此时寄存器清零，发生递归异常时异常向量就会知道它来自内核。

###save all
scause：表示导致中断或异常的原因，stval：保存了与异常相关的地址值，stval和scause寄存器传递了有关异常或中断的关键信息，内核能以此了解导致中断或异常的原因、与异常相关的地址信息，从而执行相应的处理异常或中断处理。异常处理完成后它们的值就不再具有特定的意义，并不影响程序的继续执行，所以不需要还原它们。

##扩展练习Challenge3：完善异常中断

编程完善在触发一条非法指令异常 mret和，在 kern/trap/trap.c的异常处理函数中捕获，并对其进行处理，简单输出异常类型和异常指令触发地址，即“Illegal instruction caught at 0x(地址)”，“Breakpoint caught at 0x（地址）”与“Exception type:Illegal instruction"，“Exception type: Breakpoint”。

trap.c

	case CAUSE_ILLEGAL_INSTRUCTION:
	    // 非法指令异常处理
	    /* LAB1 CHALLENGE3   YOUR CODE :  */
	    /*(1)输出指令异常类型（ Illegal instruction）
	      *(2)输出异常指令地址
	      *(3)更新 tf->epc寄存器
	    */
            cprintf("Illegal instruction caught at: 0x%08x\n", tf->epc);
	    cprintf("Exception type:Illegal instruction\n");       
            // 更新epc寄存器，跳过导致异常的指令
            tf->epc += 4; 
	    break;
	case CAUSE_BREAKPOINT:
	    //断点异常处理
	    /* LAB1 CHALLLENGE3   YOUR CODE :  */
	    /*(1)输出指令异常类型（ breakpoint）
	      *(2)输出异常指令地址
	      *(3)更新 tf->epc寄存器
	    */
     	    cprintf("Illegal instruction caught at: 0x%08x\n", tf->epc);
	    cprintf("Exception type:Illegal instruction\n");
            // 更新epc寄存器，跳过导致异常的指令
            tf->epc += 4; 
	    break;

init.c

    int kern_init(void) {
    extern char edata[], end[];
    memset(edata, 0, end - edata);

    cons_init();  // init the console

    const char *message = "(THU.CST) os is loading ...\n";
    cprintf("%s\n\n", message);

    print_kerninfo();

    // grade_backtrace();

    idt_init();  // init interrupt descriptor table

    // rdtime in mbare mode crashes
    clock_init();  // init clock interrupt

    intr_enable();  // enable irq interrupt

    asm("ebreak");//使用RISC-V 中的断点指令触发断点异常
    asm volatile(".word 0xFFFFFFFF");  // 插入无效指令，触发非法指令异常
    
    

    while (1)
        ;
    }
