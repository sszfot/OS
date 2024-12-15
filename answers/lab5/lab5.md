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

首先获取源地址和目的地址对应的内核虚拟地址，然后拷贝内存，最后将拷贝完成的页插入到页表中。

```c
uintptr_t* src = page2kva(page);
uintptr_t* dst = page2kva(npage);
memcpy(dst, src, PGSIZE);
ret = page_insert(to, npage, start, perm);
```

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
