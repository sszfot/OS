#include <pmm.h>
#include <list.h>
#include <string.h>
#include <assert.h>
#include <memlayout.h>
#include <stdio.h>  // 使用 cprintf

#define MAX_ORDER 10  // 定义伙伴系统支持的最大阶数 (2^10 = 1024 页)

// 伙伴系统的自由链表
struct free_area {
    list_entry_t free_list[MAX_ORDER + 1];  // 每个阶级都有一个空闲链表
    size_t nr_free[MAX_ORDER + 1];          // 每个阶级的空闲块数量
};

static struct free_area buddy_free_area;

// 初始化伙伴系统
static void buddy_init(void) {
    for (int i = 0; i <= MAX_ORDER; i++) {
        list_init(&buddy_free_area.free_list[i]);  // 初始化每个阶的自由链表
        buddy_free_area.nr_free[i] = 0;
    }
}

// 初始化内存块
static void buddy_init_memmap(struct Page* base, size_t n) {
    assert(n > 0);
    int order = MAX_ORDER;
    while ((1 << order) > n) {
        order--;  // 找到合适的阶数
    }

    base->order = order;
    list_add(&buddy_free_area.free_list[order], &base->page_link);
    buddy_free_area.nr_free[order]++;
}

// 打印伙伴系统的状态
static void print_buddy_status(void) {
    cprintf("Buddy system status:\n");
    for (int i = 0; i <= MAX_ORDER; i++) {
        cprintf("Order %d: %lu free blocks\n", i, buddy_free_area.nr_free[i]);
    }
}

// 分配页面
static struct Page* buddy_alloc_pages(size_t n) {
    assert(n > 0);
    cprintf("[INFO] Requesting to allocate %lu pages.\n", n);

    int order = 0;
    while ((1 << order) < n) {
        order++;  // 找到满足请求的最小阶数
    }
    
    cprintf("[INFO] Calculated order for %lu pages is %d.\n", n, order);

    for (int current_order = order; current_order <= MAX_ORDER; current_order++) {
        if (!list_empty(&buddy_free_area.free_list[current_order])) {
            // 找到合适的块，开始拆分
            list_entry_t* le = list_next(&buddy_free_area.free_list[current_order]);
            struct Page* page = le2page(le, page_link);
            list_del(le);  // 从当前阶级的自由链表中移除
            buddy_free_area.nr_free[current_order]--;
            cprintf("[INFO] Allocated block of order %d (size: %d pages) at address %p.\n",
                    current_order, (1 << current_order), page);

            // 如果阶数大于所需的阶数，进行拆分
            while (current_order > order) {
                current_order--;
                struct Page* buddy = page + (1 << current_order);
                buddy->order = current_order;
                list_add(&buddy_free_area.free_list[current_order], &buddy->page_link);
                buddy_free_area.nr_free[current_order]++;
                cprintf("[INFO] Split block: new block of order %d (size: %d pages) at address %p.\n",
                        current_order, (1 << current_order), buddy);
            }

            page->order = order;
            cprintf("[INFO] Final allocated block of order %d (size: %d pages) at address %p.\n",
                    order, (1 << order), page);
            return page;
        }
    }

    cprintf("[ERROR] No suitable block found for allocation.\n");
    return NULL;  // 没有合适的块
}

// 释放页面
static void buddy_free_pages(struct Page* base, size_t n) {
    assert(n > 0);
    cprintf("[INFO] Freeing %lu pages starting at address %p.\n", n, base);

    int order = 0;
    while ((1 << order) < n) {
        order++;
    }

    struct Page* page = base;
    while (order < MAX_ORDER) {
        struct Page* buddy = page + (1 << order);
        if (!list_empty(&buddy_free_area.free_list[order])) {
            // 如果找不到相邻的伙伴块，直接释放
            page->order = order;
            list_add(&buddy_free_area.free_list[order], &page->page_link);
            buddy_free_area.nr_free[order]++;
            cprintf("[INFO] Freed block of order %d (size: %d pages) at address %p.\n",
                    order, (1 << order), page);
            return;
        } else {
            // 如果找到相邻的伙伴块，合并
            list_del(&buddy->page_link);
            buddy_free_area.nr_free[order]--;
            if (page > buddy) {
                struct Page* temp = page;
                page = buddy;
                buddy = temp;
            }
            order++;
            cprintf("[INFO] Merged with buddy block: new order %d (size: %d pages) at address %p.\n",
                    order, (1 << order), page);
        }
    }

    page->order = order;
    list_add(&buddy_free_area.free_list[order], &page->page_link);
    buddy_free_area.nr_free[order]++;
    cprintf("[INFO] Freed merged block of order %d (size: %d pages) at address %p.\n",
            order, (1 << order), page);
}

