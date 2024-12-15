<h1><center>lab5实验报告</center></h1>

## 练习0:填写已有实验

+ 在`vmm.c的do_pgfault函数`中添加：

  ```c
            swap_in(mm,addr,&page);
            page_insert(mm->pgdir,page,addr,perm);
            swap_map_swappable(mm,addr,page,1);
            page->pra_vaddr = addr;
  ```



+ 填写`alloc_proc`并在`alloc_proc`中添加额外的初始化：

  ```c
        proc->state = PROC_UNINIT;
        // 获取一个唯一的 PID
        proc->pid = -1; // 在稍后调用 get_pid 时分配实际 PID
        // 初始化运行次数为 0
        proc->runs = 0;
        // 分配内核栈
        proc->kstack = 0; // 使用 setup_kstack 函数时会赋值
        // 初始化是否需要重新调度的标志为 false
        proc->need_resched = 0;
        // 设置父进程为 NULL
        proc->parent = NULL;
        // 初始化内存管理字段为 NULL
        proc->mm = NULL;
        // 初始化上下文
        memset(&(proc->context), 0, sizeof(struct context));
        // 初始化中断帧为 NULL
        proc->tf = NULL;
        // 设置 CR3 寄存器值为 0，稍后分配页目录时会更新
        proc->cr3 = boot_cr3;
        // 初始化标志位为 0
        proc->flags = 0;
        // 初始化进程名称为空字符串
        memset(proc->name, 0, sizeof(proc->name));
        proc->wait_state = 0;
        proc->cptr = NULL;// Child Pointer 表示当前进程的子进程
        proc->optr = NULL;// Older Sibling Pointer 表示当前进程的上一个兄弟进程
        proc->yptr = NULL;// Younger Sibling Pointer 表示当前进程的下一个兄弟进程
  ```

+ 在`do_fork`中修改代码如下：

  ```c
    proc = alloc_proc();
    if (proc == NULL) {
        goto fork_out;
    }
    proc->parent = current;
    assert(current->wait_state == 0);
    //为进程分配一个内核栈
    if (setup_kstack(proc) != 0) {
        goto bad_fork_cleanup_proc;
    }

    //复制内存管理信息
    if (copy_mm(clone_flags, proc) != 0) {
        goto bad_fork_cleanup_kstack;
    }
    
    copy_thread(proc, stack, tf);
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        proc->pid = get_pid();
        set_links(proc);
        hash_proc(proc);
        
    }
    local_intr_restore(intr_flag);
    wakeup_proc(proc);
    ret = proc->pid;
  ```

## 练习一:加载应用程序并执行

### 代码

将`sp`设置为栈顶，`epc`设置为文件的入口地址，`sstatus`的`SPP`位清零，代表异常来自用户态，之后需要返回用户态；`SPIE`位清零，表示不启用中断。

```c
tf->gpr.sp = USTACKTOP;
tf->epc = elf->e_entry;
tf->status = sstatus & ~(SSTATUS_SPP | SSTATUS_SPIE);
```

### 执行过程

1. 在`init_main`中通过`kernel_thread`调用`do_fork`创建并唤醒线程，
2. 执行函数`user_main`，这时该线程状态已经为`PROC_RUNNABLE`，表明该线程开始运行
3. 在`user_main`中调用`kernel_execve`
4. 在`kernel_execve`中执行`ebreak`，发生断点异常跳转到`CAUSE_BREAKPOINT`
5. 在`CAUSE_BREAKPOINT`处调用`syscall`，执行`sys_exec`，调用`do_execve`
6. 在`do_execve`中调用`load_icode`，加载文件
7. 加载完毕后返回，直到`__alltraps`的末尾，接着执行`__trapret`后的内容，到`sret`，表示退出S态，回到用户态执行，这时开始执行用户的应用程序

## 练习二：父进程复制自己的内存空间给子进程

### 代码

