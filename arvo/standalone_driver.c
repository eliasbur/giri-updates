/* Standalone replacement for libFuzzer's main(): read one input file, hand it
 * to LLVMFuzzerTestOneInput exactly once, return.  Giri needs a single
 * deterministic execution, not a fuzzing loop, and it needs main() to be part
 * of the traced bitcode so the runtime's ctor/atexit pair brackets the run. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size);
__attribute__((weak)) int LLVMFuzzerInitialize(int *argc, char ***argv);

int main(int argc, char **argv) {
  FILE *f;
  long n;
  unsigned char *buf;

  if (argc < 2) {
    fprintf(stderr, "usage: %s <input-file>\n", argv[0]);
    return 1;
  }
  if (LLVMFuzzerInitialize)
    LLVMFuzzerInitialize(&argc, &argv);

  f = fopen(argv[1], "rb");
  if (!f) { perror(argv[1]); return 1; }
  fseek(f, 0, SEEK_END);
  n = ftell(f);
  fseek(f, 0, SEEK_SET);
  buf = (unsigned char *)malloc(n ? n : 1);
  if (fread(buf, 1, n, f) != (size_t)n) { fclose(f); return 1; }
  fclose(f);

  LLVMFuzzerTestOneInput(buf, (size_t)n);
  free(buf);
  return 0;
}