// 获取空闲页数
static size_t buddy_nr_free_pages(void) {
    size_t total_free = 0;
    for (int i = 0; i <= MAX_ORDER; i++) {
        total_free += buddy_free_area.nr_free[i] * (1 << i);
    }
    return total_free;
}

// 测试伙伴系统
void buddy_system_test() {
    cprintf("Running buddy system test...\n");

    struct Page* p0, *p1, *p2, *p3;
    int all_pages;

    // 初始化伙伴系统
    buddy_init();
    cprintf("[INFO] Initialized buddy system.\n");

    // 初始化一个1024页的块
    buddy_init_memmap((struct Page*)0x1000, 1024);
    cprintf("[INFO] Initialized memory map for 1024 pages.\n");

    // 获取当前空闲页数
    all_pages = buddy_nr_free_pages();
    
    // 测试: 分配超过可用页数的请求，应该返回 NULL
    assert(buddy_alloc_pages(all_pages + 1) == NULL);
    cprintf("[INFO] Successfully handled allocation request for too many pages.\n");

    // 分配1页
    p0 = buddy_alloc_pages(1);
    assert(p0 != NULL);
    cprintf("[INFO] Allocated 1 page.\n");

    // 分配2页，确保它们紧跟在之前分配的1页后面
    p1 = buddy_alloc_pages(2);
    assert(p1 == p0 + 2);
    assert(!PageReserved(p0) && !PageProperty(p0));
    assert(!PageReserved(p1) && !PageProperty(p1));
    cprintf("[INFO] Allocated 2 pages adjacent to the first allocation.\n");

    // 再分配1页，确保它紧跟在之前的2页后
    p2 = buddy_alloc_pages(1);
    assert(p2 == p0 + 1);
    cprintf("[INFO] Allocated another 1 page adjacent to the first allocation.\n");

    // 分配8页
    p3 = buddy_alloc_pages(8);
    assert(p3 == p0 + 8);
    assert(!PageProperty(p3) && !PageProperty(p3 + 7) && PageProperty(p3 + 8));
    cprintf("[INFO] Allocated 8 pages at expected location.\n");

    // 释放2页，并确保属性被正确恢复
    buddy_free_pages(p1, 2);
    assert(PageProperty(p1) && PageProperty(p1 + 1));
    assert(p1->ref == 0);
    cprintf("[INFO] Freed 2 pages and verified properties.\n");

    // 释放1页
    buddy_free_pages(p0, 1);
    cprintf("[INFO] Freed 1 page.\n");

    // 释放另外的1页
    buddy_free_pages(p2, 1);
    cprintf("[INFO] Freed another 1 page.\n");

    // 释放后，再次分配3页，检查是否能正确重新分配
    p2 = buddy_alloc_pages(3);
    assert(p2 == p0);
    cprintf("[INFO] Reallocated 3 pages successfully.\n");

    // 再次释放这3页
    buddy_free_pages(p2, 3);
    assert((p2 + 2)->ref == 0);
    cprintf("[INFO] Freed reallocated 3 pages.\n");

    // 验证释放后的空闲页数是否合理
    assert(buddy_nr_free_pages() == all_pages >> 1);
    cprintf("[INFO] Verified number of free pages after freeing.\n");

    // 测试大块分配：分配129页，检查它的分配位置是否正确
    p1 = buddy_alloc_pages(129);
    assert(p1 == p0 + 256);
    cprintf("[INFO] Allocated 129 pages at expected location.\n");

    // 释放129页
    buddy_free_pages(p1, 256);
    cprintf("[INFO] Freed 129 pages.\n");

    // 释放之前分配的8页
    buddy_free_pages(p3, 8);
    cprintf("[INFO] Freed 8 pages.\n");

    cprintf("[INFO] Buddy system test completed successfully.\n");
}
// 定义 buddy_pmm_manager
const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_system_test,  // 测试函数
};

