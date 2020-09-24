#include <cstdio>

void break_here() {}

int main() {
  int x = 42;
  printf("Hello %d\n", x);

  break_here();

  return 0;
}
