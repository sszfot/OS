# lab4:进程管理

当一个程序加载到内存中运行时，首先通过ucore OS的内存管理子系统分配合适的空间，然后就需要考虑如何分时使用CPU来“并发”执行多个程序，让每个运行的程序（这里用线程或进程表示）“感到”它们各自拥有“自己”的CPU。

## 实验目的

了解内核线程创建/执行的管理过程
了解内核线程的切换和基本调度过程

## 实验内容

本次实验将首先接触的是内核线程的管理。内核线程是一种特殊的进程，内核线程与用户进程的区别有两个：

内核线程只运行在内核态
用户进程会在在用户态和内核态交替运行
所有内核线程共用ucore内核内存空间，不需为每个内核线程维护单独的内存空间
而用户进程需要维护各自的用户内存空间

在ucore的调度和执行管理中，对线程和进程做了统一的处理。且由于ucore内核中的所有内核线程共享一个内核地址空间和其他资源，所以这些内核线程从属于同一个唯一的内核进程，即ucore内核本身。

## 重要知识点

## 练习1：分配并初始化一个进程控制块（需要编码）

alloc_proc函数（位于kern/process/proc.c中）负责分配并返回一个新的struct proc_struct结构，用于存储新建立的内核线程的管理信息。ucore需要对这个结构进行最基本的初始化，你需要完成这个初始化过程。（在alloc_proc函数的实现中，需要初始化的proc_struct结构中的成员变量至少包括：state/pid/runs/kstack/need_resched/parent/mm/context/tf/cr3/flags/name。）

### 设计实现过程

```
// 初始化进程状态为未初始化
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
```

在此初始化过程中：
tate 字段：设置为 PROC_UNINIT，表示新创建的进程未初始化。
pid 字段：设置为 -1，稍后分配实际值。
kstack 字段：为内核栈地址，将在分配内核栈后填充。
need_resched：设置为 false，表示当前不需要重新调度。
parent 和 mm 字段：初始值为 NULL，这些会在稍后设置。
清零 context 和 name 字段：确保没有残留数据。

### 请说明proc_struct中struct context context和struct trapframe *tf成员变量含义和在本实验中的作用是啥？（提示通过看代码和编程调试可以判断出来）

**struct context context**：
context 是一个用于保存进程上下文信息的数据结构，主要用于 进程切换 时保存或恢复 CPU 的状态。
它包括寄存器内容（如栈指针 sp 和返回地址 ra），这些信息在进程从当前任务切换到另一个任务时需要保存，以便稍后可以恢复。

###### 在本实验中的作用

- **保存上下文**：当需要切换到另一个进程时，`context` 保存当前进程的 CPU 寄存器状态（如栈指针和返回地址）。
- **恢复上下文**：通过 `switch_to()` 函数切换进程时，使用 `context` 恢复目标进程的寄存器状态，使其从上次暂停的位置继续执行。
- **调度器切换**：在 `proc_run` 中调用 `switch_to` 函数时，`context` 是关键，确保正确保存当前进程的状态并切换到下一个进程。

```
proc->context.ra = (uintptr_t)forkret;
proc->context.sp = (uintptr_t)(proc->tf);
```

ra：设置为 `forkret`，表示进程在被调度运行时从 `forkret` 函数开始。

sp：设置为 proc->tf 的地址，表示新的栈顶。

**struct trapframe *tf**：

- `trapframe` 是一个结构体，保存了处理器在陷入中断或异常时的寄存器状态。
- 它包括通用寄存器的值、程序计数器 `pc`、栈指针 `sp` 等。
- 它用于处理器从用户态或内核态陷入中断时保存处理器状态，以及在异常处理完成后恢复状态。

###### 在本实验中的作用

- **中断处理**：`trapframe` 保存了在中断发生时的 CPU 状态，确保可以在处理完成后恢复之前的执行状态。
- **进程切换**：在 `copy_thread` 中，`tf` 被分配到内核栈顶，用于初始化新进程的状态，保证新进程在被第一次调度运行时能够正确执行。
- **新进程初始化**：在 `copy_thread` 中设置 `proc->tf`，并将其内容初始化为传入的 `tf`，让新进程从一个特定的位置开始执行。

