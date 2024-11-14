# LRU页面置换算法设计文档

#### 1. 算法简介

LRU（Least Recently Used）页面置换算法通过跟踪页面的最近使用情况来决定页面替换。每次缺页时，LRU 会选择最近未被访问的页面进行替换，从而减少因频繁访问页面被误替换的情况。相比于 FIFO 和 Clock 算法，LRU 更符合“实际使用情况”，适合页面访问模式具有时间局部性的工作负载。

#### 2. 数据结构设计

- **双向链表** `lru_list`：
  - 使用一个双向链表来存储所有加载到内存中的页面。
  - 链表头部存储最近被访问的页面，链表尾部存储最近最少使用的页面。
- **Page 结构体**：
  - 扩展 `Page` 结构体，添加一个链表节点（`list_entry_t page_link`），用于将页面加入到 `lru_list` 中。

#### 3. 核心函数设计

以下是实现 LRU 页面置换的关键函数设计：

- **lru_init()**：
  
  - 初始化 LRU 链表 `lru_list`，清空链表并准备接受页面。

- **lru_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)**：
  
  - 将指定的页面加入到 `lru_list` 链表的头部，表示页面最近被访问。
  - 访问页面时，首先判断页面是否已在链表中，若在链表中则将其移至链表头部，否则添加到链表头部。

- **lru_swap_out(struct mm_struct *mm, int *swap_in)**：
  
  - 从 `lru_list` 链表的尾部选择最近最少使用的页面作为替换页面。
  - 将尾部页面从链表中移除，并返回该页面用于后续的换出操作。

- **辅助函数**：
  
  - `list_add()` 和 `list_del()`：用于链表的基本插入和删除操作。
  
  - - - 
  
  

#### 4. 流程设计

- **页面加载**：
  
  - 页面被加载时调用 `lru_map_swappable()`，将页面加入链表头部，表示它是最近被访问的。
  - 如果页面已存在于链表中，则直接将页面移至链表头部。

- **页面替换**：
  
  - 当内存不足时触发 `lru_swap_out()`，从 `lru_list` 的尾部获取最久未使用的页面。
  - 将页面从链表中删除后进行换出操作。
  
  我们认为核心在于修改系统的内存管理模块，使得它能够跟踪和替换最近最少使用的页。LRU 算法的核心思想是：当需要替换一页时，总是选择最近最少被访问的页。这通常可以通过维护一个双向链表来实现，链表的头部存放最近访问的页，尾部存放最少访问的页。
  
  #### 1.定义并初始化 `lru_swap_manager`
  
  ```
  struct swap_manager lru_swap_manager = {
      .init = lru_init,
      .map_swappable = lru_map_swappable,
      .set_unswappable = lru_set_unswappable,
      .swap_out = lru_swap_out,
      .tick_event = lru_tick_event,
  };
  void lru_init(void) {
      list_init(&lru_list); // 初始化 LRU 链表
  }
  int lru_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in) {
      list_entry_t *head = &lru_list;
      list_entry_t *entry = &page->page_link;
  
      if (list_find(head, entry) == NULL) {
          list_add(head, entry); // 将页加入 LRU 链表头部
      }
      return 0;
  }
  struct Page *lru_swap_out(struct mm_struct *mm, int *swap_in) {
      list_entry_t *tail = list_prev(&lru_list);
      if (tail != &lru_list) {
          struct Page *page = le2page(tail, page_link);
          list_del(tail); // 从 LRU 链表中移除该页
          return page;
      }
      return NULL;
  }
  ```
  
  - `init` 函数初始化 LRU 相关的资源。
  - `map_swappable` 函数用于将页标记为可交换，并将其加入 LRU 链表。
  - `set_unswappable` 函数将页标记为不可交换。
  - `swap_out` 函数执行实际的页替换操作，从 LRU 链表尾部选择最近最少使用的页。
  - `tick_event` 用于处理周期性事件，如老化页的计时器更新。
  - 在 `lru_init` 函数中，我们初始化链表，用于跟踪 LRU 页。
  - `lru_map_swappable`用于将某个页标记为可交换页，并将其添加到 LRU 链表头部。这样，最近访问的页总是位于链表头部。
  - `lru_swap_out`实现当需要换出一页时，我们从 LRU 链表的尾部找到最近最少使用的页
  
  #### 2.对其他文件进行修改
  
  对swap_lru.c以及swap_lru.h进行编写后，主要在进行了以下修改：
  
  1.在 `kern_init` 函数中调用 `swap_init` 函数来初始化换页系统（`swap_manager`），并确保 `swap_init` 正确地设置为使用 LRU 换页管理器。
  
  2.在 `swap.h` 中定义 `swap_manager` 结构体指针，并在适当位置引入 `lru_swap_manager` 以支持 LRU。
  
  3.在 `pmm.c` 中增加对 `swap_init_ok` 的引用，以确保在内存不足时启动换页功能。
  
  ![](C:\Users\lenovo\AppData\Roaming\marktext\images\2024-10-31-13-27-08-b73d6af60a552e96b770c3e346e51de.png)
  
  ![](C:\Users\lenovo\AppData\Roaming\marktext\images\2024-10-31-13-27-01-ddd6fe2324865936dca9b90bdfdb482.png)
  
  ![](C:\Users\lenovo\AppData\Roaming\marktext\images\2024-10-31-13-26-54-130ce6133b12079ee988794a2fb2a51.png)

#### 5. 优缺点分析

- **优点**：
  
  - 替换最近最少使用的页面，理论上最优，适用于大多数页面访问频繁的应用场景。
  - 能有效降低缺页率，尤其在页面访问模式具有时间局部性时。

- **缺点**：
  
  - 实现相对复杂，需要维护一个链表或类似的数据结构。
  - 需要对链表操作进行更新，增加了系统开销，尤其在访问频繁时影响性能。

#### 6. 总结

在实现 LRU 页面置换算法时，合理地维护页面链表，并及时将最近访问的页面移至链表头部，可以有效减少缺页中断的频率。然而，由于系统开销相对较高，LRU 更适合内存较大、访问局部性强的环境。 ​