首先获取源地址和目的地址对应的内核虚拟地址，然后拷贝内存，最后将拷贝完成的页插入到页表中。copy_range函数的核心任务是复制父进程的内存地址空间到子进程，确保子进程获得一个与父进程相同的内存映像，同时保留原有的内存结构和内容。实现过程中需要考虑内存段的正确性和效率。

```c
uintptr_t* src = page2kva(page);
uintptr_t* dst = page2kva(npage);
memcpy(dst, src, PGSIZE);
ret = page_insert(to, npage, start, perm);
```
具体操作如下：

    获取源页面和目标页面的虚拟地址：
        page2kva(page)：将物理页面 page 转换为其对应的虚拟地址。
        page2kva(npage)：将物理页面 npage 转换为其对应的虚拟地址。

    复制页面内容：
        memcpy(dst, src, PGSIZE)：将源页面的内容（PGSIZE 大小）复制到目标页面中。

    插入页面映射：
        page_insert(to, npage, start, perm)：将目标页面 npage 映射到 to 地址空间中的虚拟地址 start，并设置相应的权限 perm。

####如何设计实现Copy on Write机制？
Copy-on-Write (COW)机制是一种优化内存复制的技术，特别适用于fork场景。在fork时，父子进程共享同一块物理内存，但以只读方式共享，直到其中一个进程试图写入时才触发真正的内存复制操作。
实现步骤

    fork时设置只读共享
        在do_fork函数中，为父进程和子进程的所有内存页标记为只读共享，更新页表中对应页的权限位为read-only。
        使用引用计数记录每个物理页被共享的次数。

    修改页表设置
        父进程和子进程的页表条目指向相同的物理页。
        设置PTE_W（可写标志位）为0，表示只读权限。
        增加引用计数器，记录该页的共享次数。

    （Copy-on-Write）
        当父进程或子进程试图写入一块共享的物理页时，会触发Page Fault。
        在页错误处理函数中检查页是否标记为只读且可共享。
        如果是：
            分配一块新的物理页。
            将原物理页的内容复制到新分配的页中。
            更新页表，将虚拟地址映射到新的物理页，并设置为可写。
            减少原物理页的引用计数，如果引用计数归零则释放该页。

    最终内存释放
        当父进程或子进程终止时，遍历页表，释放其占用的物理页。
        如果一个页的引用计数大于1，仅减少计数而不释放；只有当计数归零时，释放物理页。

