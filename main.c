/*
 * read(2) coherence across dlopen.
 *
 * On Linux, read() of a file returns its on-disk bytes whether or not the
 * file is currently mapped: dlopen maps libraries with MAP_PRIVATE, so the
 * loader's relocation writes are copy-on-write and don't reach the file. This
 * reads librepro.so before and after dlopen and diffs the two; under CheerpX
 * the second read comes back as the relocated in-memory image.
 *
 * It also forks a child that never dlopens the library and has it read the
 * file. On Linux the child is unaffected by the parent's mapping; under
 * CheerpX it reads the parent's relocations too, so the effect is not confined
 * to the process that loaded the library.
 */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define LIB "/usr/local/lib/librepro.so"

// Read the whole file via raw open()/read() so the result is the file's
// on-disk content, independent of stdio buffering.
static long slurp(const char *path, unsigned char **out) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    perror("open");
    return -1;
  }
  struct stat st;
  if (fstat(fd, &st) < 0) {
    perror("fstat");
    close(fd);
    return -1;
  }
  long n = (long)st.st_size;
  unsigned char *buf = malloc(n);
  long off = 0;
  while (off < n) {
    ssize_t r = read(fd, buf + off, (size_t)(n - off));
    if (r <= 0) {
      perror("read");
      close(fd);
      free(buf);
      return -1;
    }
    off += r;
  }
  close(fd);
  *out = buf;
  return n;
}

static long diff_count(const unsigned char *a, const unsigned char *b, long n,
                       long *first, long *last) {
  long nd = 0;
  *first = -1;
  *last = -1;
  for (long i = 0; i < n; i++)
    if (a[i] != b[i]) {
      if (*first < 0)
        *first = i;
      *last = i;
      nd++;
    }
  return nd;
}

int main(void) {
  setvbuf(stdout, NULL, _IONBF, 0);

  unsigned char *ref = NULL;
  long n = slurp(LIB, &ref); // on-disk baseline, before any dlopen
  if (n < 0)
    return 2;
  printf("librepro.so: %ld bytes\n\n", n);

  // Fork a reader that never dlopens, for the cross-process check below.
  int pfd[2];
  if (pipe(pfd) < 0) {
    perror("pipe");
    return 2;
  }
  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    return 2;
  }

  if (pid == 0) {
    close(pfd[1]);
    char c;
    (void)read(pfd[0], &c, 1); // wait until the parent has dlopened
    unsigned char *cb = NULL;
    long m = slurp(LIB, &cb);
    if (m < 0)
      _exit(2);
    long cf, cl;
    long cd = diff_count(cb, ref, m < n ? m : n, &cf, &cl);
    if (cd == 0)
      printf("[OK]   cross-process  read by a process that never dlopen'd "
             "matches on-disk\n");
    else
      printf("[BUG]  cross-process  read by a process that never dlopen'd "
             "differs at %ld bytes (from 0x%lx)\n",
             cd, cf);
    _exit(0);
  }

  // Parent: dlopen (RTLD_NOW applies the relocations), then read again.
  void *h = dlopen(LIB, RTLD_NOW | RTLD_GLOBAL);
  if (!h)
    fprintf(stderr, "dlopen: %s\n", dlerror());

  unsigned char *after = NULL;
  long n2 = slurp(LIB, &after);
  if (n2 < 0)
    return 2;

  long first, last, k = n < n2 ? n : n2;
  long ndiff = diff_count(after, ref, k, &first, &last);
  int reproduced = (n == n2) ? (ndiff != 0) : 1;

  if (!reproduced) {
    printf("[OK]   same-process   read after dlopen matches on-disk\n");
  } else {
    printf("[BUG]  same-process   read after dlopen differs at %ld/%ld bytes "
           "[0x%lx..0x%lx]\n",
           ndiff, k, first, last);
    // Format each hex line into one string and emit with a single printf:
    // CheerpX's custom console renders every write() as its own line.
    long w = first & ~0xfL;
    char hx[64];
    int p = 0;
    for (int i = 0; i < 16 && w + i < k; i++)
      p += snprintf(hx + p, sizeof hx - p, "%02x ", ref[w + i]);
    printf("       0x%04lx  on-disk   %s\n", w, hx);
    p = 0;
    for (int i = 0; i < 16 && w + i < k; i++)
      p += snprintf(hx + p, sizeof hx - p, "%02x ", after[w + i]);
    printf("       0x%04lx  readback  %s\n", w, hx);
  }

  // Release the never-dlopen child so it reports the cross-process result.
  (void)write(pfd[1], "x", 1);
  close(pfd[1]);
  int st = 0;
  waitpid(pid, &st, 0);

  printf("\n%s\n", reproduced ? "REPRODUCED" : "OK");
  return reproduced ? 1 : 0;
}
