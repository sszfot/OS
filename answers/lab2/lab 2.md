#lab 2
本次的实验主要是在实验一的基础上完成物理内存管理，并建立一个最简单的页表映射。
##知识点
本实验中重要的知识点：

内存分配：
连续内存分配要求每个进程的地址空间必须是连续的物理内存。操作系统将物理内存划分为一个个连续的区域，每个进程从中获得一段连续的内存区域来执行代码和存储数据。随着进程的频繁加载和卸载，内存中的空闲块变得不连续，产生外部碎片。尽管内存空闲总量可能很大，但分散的小块空闲区域可能不足以满足大进程的需求。分页内存分配是现代操作系统中使用的内存分配方式，它将物理内存和进程的虚拟内存空间都划分为固定大小的块。进程的虚拟地址空间不再要求是连续的物理地址空间，而是通过页表实现虚拟地址到物理地址的映射。
内存回收：内存回收是指在程序不再需要某些内存时，将其释放回系统，以便其他程序或任务可以使用。
碎片处理：碎片化是指内存分配和回收过程中，系统的空闲内存被分割成多个不连续的小块，导致较大的内存请求无法得到满足。内部碎片是由于分配的内存块大于实际需要的内存造成的浪费。外部碎片是指虽然有足够的空闲内存，但由于空闲块是不连续的，无法满足较大的内存分配请求。伙伴系统将内存按照 2 的幂次分成块，当一个块被释放时，如果与其相邻的块也是空闲的，可以合并成更大的块。通过实现伙伴系统可以用于进行碎片处理。

OS原理中很重要，但在实验中没有对应上的知识点：

##练习1：理解first-fit 连续物理内存分配算法
kern/mm/default_pmm.c中的相关代码，认真分析default_init，default_init_memmap，default_alloc_pages， default_free_pages等相关函数
###描述程序在进行物理内存分配的过程以及各个函数的作用。
分配过程基于首次适应（First-Fit）算法
####过程
1.初始化时，系统调用 default_init 函数初始化空闲链表和计数器。
2.系统根据内存布局调用 default_init_memmap 函数，将一段内存块注册为可用的空闲内存并将其加入进行管理。
3.当用户请求内存时，default_alloc_pages 函数会遍历空闲链表，找到第一个满足请求的块，并分配给用户，同时调整剩余块的大小。当没有合适的块时，系统不会分配任何内存，并返回 NULL 表示分配失败。
4.需要释放内存时，default_free_pages 函数会将内存块重新放入空闲链表，并尝试合并相邻的空闲块，减少碎片化。

####default_init（内存管理器的初始化）
该函数的主要任务是初始化内存的空闲链表 free_list，用来存储所有空闲的内存块，并将记录空闲页面数量的变量 nr_free 置为 0，供后续使用。

####default_init_memmap（内存块的添加）
负责将一段内存标记为可用并插入空闲列表中，这样这段内存就可以参与内存分配。base：表示要初始化的内存块的起始页，n：表示该内存块包含的页数。assert(n > 0)：确保传入的页数 n 是有效的。页初始化循环过程中，遍历 base 起始页到 base + n 结束的每一页，assert(PageReserved(p))：确保页已经被保留并且未使用；p->flags = p->property = 0：清除每页的标志和属性字段；set_page_ref(p, 0)：将页的引用计数设置为 0，表示该页当前没有被使用；base->property = n：设置内存块的第一个页的 property 为 n，表示这是一个包含 n 页的连续空闲块；SetPageProperty(base)：将页的 PG_property 标志位设置为 1，表示该页是一个具有属性的空闲块。同时对全局空闲页数计数进行了更新。接下来实现页的添加操作，list_empty(&free_list)：检查 free_list 是否为空链表。如果是，则直接将该块作为第一个块插入链表，然后遍历 free_list，查找适当的位置插入新的内存块。

####default_alloc_pages（分配物理内存页）
通过遍历空闲链表，找到第一个能满足分配需求的内存块，并将它分配给用户，确保物理内存的有效利用。首先对页数的有效性进行检查，确保请求的页数大于0并且不超过当前系统中的空闲页数。然后遍历 free_list 链表，使用le2page 从链表节点 le 中提取出对应的 Page 结构，如果找到了一个拥有至少 n 页的空闲块，则记录该块，并退出循环。如果找到了合适的块，使用 list_del 将该块从 free_list 中删除，如果该块的大小比请求的页数 n 大，则前 n 页分配给请求，剩下的页重新插入链表，并从 nr_free 中减去分配的页数 n。

####default_free_pages（释放物理内存页）
将每页的 flags 和 property 置为 0，标记它们为未使用。将该块插入空闲链表中，保持按地址排序。尝试，合并相邻的空闲块，先尝试与前一个块合并，如果前一个块的结束地址等于当前块的起始地址，则合并两个块；再尝试与后一个块合并，如果当前块的结束地址等于下一个块的起始地址，则合并两个块。总的来说就是将内存块归还给系统并尝试合并相邻的内存块，以减少内存碎片化。

###设计实现过程。
代码中主要通过 default_alloc_pages(size_t n) 函数来实现First Fit算法。在这个函数中，程序会遍历存储空闲内存块的链表 free_list，找到第一个满足条件的内存块，将其分配并根据需要调整空闲链表。如果找到的空闲块比请求的还大，那么它会把多余的部分重新放回到空闲列表中。
大致过程就是：搜索合适的块、分配前n页、拆解比需求大的块重新插入链表、返回页面起始地址。

###你的first fit算法是否有进一步的改进空间？

从提高算法的效率和内存利用率方面考虑：