```
proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE - sizeof(struct trapframe));
*(proc->tf) = *tf;
```

将 `tf` 指向分配在内核栈顶的 `trapframe`。

通过 `*(proc->tf) = *tf` 复制父进程的 `trapframe` 信息。

## 练习2：为新创建的内核线程分配资源（需要编码）

创建一个内核线程需要分配和设置好很多资源。kernel_thread函数通过调用do_fork函数完成具体内核线程的创建工作。do_kernel函数会调用alloc_proc函数来分配并初始化一个进程控制块，但alloc_proc只是找到了一小块内存用以记录进程的必要信息，并没有实际分配这些资源。ucore一般通过do_fork实际创建新的内核线程。do_fork的作用是，创建当前内核线程的一个副本，它们的执行上下文、代码、数据都一样，但是存储位置不同。因此，我们实际需要"fork"的东西就是stack和trapframe。在这个过程中，需要给新内核线程分配资源，并且复制原进程的状态。你需要完成在kern/process/proc.c中的do_fork函数中的处理过程。它的大致执行步骤包括：

```
调用alloc_proc，首先获得一块用户信息块。
为进程分配一个内核栈。
复制原进程的内存管理信息到新进程（但内核线程不必做此事）
复制原进程上下文到新进程
将新进程添加到进程列表
唤醒新进程
返回新进程号
```

### 设计过程

#### **步骤 1：调用 `alloc_proc`**

- 使用 `alloc_proc` 分配并初始化 `proc_struct`，为新进程记录基本信息。
- 如果分配失败，则返回错误码 `-E_NO_MEM`。

#### **步骤 2：分配内核栈**

- 调用 `setup_kstack` 为新进程分配一个内核栈。
- 如果失败，释放已分配的 `proc_struct` 并退出。

#### **步骤 3：复制内存管理信息**

- 调用 `copy_mm` 函数，复制当前进程的内存管理信息到新进程。
  - 对于内核线程（`clone_flags` 带有 `CLONE_VM`），可能直接共享内存。
  - 对于用户进程，则需要复制内存。
- 如果失败，清理分配的资源。

#### **步骤 4：复制上下文和 `trapframe`**

- 调用 `copy_thread`，将当前进程的上下文和 `trapframe` 复制到新进程。
- 设置 `proc->context` 和 `proc->tf`，确保新进程的运行状态正确。

#### **步骤 5：唤醒新进程**

- 调用 `wakeup_proc`，将新进程的状态设置为 `PROC_RUNNABLE`。
- 将新进程插入到哈希表 `hash_list` 和全局进程列表 `proc_list` 中。

#### **步骤 6：返回新进程 PID**

- 返回新进程的 PID，表示新进程创建成功。

具体来说：

**`alloc_proc`**：分配并初始化一个新的进程控制块。

**`setup_kstack`**：为新进程分配一个内核栈，提供运行所需的栈空间。

**`copy_mm`**：复制或共享内存管理信息（对于内核线程，可以直接跳过此步）。

**`copy_thread`**：复制上下文和中断帧，确保新进程能够从正确的位置运行。

**`hash_proc`** 和 **`list_add`**：将新进程添加到调度器的管理结构中。

**`wakeup_proc`**：将新进程状态设置为可运行。

### 请说明ucore是否做到给每个新fork的线程一个唯一的id？请说明你的分析和理由。

在 uCore 中，每个新 fork 的线程确实会分配一个唯一的 ID (`pid`)，在get_pid中进行分配：

```
static int
get_pid(void) {
    static_assert(MAX_PID > MAX_PROCESS);
    struct proc_struct *proc;
    list_entry_t *list = &proc_list, *le;
    static int next_safe = MAX_PID, last_pid = MAX_PID;

    // 分配下一个 PID，循环检查是否唯一
    if (++last_pid >= MAX_PID) {
        last_pid = 1;
        goto inside;
    }

    if (last_pid >= next_safe) {
    inside:
        next_safe = MAX_PID;
    repeat:
        le = list;
        while ((le = list_next(le)) != list) {
            proc = le2proc(le, list_link);
            if (proc->pid == last_pid) {
                if (++last_pid >= next_safe) {
                    if (last_pid >= MAX_PID) {
                        last_pid = 1;
                    }
                    next_safe = MAX_PID;
                    goto repeat;
                }
            } else if (proc->pid > last_pid && next_safe > proc->pid) {
                next_safe = proc->pid;
            }
        }
    }
    return last_pid;
}
```

