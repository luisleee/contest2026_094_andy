/****************************************************************************
 * contest2026_094_andy/chip/d13x/d13x_boot.c
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/types.h>

void up_allocate_heap(void **heap_start, size_t *heap_size)
{
  extern uintptr_t _sheap;
  extern uintptr_t _eheap;

  *heap_start = (void *)&_sheap;
  *heap_size = (size_t)((uintptr_t)&_eheap - (uintptr_t)&_sheap);
}
