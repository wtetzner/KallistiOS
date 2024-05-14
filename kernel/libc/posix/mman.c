/* #include <stdio.h> */
#include <sys/mman.h>

void *	mmap(void * addr, size_t length, int prot, int flags, int fd, off_t offset) {
  /* printf("mmap(-, %d)\n", length); */
  return malloc(length);
}

int	munmap(void * addr, size_t length) {
  return free(addr);
}
