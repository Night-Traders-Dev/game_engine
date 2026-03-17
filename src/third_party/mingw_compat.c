// Compatibility shims for functions missing from mingw
// Only compiled when cross-compiling for Windows

#ifdef _WIN32
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <fcntl.h>
#include <process.h>

// mkstemps: create a temp file with a suffix
// mingw doesn't have mkstemps, provide a pid-based fallback
int mkstemps(char *tmpl, int suffixlen) {
    // Find the XXXXXX pattern
    size_t len = strlen(tmpl);
    if (len < 6 + (size_t)suffixlen) return -1;
    char *xs = tmpl + len - suffixlen - 6;
    // Replace XXXXXX with pid + counter based unique name
    static int counter = 0;
    snprintf(xs, 7, "%04x%02x", _getpid() & 0xFFFF, (counter++) & 0xFF);
    // Restore the suffix (snprintf may have written a null into it)
    // Actually the suffix is after the 6 chars, snprintf only wrote 6+null
    // but the null overwrote the first suffix char. Fix:
    // Just use the whole template approach differently:
    // Write unique chars without null-terminating into the XXXXXX region
    int pid = _getpid();
    for (int i = 0; i < 6; i++) {
        int v = ((pid >> (i * 3)) ^ (counter + i * 7)) & 0x3F;
        xs[i] = 'A' + (v % 26);
    }
    counter++;
    int fd = _open(tmpl, _O_CREAT | _O_EXCL | _O_RDWR, 0600);
    return fd;
}
#endif
