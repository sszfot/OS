#ifndef __KERN_MM_SWAP_LRU_H__
#define __KERN_MM_SWAP_LRU_H__
struct mm_struct; // 前置声明
#include <list.h>
#include <mmu.h>
#include <memlayout.h>
#include <swap.h>

extern struct swap_manager lru_swap_manager;
extern struct swap_manager *swap_manager;
void lru_init(void);
int lru_map_swappable(struct mm_struct* mm, uintptr_t addr, struct Page* page, int swap_in);
int lru_set_unswappable(struct mm_struct* mm, uintptr_t addr);
struct Page* lru_swap_out(struct mm_struct* mm, int* swap_in);

#endif /* !__KERN_MM_SWAP_LRU_H__ */