####维护不同大小的空闲块链表解决线性搜索方式在大量的内存块或内存分配/释放频繁的情况
遍历整个空闲链表寻找满足请求的块在系统负载较高时可能变得耗时。如果内存块很多，线性查找的开销也会增加。通过为空闲块建立索引或哈希表，可以加速查找过程。

####Best Fit 和 First Fit 的结合解决产生大量小碎片，导致后续的分配失败的问题
通过后台进程，周期性地将相邻的小块合并成大的块，将分散的内存碎片合并，释放出较大的连续内存块，便于后续大块内存分配。在每次释放内存时，除了合并相邻的空闲块之外，也可以考虑是否有必要触发碎片整理操作

####分页机制的优化解决如果频繁分配和释放不同大小的内存块，可能会导致内存的不均衡分布的问题。
根据系统当前的内存使用情况，调整内存块的管理方式，内存较为充裕时，允许更大的块分配；当内存紧张时，使用更严格的分配策略以减少碎片化。但这种动态调整的方法需要一定的性能开销，可能会带来新的问题。

##练习2：实现 Best-Fit 连续物理内存分配算法（需要编程）
参考kern/mm/default_pmm.c对First Fit算法的实现，编程实现Best Fit页面分配算法
###设计实现过程
1.遍历空闲链表，寻找所有能够满足分配需求的内存块，并选择其中最小的那个块进行分配。
2.如果找到的块大于需求，则将剩余部分重新插入到空闲链表中。
3.如果没有找到合适的块，则返回 NULL。

实现结果：
make qemu：
![屏幕截图 2024-10-13 143124](https://github.com/user-attachments/assets/b158ac5c-54ba-44aa-8e6e-f356c8dbcb53)
make grade：
![屏幕截图 2024-10-13 143235](https://github.com/user-attachments/assets/c9b8fe39-b338-4db9-8a91-dd9266b53fbc)

###阐述代码是如何对物理内存进行分配和释放
####分配
检查：首先，函数检查当前系统中的空闲页数 nr_free 是否大于或等于请求的页数 n。如果空闲页数不足，直接返回 NULL，表示分配失败。

查找：从 free_list 链表的头开始遍历，查找能够满足请求大小的所有块。它记录找到的最小的满足条件的块（即 p->property >= n 且最小的块）。

分配：如果块的大小正好等于请求的大小，则直接分配整个块。如果块的大小大于请求的大小，则将剩余的部分拆分出来，并将剩余部分重新插入空闲链表。

更新：减少全局空闲页数 nr_free，清除已分配块的 PG_property 标志，最后返回分配的内存块的起始地址。
####释放

###你的 Best-Fit 算法是否有进一步的改进空间？


##扩展练习Challenge：buddy system（伙伴系统）分配算法（需要编程）
参考伙伴分配器的一个极简实现， 在ucore中实现buddy system分配算法，要求有比较充分的测试用例说明实现的正确性，需要有设计文档。

##扩展练习Challenge：任意大小的内存单元slub分配算法（需要编程）
slub算法，实现两层架构的高效内存单元分配，第一层是基于页大小的内存分配，第二层是在第一层基础上实现基于任意大小的内存分配。可简化实现，能够体现其主体思想即可。
###参考linux的slub分配算法/，在ucore中实现slub分配算法。要求有比较充分的测试用例说明实现的正确性，需要有设计文档。

##扩展练习Challenge：硬件的可用物理内存范围的获取方法（思考题）
###如果 OS 无法提前知道当前硬件的可用物理内存范围，请问你有何办法让 OS 获取可用物理内存范围？
通常，可以通过硬件提供的机制、固件接口、或者通过检测的方式让操作系统获取系统中的物理内存范围。
1.在基于 BIOS 的系统中，操作系统可以通过调用 INT 0x15, EAX=0xE820 来获取系统内存的布局。BIOS 会返回一个包含内存段类型和大小的列表，操作系统可以根据这个列表来知道哪些内存区域可用。
在基于 UEFI 的系统中，操作系统可以通过 UEFI Boot Services 提供的 GetMemoryMap() 函数来获取内存布局。UEFI 内存映射包含每个内存段的物理地址、大小和类型。

2.Multiboot 是一种操作系统引导协议，支持在不同硬件平台上引导操作系统。许多现代引导加载程序（如 GRUB）都支持 Multiboot 协议。操作系统通过 Multiboot 引导程序传递的信息来获取物理内存范围。

实现方式： Multiboot 引导加载程序会在启动时向操作系统传递内存映射等信息。操作系统可以在初始化时通过读取 Multiboot 信息来获取可用的物理内存范围。

3.ACPI（高级配置与电源接口）是现代计算机硬件配置管理的重要标准之一。ACPI 提供了丰富的硬件信息，操作系统可以通过 ACPI 表来获取内存信息。

实现方式： 操作系统可以读取 ACPI SRAT（系统资源分配表） 来获取物理内存布局。SRAT 表包含了可用的物理内存和每个内存块的类型。

4.手动检测（内存探测）

    概述： 操作系统还可以通过手动探测物理内存范围，尝试访问某些内存区域并根据是否成功来判断该区域是否可用。这个方法较为原始，通常用作辅助或者最后的尝试。

    实现方式： 操作系统可以尝试向某些物理地址写入和读取数据，如果操作成功且数据一致，说明该区域是可用的。否则，该区域可能已经被系统或硬件保留。

5.设备树（Device Tree）

    概述： 在某些嵌入式系统（如 ARM 架构）中，设备树（Device Tree） 可以提供硬件配置信息，包括可用的物理内存范围。设备树是一种数据结构，包含了内存和其他硬件资源的信息。

    实现方式： 操作系统在启动时解析设备树文件，并从中提取物理内存的相关信息。