在循环中，当所有可用的 `pid` 都被分配时，`get_pid` 会重置 `last_pid` 为 1，并重新遍历，寻找可以使用的 `pid`。`get_pid` 会遍历当前所有的进程，确保新分配的 `pid` 没有冲突。即使是并发环境下，`pid` 分配也会通过互斥机制避免重复。并且，如果一个线程退出，其 `pid` 可以被回收并重新分配给新的线程。因此`pid` 的唯一性只在某一时刻内保证，而不一定在整个系统运行期间保持完全独立。

## 练习3：编写proc_run 函数（需要编码）

proc_run用于将指定的进程切换到CPU上运行。它的大致执行步骤包括：

检查要切换的进程是否与当前正在运行的进程相同，如果相同则不需要切换。
禁用中断。你可以使用/kern/sync/sync.h中定义好的宏local_intr_save(x)和local_intr_restore(x)来实现关、开中断。
切换当前进程为要运行的进程。
切换页表，以便使用新进程的地址空间。/libs/riscv.h中提供了lcr3(unsigned int cr3)函数，可实现修改CR3寄存器值的功能。
实现上下文切换。/kern/process中已经预先编写好了switch.S，其中定义了switch_to()函数。可实现两个进程的context切换。
允许中断。

```
void
proc_run(struct proc_struct *proc) {
    if (proc != current) { // 如果要切换的进程不是当前进程
        // LAB4:EXERCISE3 YOUR CODE

        // 1. 禁用中断并保存中断状态
        unsigned long flags;
        local_intr_save(flags);

        // 2. 切换当前进程为指定进程
        struct proc_struct *prev = current;
        current = proc;

        // 3. 切换页表，修改CR3寄存器
        lcr3(proc->cr3);

        // 4. 执行上下文切换
        switch_to(&(prev->context), &(current->context));

        // 5. 恢复中断状态
        local_intr_restore(flags);
    }
}
```

- 如果要运行的进程 `proc` 已经是当前运行的进程，则无需切换。

- 将当前运行的进程指针 `current` 切换为目标进程 `proc`。

- 使用 `prev` 保存原先的进程，便于在上下文切换时访问它的上下文。

- 调用 `local_intr_save(flags)` 保存当前的中断状态并禁用中断。

- 在上下文切换过程中禁用中断，避免调度器或中断引起的不一致。

- 调用 `lcr3` 修改 CR3 寄存器的值，以切换页表到目标进程的地址空间。

- `proc->cr3` 应该包含目标进程的页目录表基地址。

- 调用 `switch_to`，从 `prev` 的上下文切换到 `current` 的上下文。

- `switch_to` 是汇编实现的函数，负责保存当前寄存器状态并加载新进程的寄存器状态。

- 调用 `local_intr_restore(flags)` 恢复之前保存的中断状态。

- 允许中断重新处理，确保系统正常运行。

### 关键在于以下三点

1. **上下文切换的实现**
   
   - `switch_to` 负责切换寄存器状态，包括栈指针、程序计数器等。
   - 切换完成后，目标进程从上次被切换时暂停的位置继续运行。

2. **页表切换的重要性**
   
   - 通过修改 CR3 寄存器，确保目标进程访问其自己的地址空间。

3. **中断管理**
   
   - 在切换过程中禁用中断，避免中断引起的状态不一致。
   - 切换完成后恢复中断状态。

### 在本实验的执行过程中，创建且运行了几个内核线程？

在本实验中，共创建了 **两个内核线程**：

- **`idleproc`**：第一个内核线程，负责初始化调度器并维持系统的基本运行。

- **`initproc`**：第二个内核线程，作为初始化线程执行具体任务
  
  以下是具体信息：

