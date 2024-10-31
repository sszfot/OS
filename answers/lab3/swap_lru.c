#include "swap_lru.h"
#include <list.h>
#include <pmm.h>
#include <vmm.h>

static list_entry_t lru_list; // LRU链表，用于管理页的访问顺序

struct swap_manager lru_swap_manager = {
    .init = lru_init,
    .map_swappable = lru_map_swappable,
    .set_unswappable = lru_set_unswappable,
    .swap_out = lru_swap_out,
};

struct swap_manager *swap_manager = &lru_swap_manager;
// 初始化LRU链表
void lru_init(void) {
    list_init(&lru_list);
}

list_entry_t *list_find(list_entry_t *head, list_entry_t *entry);
list_entry_t *list_find(list_entry_t *head, list_entry_t *entry) {
    list_entry_t *le = head;
    while ((le = list_next(le)) != head) {
        if (le == entry) {
            return le; // 找到条目
        }
    }
    return NULL; // 未找到条目
}

// 将页面标记为“可交换的”并插入到LRU链表头部
int lru_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in) {
    list_entry_t *head = &lru_list;
    list_entry_t *entry = &page->pra_page_link;
    if (list_find(head, entry) == NULL) {
        list_add(head, entry);
    }
    return 0;
}

// 设置页面为“不可交换的”，并从LRU链表中移除
int lru_set_unswappable(struct mm_struct *mm, uintptr_t addr) {
    struct Page *page = get_page(mm->pgdir, addr, NULL);
    if (page != NULL) {
        list_del(&page->pra_page_link);
    }
    return 0;
}

// 页替换逻辑，选择最久未使用的页（链表尾部），并执行换出操作
struct Page *lru_swap_out(struct mm_struct *mm, int *swap_in) {
    list_entry_t *le = lru_list.prev; // 链表尾部是最近最少使用的页面
    if (le == &lru_list) {
        return NULL;
    }
    struct Page *page = le2page(le, pra_page_link);
    list_del(le);
    return page;
}
// 在从 'head' 开始的链表中查找 'entry'。
// 如果找到匹配的条目，则返回指向该条目的指针；如果未找到，则返回 NULL。












