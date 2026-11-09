// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * ntfsotomatik-bildirim — kullanıcı tarafı bildirim yardımcısı (LaunchAgent)
 *
 * Yalnızca okur: root servisinin yazdığı /Library/NTFSOtomatik/durum/bildirim dosyasını izler
 * (kqueue/dispatch ile, yoklama yok) ve yeni satırları macOS bildirimi olarak gösterir.
 * Root servisine hiçbir şey göndermez; iletişim tek yönlüdür (root -> kullanıcı).
 * Metin osascript'e kod olarak değil, argüman (argv) olarak verilir.
 */
#include <dispatch/dispatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define NOTIFY_FILE "/Library/NTFSOtomatik/durum/bildirim"

extern char **environ;
static off_t g_off = 0;
static dispatch_source_t g_src = NULL;

static void show(const char *msg) {
    char text[512];
    snprintf(text, sizeof text, "%s", msg);
    for (char *p = text; *p; p++) if ((unsigned char)*p < 0x20) *p = ' ';
    char *argv[] = { "/usr/bin/osascript",
        "-e", "on run argv",
        "-e", "display notification (item 1 of argv) with title \"NTFSOtomatik\"",
        "-e", "end run",
        text, NULL };
    pid_t pid;
    if (posix_spawn(&pid, argv[0], NULL, NULL, argv, environ) == 0) {
        int st; waitpid(pid, &st, 0);
    }
}

static void read_new(void) {
    int fd = open(NOTIFY_FILE, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return;
    struct stat st;
    if (fstat(fd, &st) != 0) { close(fd); return; }
    if (st.st_uid != 0 || (st.st_mode & 022)) { close(fd); return; } /* yalnızca root'un yazabildiği dosyaya güven */
    if (st.st_size < g_off) g_off = 0;          /* dosya kısaltıldı */
    if (st.st_size == g_off) { close(fd); return; }
    if (lseek(fd, g_off, SEEK_SET) < 0) { close(fd); return; }
    size_t want = (size_t)(st.st_size - g_off);
    if (want > 65536) { g_off = st.st_size - 65536; lseek(fd, g_off, SEEK_SET); want = 65536; }
    char *buf = malloc(want + 1);
    if (!buf) { close(fd); return; }
    ssize_t n = read(fd, buf, want);
    close(fd);
    if (n <= 0) { free(buf); return; }
    buf[n] = 0;
    char *line = buf, *nl;
    while ((nl = strchr(line, '\n'))) {
        *nl = 0;
        char *tab = strchr(line, '\t');
        if (tab && tab[1]) show(tab + 1);
        g_off += (nl - line) + 1;
        line = nl + 1;
    }
    free(buf);
}

static void watch(void);

static void rewatch_later(void) {
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{ watch(); });
}

static void watch(void) {
    int fd = open(NOTIFY_FILE, O_EVTONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) { rewatch_later(); return; } /* dosya henüz yok: servis oluşturunca izlenir */
    g_src = dispatch_source_create(DISPATCH_SOURCE_TYPE_VNODE, (uintptr_t)fd,
              DISPATCH_VNODE_WRITE | DISPATCH_VNODE_EXTEND | DISPATCH_VNODE_DELETE | DISPATCH_VNODE_RENAME,
              dispatch_get_main_queue());
    dispatch_source_set_event_handler(g_src, ^{
        unsigned long f = dispatch_source_get_data(g_src);
        read_new();
        if (f & (DISPATCH_VNODE_DELETE | DISPATCH_VNODE_RENAME)) {
            dispatch_source_cancel(g_src); g_src = NULL; g_off = 0; rewatch_later();
        }
    });
    dispatch_source_set_cancel_handler(g_src, ^{ close(fd); });
    dispatch_resume(g_src);
    read_new();
}

int main(int argc, char **argv) {
    (void)argv;
    if (argc != 1 || getuid() == 0) return 2; /* kullanıcı olarak, argümansız */
    struct stat st;
    if (stat(NOTIFY_FILE, &st) == 0) g_off = st.st_size; /* eski bildirimleri yeniden gösterme */
    watch();
    dispatch_main();
}
