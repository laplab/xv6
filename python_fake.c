#include "types.h"
#include "user.h"

int main(int argc, char **argv) {
  for (int i = 0; i < argc; i++) {
    printf(1, argv[i]);
    printf(1, "\n");
  }
  exit();
}
