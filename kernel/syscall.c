/*
 * contains the implementation of all syscalls.
 */

#include <stdint.h>
#include <errno.h>

#include "util/types.h"
#include "syscall.h"
#include "string.h"
#include "process.h"
#include "util/functions.h"
#include "pmm.h"
#include "vmm.h"
#include "spike_interface/spike_utils.h"
#include "memlayout.h"

extern uint64 first_malloc;
extern struct MCB_s *MCB_head;
extern uint64 cur_map;

//
// implement the SYS_user_print syscall
//
ssize_t sys_user_print(const char* buf, size_t n) {
  // buf is now an address in user space of the given app's user stack,
  // so we have to transfer it into phisical address (kernel is running in direct mapping).
  assert( current );
  char* pa = (char*)user_va_to_pa((pagetable_t)(current->pagetable), (void*)buf);
  sprint(pa);
  return 0;
}

//
// implement the SYS_user_exit syscall
//
ssize_t sys_user_exit(uint64 code) {
  sprint("User exit with code:%d.\n", code);
  // in lab1, PKE considers only one app (one process). 
  // therefore, shutdown the system when the app calls exit()
  shutdown(code);
}

//
// maybe, the simplest implementation of malloc in the world ... added @lab2_2
//
uint64 sys_user_allocate_page(uint64 n) {
  MCB_init();
  //sprint("%d\n", sizeof(MCB_head->next));
  MCB *cur = (MCB*)MCB_head;
  while(1) {
    if(cur->m_size >= n && cur->m_state == 0) {
      cur->m_state = 1;
      return cur->m_startaddr;
    }
    if(cur->next == NULL) break;
    cur = cur->next;
  }
  uint64 heap_top = cur_map;
  malloc_map(sizeof(MCB) + n);
  pte_t *pte = page_walk(current->pagetable, heap_top, 0);
  MCB *cur_1 = (MCB*)(PTE2PA(*pte) + (heap_top & 0xfff));
  uint64 align = (uint64)cur_1 % 8;
  cur_1 = (MCB*)((uint64)cur_1 + 8 - align);

  cur_1->m_state = 1;
  cur_1->m_startaddr = heap_top + sizeof(MCB);
  cur_1->m_size = n;
  cur_1->next = cur->next;

  cur->next = cur_1;
  return cur_1->m_startaddr;
}

//
// reclaim a page, indicated by "va". added @lab2_2
//
uint64 sys_user_free_page(uint64 va) {
  //user_better_free((void*)va);
  void *real_addr = (void*)((uint64)va - sizeof(MCB));
  pte_t *pte = page_walk(current->pagetable, (uint64)(real_addr), 0);
  MCB *cur = (MCB*)(PTE2PA(*pte) + ((uint64)real_addr & 0xfff));
  uint64 align = (uint64)cur % 8;
  cur = (MCB*)((uint64)cur + 8 - align);
  cur->m_state = 0;
  return 0;
}


//
// [a0]: the syscall number; [a1] ... [a7]: arguments to the syscalls.
// returns the code of success, (e.g., 0 means success, fail for otherwise)
//
long do_syscall(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7) {
  switch (a0) {
    case SYS_user_print:
      return sys_user_print((const char*)a1, a2);
    case SYS_user_exit:
      return sys_user_exit(a1);
    // added @lab2_2
    case SYS_user_allocate_page:
      return sys_user_allocate_page(a1);
    case SYS_user_free_page:
      return sys_user_free_page(a1);
    default:
      panic("Unknown syscall %ld \n", a0);
  }
}
