#ifndef __KERN_MM_BUDDY_PMM_H__
#define __KERN_MM_BUDDY_PMM_H__

#include <pmm.h>
#include <list.h>

// 定义伙伴系统支持的最大阶数 (假设支持的最大阶数是 10，表示最大块大小为 2^10 页 = 1024 页)
#define MAX_ORDER 10
extern const struct pmm_manager buddy_pmm_manager;


// 伙伴系统的空闲链表管理结构体
struct free_area {
    list_entry_t free_list[MAX_ORDER + 1];  // 每个阶级都有一个空闲链表
    size_t nr_free[MAX_ORDER + 1];          // 每个阶级的空闲块数量
};

// 伙伴系统的全局空闲内存区管理结构体
extern struct free_area buddy_free_area;

// 函数声明

/**
 * @brief 初始化伙伴系统，设置每个阶级的链表为空
 */
void buddy_init(void);

/**
 * @brief 初始化内存映射，将内存区域初始化为指定的阶级并放入空闲链表
 * @param base 起始页框
 * @param n 页框数量
 */
void buddy_init_memmap(struct Page *base, size_t n);

/**
 * @brief 分配 n 个页面的内存块
 * @param n 页数
 * @return 返回分配的页框结构体指针，如果分配失败返回 NULL
 */
struct Page *buddy_alloc_pages(size_t n);

/**
 * @brief 释放 n 个页面的内存块，并尝试合并其伙伴块
 * @param base 起始页框
 * @param n 页框数量
 */
void buddy_free_pages(struct Page *base, size_t n);

/**
 * @brief 获取当前所有阶级的空闲页面总数
 * @return 返回空闲页面的总数
 */
size_t buddy_nr_free_pages(void);

/**
 * @brief 检查伙伴系统是否正确运行 (可选的检查函数，通常用于测试)
 */
void buddy_check(void);

#endif /* !__KERN_MM_BUDDY_PMM_H__ */

