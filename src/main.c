#include "file-parser/file-parser.h"
#include <stdio.h>

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("usage: %s [file name]\n", argv[0]);
    return 0;
  }

  metainfo_t file = parse_file(argv[1]);
  return 0;
}