### 1）idleproc

- **创建位置**：在 `proc_init()` 函数中。
- **作用**：系统的第一个内核线程，负责初始化系统并运行调度器。
- **特性**：
  - PID 为 0。
  - 其 `state` 被设置为 `PROC_RUNNABLE`。
  - 它是 uCore 系统启动后第一个运行的线程。

### 2）initproc

- **创建位置**：在 `proc_init()` 函数中，通过调用 `kernel_thread(init_main, "Hello world!!", 0)` 创建。
- **作用**：作为系统的初始化线程，负责执行用户程序的初始化任务。
- **特性**：
  - PID 为 1。
  - 使用 `kernel_thread` 创建，并设置了初始名称和运行入口函数。
  - 创建完成后，其 `state` 被设置为 `PROC_RUNNABLE`。

## 对以上三个练习运行后的结果截图

![](C:\Users\lenovo\AppData\Roaming\marktext\images\2024-12-06-15-30-17-49db26e986388b9ddd4264bd5bc6a50.png)

![](C:\Users\lenovo\AppData\Roaming\marktext\images\2024-12-06-15-30-25-bb6f89194a4c936bff77e50d0a62caf.png)

## 扩展练习 Challenge

## 说明语句local_intr_save(intr_flag);....local_intr_restore(intr_flag);是如何实现开关中断的？

`local_intr_save(intr_flag)` 和 `local_intr_restore(intr_flag)` 是用于开关中断的宏或函数。这些语句通过操作处理器的中断状态寄存器实现开关中断的功能。

### **1. `local_intr_save(intr_flag)` 的作用和实现**

#### **作用**

- 保存当前的中断状态到变量 `intr_flag` 中。
- 禁用当前 CPU 的中断。

#### **实现原理**

1. **读取当前中断状态**：
   
   - 使用汇编或特定的内核函数（如 `read_csr`）读取处理器的状态寄存器（如 RISC-V 的 `sstatus` 或 x86 的 `EFLAGS`）。
   - 在 RISC-V 中，`sstatus` 中的 `SIE` 位表示中断是否启用。
     - `SIE = 1`：中断启用。
     - `SIE = 0`：中断禁用。

2. **保存状态到 `intr_flag`**：
   
   - 保存当前状态到局部变量 `intr_flag` 中，便于后续恢复。

3. **禁用中断**：
   
   - 将 `SIE` 位清零，禁用中断。

### **2. `local_intr_restore(intr_flag)` 的作用和实现**

#### **作用**

- 根据 `intr_flag` 的值恢复中断状态。
- 如果在 `local_intr_save` 时中断被禁用，则恢复为禁用；如果原来中断启用，则恢复为启用。

#### **实现原理**

1. **读取 `intr_flag`**：
   
   - 从变量 `intr_flag` 中读取保存的状态。

2. **恢复中断状态**：
   
   - 将 `intr_flag` 的值写回处理器的状态寄存器。
   - 在 RISC-V 中，通过设置 `SIE` 位来恢复中断。

### **3. 流程**

1. **保存并禁用中断**：

```
unsigned long intr_flag;
local_intr_save(intr_flag);
// 进入关键代码区，中断已被禁用
```

    2.**执行关键代码**：

- 此时中断被禁用，当前代码不会被打断。

    3.**恢复中断**：

```
local_intr_restore(intr_flag);
// 恢复原来的中断状态
```

总结以上，关键点在于：

- **中断状态寄存器（CSR）**：
  
  - 在 RISC-V 中，`sstatus` 是保存中断状态的控制寄存器。
  - 在 x86 中，对应的是 `EFLAGS` 寄存器中的 `IF` 位。

- **宏实现的高效性**：
  
  - `local_intr_save` 和 `local_intr_restore` 使用内联汇编或快速的寄存器操作，保证了性能。

- **线程/进程安全性**：
  
  - 通过禁用中断保护临界区，防止多线程或多进程竞争资源导致的数据不一致。

实际应用：禁用中断可以防止关键代码区被中断打断，保证代码的原子性，或是在进程切换中，临时禁用中断以避免调度器和中断冲突。