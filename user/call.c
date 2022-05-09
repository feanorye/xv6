#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int g(int x, int y, int c, int d) {
  return x+3 + y + c + d;
}

int f(int x) {
  x = x + 1;
  return g(x, 2, 1, 4);
}

void main(void) {
  unsigned int i = 0x00646c72;
	printf("H%x Wo%s", 57616, &i);
  printf("x=%d y=%d\n", 3);
  printf("%d %d\n", f(8)+1, 13);
  exit(0);
}
