#include <sys/signal.h>

int pthread_sigmask(int how, const sigset_t *restrict set,
                    sigset_t *restrict oset) {
  (void)how;
  (void)set;
  (void)oset;
  return 0;
}
