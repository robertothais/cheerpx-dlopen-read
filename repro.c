/*
 * Minimal shared object built non-PIC, so it carries text relocations
 * (DT_TEXTREL): the absolute reference to `global_var` in read_global() and
 * the call to puts() become relocations (R_386_32 / R_386_PLT32) inside the
 * .text segment. To apply them, ld.so makes the code pages writable and
 * patches them in place at load. Since the mapping is MAP_PRIVATE (copy-on-
 * write), those writes normally stay out of the file that read() sees.
 */

#include <stdio.h>

int global_var = 0x11223344;

int read_global(void) { return global_var; }

int repro_loaded(void) {
  puts("librepro: loaded");
  return read_global();
}
