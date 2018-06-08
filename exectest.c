#include "types.h"
#include "user.h"

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf(1, "usage: exectest <filename>\n");
  } else {
    exec(argv[1], argv+1);
  }
  exit();
}