### 实验结果
![屏幕截图 2024-12-15 143611](https://github.com/user-attachments/assets/b03b7b6e-8428-43d7-ad18-ee4b498ee339)
![屏幕截图 2024-12-15 143554](https://github.com/user-attachments/assets/0eed3310-5434-4e56-bb29-ac7d11f909b9)



## 练习三：阅读分析源代码，理解进程执行 fork/exec/wait/exit 的实现，以及系统调用的实现

### 函数分析

1. `fork`：通过发起系统调用执行`do_fork`函数。用于创建并唤醒线程，可以通过`sys_fork`或者`kernel_thread`调用。首先初始化一个新线程并为新线程分配内核栈空间，然后为新线程分配新的虚拟内存或与其他线程共享虚拟内存，
   之后获取原线程的上下文与中断帧，设置当前线程的上下文与中断帧将新线程插入哈希表和链表中，最终唤醒新线程并返回线程`id`
2. `exec`：通过发起系统调用执行`do_execve`函数。用于创建用户空间，加载用户程序。通过`sys_exec`调用。主要完成回收当前线程的虚拟内存空间并为当前线程分配新的虚拟内存空间并加载应用程序
3. `wait`：通过发起系统调用执行`do_wait`函数。用于等待线程完成，可以通过`sys_wait`或者`init_main`调用。先查找状态为`PROC_ZOMBIE`的子线程；如果查询到拥有子线程的线程，则设置线程状态并切换线程；如果线程已退出，则调用`do_exit`
   然后将线程从哈希表和链表中删除，释放线程资源。
5. `exit`：通过发起系统调用执行`do_exit`函数。用于退出线程，可以通过`sys_exit`、`trap`、`do_execve`、`do_wait`调用。若当前线程的虚拟内存没有用于其他线程，则销毁该虚拟内存将当前线程状态设为`PROC_ZOMBIE`，然后唤醒该线程的父线程，调用`schedule`切换到其他线程

### 执行流程

+ 系统调用部分在内核态进行，用户程序的执行在用户态进行
+ 内核态使用系统调用结束后的`sret`指令切换到用户态，用户态发起系统调用产生`ebreak`异常切换到内核态
+ 内核态执行的结果通过`kernel_execve_ret`将中断帧添加到线程的内核栈中，将结果返回给用户


### 用户程序加载

该用户程序在操作系统加载时一起加载到内存里。平时使用的程序在操作系统启动时仍然位于磁盘中，只有当我们需要运行该程序时才会被加载到内存。

原因：`Makefile`执行了`ld`命令，把执行程序的代码连接在了内核代码的末尾

## 扩展练习 Challenge
1.实现 Copy on Write （COW）机制 
主要涉及以下几个模块：

    页表管理模块：为页表添加只读标志位，并处理共享页的标记。
    页面异常处理模块：处理写时复制的逻辑，触发 Page Fault 异常后完成页面拷贝。
    进程管理模块：在创建子进程时设置共享页。
详细请参见COW设计报告
2.说明该用户程序是何时被预先加载到内存中的？与我们常用操作系统的加载有何区别，原因是什么？

1）在ucore实验中，用户程序通常是以ELF格式的二进制文件保存在磁盘或内存中。当系统启动时，ucore会通过load_icode函数加载应用程序的执行文件，解析其中的代码段、数据段，并将其映射到用户态的虚拟地址空间中。具体来说就是用户程序在do_execv调用时通过load_icode被加载到内存中。do_execv会在内核态创建并初始化一个新的用户态进程，然后调用load_icode解析ELF文件并将代码段、数据段加载到内存，同时设置入口点地址和trapframe中的CPU寄存器值（如eip、esp）。总之，用户程序的加载是在进程第一次运行之前，由系统调用exec的实现完成的（在内核态）。

2）区别：与现代操作系统（如Linux或Windows）相比，ucore中的用户程序加载机制存在显著区别，主要原因在于系统的设计简化和对实验环境的特殊需求：

    *这里是预加载，而常用操作系统为按需加载：
        ucore：用户程序在创建用户进程时一次性加载到内存中，包括代码段、数据段和堆栈等全部内容。这种方式简单易实现，但会浪费一定的内存空间，无法做到内存利用的优化。
        常见操作系统：采用按需加载机制（demand paging）。程序初次运行时只加载必要的部分（如入口点对应的代码段），其余部分在执行时由页错误（page fault）触发，通过内存管理单元（MMU）和内核的页表机制动态加载。这种方式提高了内存使用效率，特别是在多任务环境中。

    *加载位置不同：
        ucore：由于实验环境中通常没有复杂的文件系统，用户程序可能直接被编译到一个固定位置（如嵌入在内核的映像文件中），或者通过简单的文件加载到内存。
        常见操作系统：用户程序一般存储在磁盘文件系统中，通过操作系统的文件管理模块加载到内存中，同时结合内存映射技术（memory mapping）将部分程序段与磁盘页面相关联。

原因分析：ucore的实验环境设计为教学目的，主要用于理解操作系统基本原理，简化了许多现代操作系统中的优化机制。例如：1.没有复杂的文件系统和页表机制，因此需要一次性加载用户程序到内存。2.采用静态加载而非动态链接库（dynamic linking）或动态加载技术。3. 常见操作系统注重性能和内存效率，因此采用按需加载机制。
