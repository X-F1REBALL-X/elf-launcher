#include <dirent.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <ps5/kernel.h>
#include <stdarg.h>

int sceSystemServiceLaunchWebBrowser(const char *uri, void *);
#define IOVEC_SIZE(x) (sizeof(x) / sizeof(struct iovec))
#define IOVEC_ENTRY(x) {x ? x : 0, x ? strlen(x) + 1 : 0}
#define TITLE_ID "ELFL00001"
#ifndef PORT
#define PORT 1000
#endif
#define HOME_ICON_VERSION "1.0.22"
#define HOME_ICON_VER_PATH "/data/elf-launcher/home-icon.ver"

#define INCASSET(name, file)                                                   \
  __asm__(".section .rodata\n"                                                 \
          ".global " #name "\n"                                                \
          ".global " #name "_end\n"                                            \
          ".global " #name "_size\n"                                           \
          ".align 16\n" #name ":\n"                                            \
          ".incbin \"" file "\"\n" #name "_end:\n" #name "_size:\n"            \
          ".quad " #name "_end - " #name "\n"                                  \
          ".previous\n");                                                      \
  extern const uint8_t name[];                                                 \
  extern const size_t name##_size;

INCASSET(param_json, "sce_sys/param.json");
INCASSET(icon0_png, "sce_sys/icon0.png");
INCASSET(pic1_png, "sce_sys/pic1.png");
INCASSET(index_html, "webapp/index.html");
INCASSET(ico_launcher_home, "icons/launcher-home.jpg");

static int send_blob(int c, const char *ctype, const void *body, size_t n);

static int send_icon(int c, const char *path) {
  const char *name = path;
  if (!strncmp(name, "icons/", 6))
    name += 6;
  if (!strcmp(name, "launcher-home.jpg"))
    return send_blob(c, "image/jpeg", ico_launcher_home, ico_launcher_home_size);
  return -1;
}

static int __attribute__((unused)) remount_system_ex(void) {
  struct iovec iov[] = {
      IOVEC_ENTRY("from"),      IOVEC_ENTRY("/dev/ssd0.system_ex"),
      IOVEC_ENTRY("fspath"),    IOVEC_ENTRY("/system_ex"),
      IOVEC_ENTRY("fstype"),    IOVEC_ENTRY("exfatfs"),
      IOVEC_ENTRY("large"),     IOVEC_ENTRY("yes"),
      IOVEC_ENTRY("timezone"),  IOVEC_ENTRY("static"),
      IOVEC_ENTRY("async"),     IOVEC_ENTRY(NULL),
      IOVEC_ENTRY("ignoreacl"), IOVEC_ENTRY(NULL),
  };
  return nmount(iov, IOVEC_SIZE(iov), MNT_UPDATE);
}

static int install_file(const char *path, const uint8_t *data, size_t size) {
  FILE *f = fopen(path, "w");
  if (!f)
    return -1;
  if (data && size && fwrite(data, size, 1, f) != 1) {
    fclose(f);
    return -1;
  }
  fclose(f);
  return 0;
}


/* Install home icon for ELFL00001. Skip when title + HOME_ICON_VERSION match.
 * Do NOT NEEDED-link libSceAppInstUtil (elfldr:9021 cannot load that).
 * Elevate, LoadStartModule, then InstallTitleDir via NID (like host/installer). */
#define NID_LoadStart "wzvqT4UqKX8"
#define NID_AppInstInit "540lotO7oHE"
#define NID_AppInstTerm "kLLazhNh6d4"
#define NID_InstallTitleDir "Wudg3Xe3heE"
#define NID_AppUnInstall "U0R69uEHte4"

static void __attribute__((unused)) elevate_for_appinst(void) {
  uint8_t privcaps[16] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                          0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  kernel_set_ucred_authid(-1, 0x4801000000000013L);
  kernel_set_ucred_caps(-1, privcaps);
}


static int __attribute__((unused)) kernel_handle(uint32_t *out) {
  if (!kernel_dynlib_handle(-1, "libkernel_web.sprx", out) ||
      !kernel_dynlib_handle(-1, "libkernel.sprx", out) ||
      !kernel_dynlib_handle(-1, "libkernel_sys.sprx", out))
    return 0;
  return -1;
}

static int __attribute__((unused)) load_appinst_util(uint32_t *ah_out) {
  uint32_t kh = 0, ah = 0;
  int (*LoadStart)(const char *, size_t, const void *, uint32_t, void *,
                   int *) = 0;
  int res = 0, rv, i;
  static const char *mod_names[] = {
      "libSceAppInstUtil.sprx",
      "/system/common/lib/libSceAppInstUtil.sprx",
      "/system/priv/lib/libSceAppInstUtil.sprx",
      "/system_ex/common_ex/lib/libSceAppInstUtil.sprx",
      "/system_ex/priv_ex/lib/libSceAppInstUtil.sprx",
      0};

  if (kernel_handle(&kh))
    return -1;
  LoadStart = (void *)kernel_dynlib_resolve(-1, kh, NID_LoadStart);
  if (!LoadStart)
    return -1;
  if (kernel_dynlib_handle(-1, "libSceAppInstUtil.sprx", &ah)) {
    for (i = 0; mod_names[i]; i++) {
      res = -1;
      rv = LoadStart(mod_names[i], 0, 0, 0, 0, &res);
      (void)rv;
      if (!kernel_dynlib_handle(-1, "libSceAppInstUtil.sprx", &ah))
        break;
    }
  }
  if (kernel_dynlib_handle(-1, "libSceAppInstUtil.sprx", &ah))
    return -1;
  *ah_out = ah;
  return 0;
}


static int home_icon_up_to_date(void) {
  FILE *f;
  char buf[64];
  size_t n;
  struct stat st;
  if (stat("/user/app/" TITLE_ID "/sce_sys/param.json", &st))
    return 0;
  if (stat("/user/app/" TITLE_ID "/sce_sys/icon0.png", &st))
    return 0;
  f = fopen(HOME_ICON_VER_PATH, "r");
  if (!f)
    return 0;
  if (!fgets(buf, sizeof(buf), f)) {
    fclose(f);
    return 0;
  }
  fclose(f);
  n = strlen(buf);
  while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
    buf[--n] = 0;
  return !strcmp(buf, HOME_ICON_VERSION);
}

static void write_home_icon_ver(void) {
  FILE *f;
  mkdir("/data", 0755);
  if (mkdir("/data/elf-launcher", 0755) && errno != EEXIST)
    return;
  f = fopen(HOME_ICON_VER_PATH, "w");
  if (!f)
    return;
  fprintf(f, "%s\n", HOME_ICON_VERSION);
  fclose(f);
}

static void clear_home_icon_ver(void) { unlink(HOME_ICON_VER_PATH); }

static int install_home_icon(void) {
  int err = -1;
  uint32_t ah = 0;
  int (*Init)(void) = 0;
  int (*Term)(void) = 0;
  int (*InstallDir)(const char *, const char *, void *) = 0;
  int (*UnInstall)(const char *) = 0;
  struct stat st;

  elevate_for_appinst();

  if (mkdir("/user/app/" TITLE_ID, 0755) && errno != EEXIST)
    return -1;
  if (mkdir("/user/app/" TITLE_ID "/sce_sys", 0755) && errno != EEXIST)
    return -1;
  if (install_file("/user/app/" TITLE_ID "/sce_sys/icon0.png", icon0_png,
                   icon0_png_size) ||
      install_file("/user/app/" TITLE_ID "/sce_sys/param.json", param_json,
                   param_json_size) ||
      install_file("/user/app/" TITLE_ID "/sce_sys/pic1.png", pic1_png,
                   pic1_png_size))
    return -1;

  if (load_appinst_util(&ah))
    return -2;
  Init = (void *)kernel_dynlib_resolve(-1, ah, NID_AppInstInit);
  Term = (void *)kernel_dynlib_resolve(-1, ah, NID_AppInstTerm);
  InstallDir = (void *)kernel_dynlib_resolve(-1, ah, NID_InstallTitleDir);
  UnInstall = (void *)kernel_dynlib_resolve(-1, ah, NID_AppUnInstall);
  if (!Init || !InstallDir)
    return -3;
  err = Init();
  if (err)
    return err;
  if (UnInstall) {
    UnInstall(TITLE_ID);
    sleep(2);
  }
  /* rewrite assets after uninstall wipe */
  if (mkdir("/user/app/" TITLE_ID, 0755) && errno != EEXIST)
    goto out;
  if (mkdir("/user/app/" TITLE_ID "/sce_sys", 0755) && errno != EEXIST)
    goto out;
  if (install_file("/user/app/" TITLE_ID "/sce_sys/icon0.png", icon0_png,
                   icon0_png_size) ||
      install_file("/user/app/" TITLE_ID "/sce_sys/param.json", param_json,
                   param_json_size) ||
      install_file("/user/app/" TITLE_ID "/sce_sys/pic1.png", pic1_png,
                   pic1_png_size))
    goto out;
  err = InstallDir(TITLE_ID, "/user/app/", 0);
  if (!err && stat("/user/app/" TITLE_ID "/sce_sys/param.json", &st))
    err = -4;
  if (!err)
    write_home_icon_ver();
out:
  if (Term)
    Term();
  return err;
}

/* The loader starts only when the page is opened, not during install. */
int start_real_elfldr(void);
void notify(const char *fmt, ...);
static void evlog(const char *kind, const char *fmt, ...);
static int start_elfldr_guarded(void);
static int loader_started;

static int connect_port(int port) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in addr;
  if (fd < 0)
    return -1;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons((uint16_t)port);
  addr.sin_addr.s_addr = htonl(0x7f000001);
  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(fd);
    return -1;
  }
  return fd;
}

static void on_page_open(void) {
  int fd;
  if (loader_started)
    return;
  loader_started = 1;
  fd = connect_port(9021);
  if (fd >= 0) {
    close(fd);
    notify("loader ready");
    return;
  }
  if (start_elfldr_guarded() == 0)
    notify("loader started");
  else
    notify("loader missing");
}

static int send_all(int c, const void *buf, size_t n) {
  const char *p = buf;
  while (n) {
    ssize_t w = send(c, p, n, 0);
    if (w <= 0)
      return -1;
    p += w;
    n -= (size_t)w;
  }
  return 0;
}


#define MiB(x) ((x) / (1024.0 * 1024))

typedef struct app_info {
  uint32_t app_id;
  uint64_t unknown1;
  uint32_t app_type;
  char title_id[10];
  char unknown2[0x3c];
} app_info_t;

int sceKernelGetAppInfo(pid_t pid, app_info_t *info);

static int is_user_daemon(const char *name, uint32_t app_id) {
  const char *ext;
  if (!name)
    return 0;
  if (!strcmp(name, "mini-syscore.elf"))
    return 0;
  if (app_id != 0)
    return 0;
  ext = strrchr(name, '.');
  if (ext && !strcasecmp(ext, ".elf"))
    return 1;
  return 0;
}

static int json_escape_name(const char *in, char *out, size_t outsz) {
  size_t o = 0;
  if (!outsz)
    return -1;
  while (*in && o + 2 < outsz) {
    unsigned char c = (unsigned char)*in++;
    if (c == '"' || c == '\\') {
      if (o + 3 >= outsz)
        break;
      out[o++] = '\\';
      out[o++] = (char)c;
    } else if (c < 0x20) {
      continue;
    } else {
      out[o++] = (char)c;
    }
  }
  out[o] = 0;
  return 0;
}

static int is_protected_proc_name(const char *name) {
  char base[64];
  const char *s;
  size_t n;
  int i;
  if (!name || !*name)
    return 0;
  s = strrchr(name, '/');
  s = s ? s + 1 : name;
  n = strlen(s);
  if (n >= sizeof(base))
    n = sizeof(base) - 1;
  for (i = 0; i < (int)n; i++) {
    char c = s[i];
    if (c >= 'A' && c <= 'Z')
      c = (char)(c - 'A' + 'a');
    base[i] = c;
  }
  base[n] = 0;
  if (!strcmp(base, "elfldr-ps5.elf") || !strcmp(base, "elfldr.elf") ||
      !strcmp(base, "payload-manager.elf") || !strcmp(base, "pldmgr.elf"))
    return 1;
  if (!strncmp(base, "elf-launcher", 12))
    return 1;
  if (!strncmp(base, "elfldr", 6))
    return 1;
  return 0;
}

/* PLDMGR-compatible JSON: {"processes":[{"pid","name","memory","is_daemon"}, ...]} */
static int is_protected_proc_name(const char *name);
static size_t process_list_json(char *buf, size_t max_size) {
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PROC, 0};
  size_t buf_size = 0;
  void *sysctl_buf = NULL;
  size_t pos = 0;
  int count = 0;

  if (max_size < 32)
    return 0;
  pos += (size_t)snprintf(buf + pos, max_size - pos, "{\"processes\":[\n");

  if (sysctl(mib, 4, NULL, &buf_size, NULL, 0) == 0 && buf_size) {
    sysctl_buf = malloc(buf_size);
    if (sysctl_buf && sysctl(mib, 4, sysctl_buf, &buf_size, NULL, 0) == 0) {
      void *ptr;
      for (ptr = sysctl_buf; ptr < (void *)((char *)sysctl_buf + buf_size);) {
        struct kinfo_proc *ki = (struct kinfo_proc *)ptr;
        app_info_t appinfo;
        char name_e[128];
        char comm[COMMLEN + 1];
        const char *nm;
        int is_daemon;
        double mem_mib;
        int n;
        if (ki->ki_structsize <= 0)
          break;
        ptr = (char *)ptr + ki->ki_structsize;

        memset(&appinfo, 0, sizeof(appinfo));
        if (sceKernelGetAppInfo(ki->ki_pid, &appinfo))
          memset(&appinfo, 0, sizeof(appinfo));

        /* COMMLEN is 19; force a terminator. Empty names must not be emitted
           (the UI used to treat "" as a match for every row). */
        memcpy(comm, ki->ki_comm, COMMLEN);
        comm[COMMLEN] = 0;
        nm = comm;
        while (*nm == ' ' || *nm == '\t')
          nm++;
        if ((int)ki->ki_pid <= 0 || !*nm)
          continue;
        is_daemon = is_user_daemon(nm, appinfo.app_id);
        json_escape_name(nm, name_e, sizeof(name_e));
        if (!name_e[0])
          continue;
        mem_mib = MiB((double)ki->ki_rssize * (double)PAGE_SIZE);

        n = snprintf(buf + pos, max_size - pos,
                     "%s {\"pid\":%d,\"name\":\"%s\",\"memory\":%.1f,\"is_daemon\":%s,"
                     "\"app_id\":%u,\"self\":%s,\"protected\":%s}",
                     (count > 0) ? ",\n" : "", (int)ki->ki_pid, name_e, mem_mib,
                     is_daemon ? "true" : "false", (unsigned)appinfo.app_id,
                     ki->ki_pid == getpid() ? "true" : "false",
                     is_protected_proc_name(nm) ? "true" : "false");
        if (n < 0 || (size_t)n >= max_size - pos)
          break;
        pos += (size_t)n;
        count++;
      }
    }
    free(sysctl_buf);
  }

  if (pos + 4 < max_size) {
    pos += (size_t)snprintf(buf + pos, max_size - pos, "\n]}\n");
  } else {
    buf[0] = '{';
    buf[1] = '}';
    buf[2] = 0;
    pos = 2;
  }
  return pos;
}

static int process_name_for_pid(int pid, char *out, size_t outsz) {
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, pid};
  size_t buf_size = 0;
  void *sysctl_buf;
  struct kinfo_proc *ki;
  if (pid <= 0 || !out || !outsz)
    return -1;
  out[0] = 0;
  if (sysctl(mib, 4, NULL, &buf_size, NULL, 0) || !buf_size)
    return -1;
  sysctl_buf = malloc(buf_size);
  if (!sysctl_buf)
    return -1;
  if (sysctl(mib, 4, sysctl_buf, &buf_size, NULL, 0)) {
    free(sysctl_buf);
    return -1;
  }
  ki = (struct kinfo_proc *)sysctl_buf;
  snprintf(out, outsz, "%s", ki->ki_comm);
  free(sysctl_buf);
  return 0;
}

/* Returns 0 on success. PLDMGR-compatible messages. */
static int process_kill_pid(int pid) {
  char name[64];
  if (pid <= 0)
    return -1;
  if (pid == (int)getpid())
    return -1;
  if (process_name_for_pid(pid, name, sizeof(name)) == 0 &&
      is_protected_proc_name(name))
    return -1;
  if (kill((pid_t)pid, SIGKILL) == 0)
    return 0;
  return -1;
}

/* CORS lines for the response being built on the main loop. */
static char g_cors[256];


static const char *http_reason(int code) {
  switch (code) {
  case 200: return "200 OK";
  case 204: return "204 No Content";
  case 400: return "400 Bad Request";
  case 403: return "403 Forbidden";
  case 404: return "404 Not Found";
  case 405: return "405 Method Not Allowed";
  case 409: return "409 Conflict";
  case 411: return "411 Length Required";
  case 413: return "413 Payload Too Large";
  case 415: return "415 Unsupported Media Type";
  case 431: return "431 Request Header Fields Too Large";
  case 502: return "502 Bad Gateway";
  case 503: return "503 Service Unavailable";
  default: return "500 Internal Server Error";
  }
}

static int send_code(int c, int code, const char *ctype, const char *cors,
                     const void *body, size_t n) {
  char hdr[512];
  int h = snprintf(hdr, sizeof(hdr),
                   "HTTP/1.1 %s\r\nContent-Type: %s\r\n"
                   "Content-Length: %zu\r\nCache-Control: no-store\r\n"
                   "%sConnection: close\r\n\r\n",
                   http_reason(code), ctype, n, cors ? cors : "");
  if (h <= 0 || h >= (int)sizeof(hdr) || send_all(c, hdr, (size_t)h))
    return -1;
  return n ? send_all(c, body, n) : 0;
}


static int send_json(int c, int http_ok, const char *body, size_t n) {
  return send_code(c, http_ok ? 200 : 500, "application/json", g_cors, body, n);
}

static int send_blob(int c, const char *ctype, const void *body, size_t n) {
  return send_code(c, 200, ctype, g_cors, body, n);
}

static int send_disk(int c, const char *path) {
  int fd = open(path, O_RDONLY);
  char buf[16384];
  char hdr[512];
  struct stat st;
  const char *ctype = "application/octet-stream";
  ssize_t n;
  if (fd < 0)
    return -1;
  if (fstat(fd, &st) || st.st_size < 0) {
    close(fd);
    return -1;
  }
  if (strstr(path, ".jpg"))
    ctype = "image/jpeg";
  else if (strstr(path, ".png"))
    ctype = "image/png";
  else if (strstr(path, ".html"))
    ctype = "text/html";
  n = snprintf(hdr, sizeof(hdr),
               "HTTP/1.1 200 OK\r\nContent-Type: %s\r\n"
               "Content-Length: %lld\r\n"
               "%sConnection: close\r\n\r\n",
               ctype, (long long)st.st_size, g_cors);
  if (n <= 0 || send_all(c, hdr, (size_t)n)) {
    close(fd);
    return -1;
  }
  while ((n = read(fd, buf, sizeof(buf))) > 0) {
    if (send_all(c, buf, (size_t)n)) {
      close(fd);
      return -1;
    }
  }
  close(fd);
  return n < 0 ? -1 : 0;
}

/* Catalog downloads and Send use ONLY /data/elf-launcher/mirror/<filename>.
 * Ignore zip-style folders (hen/, kernel/, loader/, ...) from older catalogs. */
static int mirror_disk(const char *rel, char *out, size_t outsz) {
  const char *base;
  const char *bs;
  size_t n, i;
  if (!rel || !rel[0] || !out || outsz < 40)
    return -1;
  if (strstr(rel, ".."))
    return -1;
  base = strrchr(rel, '/');
  base = base ? base + 1 : rel;
  bs = strrchr(base, '\\');
  if (bs)
    base = bs + 1;
  n = strlen(base);
  if (n == 0 || n > 180)
    return -1;
  for (i = 0; i < n; i++) {
    unsigned char c = (unsigned char)base[i];
    int ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
             (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-' ||
             c == '+';
    if (!ok)
      return -1;
  }
  if (snprintf(out, outsz, "/data/elf-launcher/mirror/%s", base) >= (int)outsz)
    return -1;
  return 0;
}

static int file_is_elf(const char *disk) {
  unsigned char mag[4];
  int fd;
  ssize_t n;
  fd = open(disk, O_RDONLY);
  if (fd < 0)
    return 0;
  n = read(fd, mag, 4);
  close(fd);
  return n == 4 && mag[0] == 0x7f && mag[1] == 'E' && mag[2] == 'L' &&
         mag[3] == 'F';
}

static int local_file(const char *rel, char *out, size_t outsz) {
  static const char *roots[] = {"/data/elf-launcher/", "/mnt/usb0/elf-launcher/",
                                "/usb0/elf-launcher/", 0};
  const char *base;
  int i;
  if (!rel || !*rel || strstr(rel, ".."))
    return -1;
  base = strrchr(rel, '/');
  base = base ? base + 1 : rel;
  for (i = 0; roots[i]; i++) {
    snprintf(out, outsz, "%s%s", roots[i], rel);
    if (!access(out, R_OK))
      return 0;
    if (base != rel) {
      snprintf(out, outsz, "%s%s", roots[i], base);
      if (!access(out, R_OK))
        return 0;
    }
  }
  return -1;
}

/* One install attempt per process; never block :1000 on AppInstUtil. */
static volatile int home_icon_install_started;

static void *install_home_icon_thread(void *arg) {
  int err;
  int force = (int)(intptr_t)arg;
  /* Elevate/AppInst is process-wide and can disrupt :1000 while WKAL
     is trying to navigate. Serve first; install tile after open window. */
  sleep(2);
  if (!force && home_icon_up_to_date())
    return NULL;
  err = install_home_icon();
  if (err)
    notify("Home icon install failed: 0x%08X", (unsigned)err);
  else
    notify("Home icon installed");
  return NULL;
}

static void start_home_icon_install_async(int force) {
  pthread_t th;
  if (__sync_lock_test_and_set(&home_icon_install_started, 1))
    return;
  if (pthread_create(&th, NULL, install_home_icon_thread,
                      (void *)(intptr_t)force)) {
    /* Keep serving even if the worker could not start. */
    return;
  }
  pthread_detach(th);
}

/* A launcher sent as raw bytes is called payload.elf, but its main thread is
 * named elf-launcher (thr_set_name in main). Other payload.elf are left alone. */
static int proc_has_thread_named(int pid, const char *tname) {
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID | KERN_PROC_INC_THREAD, pid};
  size_t buf_size = 0, tl = strlen(tname);
  void *buf, *ptr;
  int hit = 0;
  if (sysctl(mib, 4, NULL, &buf_size, NULL, 0) || !buf_size)
    return 0;
  buf_size += 4096;
  buf = malloc(buf_size);
  if (!buf)
    return 0;
  if (sysctl(mib, 4, buf, &buf_size, NULL, 0)) {
    free(buf);
    return 0;
  }
  for (ptr = buf; ptr < (void *)((char *)buf + buf_size);) {
    struct kinfo_proc *ki = (struct kinfo_proc *)ptr;
    if (ki->ki_structsize <= 0)
      break;
    ptr = (char *)ptr + ki->ki_structsize;
    if (!strncmp(ki->ki_tdname, tname, tl)) {
      hit = 1;
      break;
    }
  }
  free(buf);
  return hit;
}

/* Kill sibling elf-launcher processes so a re-send can take over :1000.
 * Bypasses is_protected_proc_name (Kill UI still protects the live server). */
static void kill_other_elf_launchers(void) {
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PROC, 0};
  size_t buf_size = 0;
  void *sysctl_buf = NULL;
  void *ptr;
  pid_t self = getpid();

  if (sysctl(mib, 4, NULL, &buf_size, NULL, 0) || !buf_size)
    return;
  sysctl_buf = malloc(buf_size);
  if (!sysctl_buf)
    return;
  if (sysctl(mib, 4, sysctl_buf, &buf_size, NULL, 0)) {
    free(sysctl_buf);
    return;
  }
  for (ptr = sysctl_buf; ptr < (void *)((char *)sysctl_buf + buf_size);) {
    struct kinfo_proc *ki = (struct kinfo_proc *)ptr;
    if (ki->ki_structsize <= 0)
      break;
    ptr = (char *)ptr + ki->ki_structsize;
    if ((pid_t)ki->ki_pid == self || ki->ki_pid <= 0)
      continue;
    if (!strncmp(ki->ki_comm, "elf-launcher", 12) ||
        (!strcmp(ki->ki_comm, "payload.elf") &&
         proc_has_thread_named(ki->ki_pid, "elf-launcher")))
      (void)kill((pid_t)ki->ki_pid, SIGKILL);
  }
  free(sysctl_buf);
}

static int bind_http(int *out_sock) {
  int s = socket(AF_INET, SOCK_STREAM, 0);
  int yes = 1;
  struct sockaddr_in addr;
  if (s < 0)
    return -1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(s);
    return -1;
  }
  *out_sock = s;
  return 0;
}


static void *launch_browser_thread(void *arg) {
  (void)arg;
  /* Default: Launch only (Fpkg). navigateToHome can leave nothing open if
     Launch fails right after; Launch-then-home kills the new browser. */
  sleep(1);
  sceSystemServiceLaunchWebBrowser("http://127.0.0.1:1000/", 0);
  return NULL;
}

static void start_fresh_browser(void) {
  pthread_t th;
  if (pthread_create(&th, NULL, launch_browser_thread, NULL) == 0)
    pthread_detach(th);
}

#define OPEN_AFTER_JB_PATH "/data/elf-launcher/open-after-jb"
#define WKAL_MARK_PATH "/data/elf-launcher/from-wkal"
#define TAKEOVER_MARK_PATH "/data/elf-launcher/update-takeover"
#define BOOT_AUTO_DONE_PATH "/data/elf-launcher/boot-auto-done"

static int64_t mono_secs(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    return 0;
  return (int64_t)ts.tv_sec;
}

static void clear_boot_auto_done(void) { unlink(BOOT_AUTO_DONE_PATH); }

/* Stamp is CLOCK_MONOTONIC seconds. After reboot the clock restarts near 0, so
 * a stamp larger than "now" is from the previous boot and AutoPayload may run
 * once again. */
static int boot_auto_done(void) {
  FILE *f;
  long long stamped = 0;
  int64_t now;
  f = fopen(BOOT_AUTO_DONE_PATH, "r");
  if (!f)
    return 0;
  if (fscanf(f, "%lld", &stamped) != 1) {
    fclose(f);
    unlink(BOOT_AUTO_DONE_PATH);
    return 0;
  }
  fclose(f);
  now = mono_secs();
  if (stamped < 0 || (int64_t)stamped > now) {
    unlink(BOOT_AUTO_DONE_PATH);
    return 0;
  }
  return 1;
}

static int write_boot_auto_done(void) {
  FILE *f;
  mkdir("/data", 0755);
  if (mkdir("/data/elf-launcher", 0755) && errno != EEXIST)
    return -1;
  f = fopen(BOOT_AUTO_DONE_PATH, "w");
  if (!f)
    return -1;
  fprintf(f, "%lld\n", (long long)mono_secs());
  fclose(f);
  return 0;
}

/* Missing file means closed. Only a file whose first byte is 1 opens the browser. */
static int open_after_jb(void) {
  FILE *f;
  int c;
  f = fopen(OPEN_AFTER_JB_PATH, "r");
  if (!f)
    return 0;
  c = fgetc(f);
  fclose(f);
  return c == '1';
}

static int write_open_after_jb(int open_flag) {
  FILE *f;
  mkdir("/data", 0755);
  if (mkdir("/data/elf-launcher", 0755) && errno != EEXIST)
    return -1;
  f = fopen(OPEN_AFTER_JB_PATH, "w");
  if (!f)
    return -1;
  fputc(open_flag ? '1' : '0', f);
  fputc('\n', f);
  fclose(f);
  return 0;
}

/* WK Autoloader sends wkal-mark.elf before elf-launcher.elf. A manual
 * payload send to :9021 never creates this file. */
static int consume_wkal_mark(void) {
  FILE *f = fopen(WKAL_MARK_PATH, "r");
  if (!f)
    return 0;
  fclose(f);
  unlink(WKAL_MARK_PATH);
  return 1;
}

/* A live :1000 answers HTTP. A port that accepts but never replies (old
 * instance wedged, e.g. after rest mode) is not "up": take it over. */
/* Self-update hand-off: the running launcher stamps this file right before it
 * sends the new ELF to elfldr. The new instance then always takes over :1000,
 * and skips AutoPayload and the browser (nothing was jailbroken). */
static int write_takeover_mark(void) {
  FILE *f = fopen(TAKEOVER_MARK_PATH, "w");
  if (!f)
    return -1;
  fprintf(f, "%lld\n", (long long)mono_secs());
  fclose(f);
  return 0;
}

static int consume_takeover_mark(void) {
  FILE *f = fopen(TAKEOVER_MARK_PATH, "r");
  long long at = -1;
  int64_t now = mono_secs();
  if (!f)
    return 0;
  if (fscanf(f, "%lld", &at) != 1)
    at = -1;
  fclose(f);
  unlink(TAKEOVER_MARK_PATH);
  return at >= 0 && at <= now && now - at <= 120;
}

static int http_alive(void) {
  struct timeval tv = {2, 0};
  static const char rq[] = "GET /ip HTTP/1.0\r\nHost: 127.0.0.1\r\n\r\n";
  char buf[16];
  ssize_t n;
  int fd = connect_port(PORT);
  if (fd < 0)
    return 0;
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
  n = send_all(fd, rq, sizeof(rq) - 1) ? -1 : recv(fd, buf, sizeof(buf), 0);
  close(fd);
  return n >= 7 && !memcmp(buf, "HTTP/1.", 7);
}

static int http_port_open(void) {
  int fd = connect_port(PORT);
  if (fd < 0)
    return 0;
  close(fd);
  return 1;
}


#include "update_impl.inc"

static void launch_browser_now(void) {
  /* Caller is about to exit, so this cannot be a detached thread. */
  sleep(1);
  sceSystemServiceLaunchWebBrowser("http://127.0.0.1:1000/", 0);
}

#define AUTO_LIST_PATH "/data/elf-launcher/auto.list"
#define AUTO_NAME_MAX 96
#define AUTO_MAX 64

static int storage_scanned;
static int storage_open_flag = -1;
static char storage_names[AUTO_MAX][AUTO_NAME_MAX];
static int storage_name_count;

static int auto_skip_name(const char *base) {
  if (!base || !base[0])
    return 1;
  if (!strcmp(base, "elfldr-ps5.elf") || !strcmp(base, "elfldr.elf") ||
      !strcmp(base, "payload-manager.elf") || !strcmp(base, "pldmgr.elf") ||
      !strcmp(base, "elf-launcher.elf") || !strcmp(base, "wkal-mark.elf") ||
      !strcmp(base, "installer.elf"))
    return 1;
  return 0;
}

static void add_auto_name(char names[][AUTO_NAME_MAX], int *n, int cap,
                          const char *raw) {
  char base[AUTO_NAME_MAX];
  const char *s;
  const char *bs;
  size_t len;
  int i;
  char dest[512];
  if (!raw || !n || *n >= cap)
    return;
  s = strrchr(raw, '/');
  s = s ? s + 1 : raw;
  bs = strrchr(s, '\\');
  if (bs)
    s = bs + 1;
  len = strlen(s);
  if (len < 5 || len >= sizeof(base))
    return;
  if (strcmp(s + len - 4, ".elf") != 0)
    return;
  memcpy(base, s, len + 1);
  if (auto_skip_name(base))
    return;
  if (mirror_disk(base, dest, sizeof(dest)))
    return;
  for (i = 0; i < *n; i++) {
    if (!strcmp(names[i], base))
      return;
  }
  memcpy(names[*n], base, len + 1);
  (*n)++;
}

static int auto_disk_path(const char *base, char *out, size_t outsz) {
  char mirror[512];
  if (mirror_disk(base, mirror, sizeof(mirror)))
    return -1;
  if (!access(mirror, R_OK) && file_is_elf(mirror)) {
    if (strlen(mirror) + 1 > outsz)
      return -1;
    memcpy(out, mirror, strlen(mirror) + 1);
    return 0;
  }
  if (snprintf(out, outsz, "/data/elf-launcher/%s", base) >= (int)outsz)
    return -1;
  if (!access(out, R_OK) && file_is_elf(out))
    return 0;
  return -1;
}

static int write_auto_names(const char *list) {
  char names[AUTO_MAX][AUTO_NAME_MAX];
  int n = 0;
  const char *p = list ? list : "";
  FILE *f;
  int i;
  mkdir("/data", 0755);
  if (mkdir("/data/elf-launcher", 0755) && errno != EEXIST)
    return -1;
  while (*p && n < AUTO_MAX) {
    char tok[AUTO_NAME_MAX];
    size_t k = 0;
    while (*p && *p != ',' && *p != '\n' && *p != '\r' && k + 1 < sizeof(tok))
      tok[k++] = *p++;
    tok[k] = 0;
    if (*p)
      p++;
    add_auto_name(names, &n, AUTO_MAX, tok);
  }
  f = fopen(AUTO_LIST_PATH, "w");
  if (!f)
    return -1;
  for (i = 0; i < n; i++) {
    fputs(names[i], f);
    fputc('\n', f);
  }
  fclose(f);
  return n;
}

static int read_auto_list(char names[][AUTO_NAME_MAX], int cap) {
  FILE *f = fopen(AUTO_LIST_PATH, "r");
  char line[AUTO_NAME_MAX];
  int n = 0;
  if (!f)
    return 0;
  while (n < cap && fgets(line, sizeof(line), f)) {
    size_t k = strlen(line);
    while (k && (line[k - 1] == '\n' || line[k - 1] == '\r'))
      line[--k] = 0;
    add_auto_name(names, &n, cap, line);
  }
  fclose(f);
  return n;
}

static void urldecode_inplace(char *s) {
  char *r = s;
  char *w = s;
  while (*r) {
    if (*r == '%' && r[1] && r[2]) {
      unsigned int h = 0;
      if (sscanf(r + 1, "%2x", &h) == 1) {
        *w++ = (char)h;
        r += 3;
        continue;
      }
    }
    *w++ = *r++;
  }
  *w = 0;
}

static void take_json_keys(const char *obj, char names[][AUTO_NAME_MAX], int *n,
                           int cap) {
  const char *p = obj;
  while (p && *p && *n < cap) {
    const char *e;
    char key[AUTO_NAME_MAX];
    size_t k;
    p = strchr(p, '"');
    if (!p)
      break;
    p++;
    e = strchr(p, '"');
    if (!e)
      break;
    k = (size_t)(e - p);
    if (k == 0 || k >= sizeof(key)) {
      p = e + 1;
      continue;
    }
    memcpy(key, p, k);
    key[k] = 0;
    p = e + 1;
    while (*p == ' ')
      p++;
    if (*p != ':')
      continue;
    p++;
    while (*p == ' ')
      p++;
    if (*p == '0' || *p == 'f' || *p == 'n' || *p == 'F')
      continue;
    add_auto_name(names, n, cap, key);
  }
}

static void harvest_auto_window(char *window) {
  char *p;
  char *brace;
  char *q;
  int depth;
  urldecode_inplace(window);
  p = strstr(window, "ps5elfs-auto");
  if (!p)
    return;
  p += 12;
  brace = 0;
  for (q = p; *q && (size_t)(q - p) < 96; q++) {
    if (*q == '{') {
      brace = q;
      break;
    }
  }
  if (!brace || brace[1] != '"')
    return;
  depth = 0;
  for (q = brace; *q && (size_t)(q - brace) < 8000; q++) {
    if (*q == '{')
      depth++;
    else if (*q == '}') {
      depth--;
      if (depth == 0) {
        *q = 0;
        break;
      }
    }
  }
  take_json_keys(brace, storage_names, &storage_name_count, AUTO_MAX);
}

static int token_at(const char *p, size_t lim, const char *tok) {
  size_t n = strlen(tok);
  size_t i;
  for (i = 0; i + n <= lim && p[i]; i++) {
    char before;
    char after;
    if (memcmp(p + i, tok, n) != 0)
      continue;
    before = i ? p[i - 1] : 0;
    after = p[i + n];
    if ((before == 0 || before == '=' || before == '"' || before == '\'') &&
        (after == 0 || after == '"' || after == '\'' || after == ';' ||
         after == ' ' || after == ',' || after == '\n' || after == '&'))
      return (int)i;
  }
  return -1;
}

static void note_browser_window(char *window) {
  char *p;
  int open_at;
  int closed_at;
  if (storage_open_flag >= 0)
    return;
  urldecode_inplace(window);
  p = strstr(window, "ps5elfs-browser");
  if (!p)
    return;
  p += 15;
  open_at = token_at(p, 32, "open");
  closed_at = token_at(p, 32, "closed");
  if (open_at >= 0 && (closed_at < 0 || open_at < closed_at))
    storage_open_flag = 1;
  else if (closed_at >= 0)
    storage_open_flag = 0;
}

static void consider_ascii(const unsigned char *b, size_t n) {
  size_t i;
  char window[9000];
  for (i = 0; i < n; i++) {
    size_t take;
    if (b[i] != 'p' || i + 12 >= n || memcmp(b + i, "ps5elfs-", 8) != 0)
      continue;
    take = n - i;
    if (take > sizeof(window) - 1)
      take = sizeof(window) - 1;
    memcpy(window, b + i, take);
    window[take] = 0;
    if (!memcmp(window, "ps5elfs-auto", 12))
      harvest_auto_window(window);
    if (!memcmp(window, "ps5elfs-browser", 15))
      note_browser_window(window);
  }
}

static int find_utf16_key(const unsigned char *b, size_t n, const char *key,
                          size_t *off) {
  size_t klen = strlen(key);
  size_t i, j;
  if (n < klen * 2)
    return 0;
  for (i = 0; i + klen * 2 <= n; i++) {
    for (j = 0; j < klen; j++) {
      if (b[i + j * 2] != (unsigned char)key[j] || b[i + j * 2 + 1] != 0)
        break;
    }
    if (j == klen) {
      *off = i;
      return 1;
    }
  }
  return 0;
}

static void consider_utf16(const unsigned char *b, size_t n, const char *key,
                           int is_auto) {
  size_t off = 0;
  size_t i, o = 0;
  char window[9000];
  if (!find_utf16_key(b, n, key, &off))
    return;
  for (i = off; i + 1 < n && o + 1 < sizeof(window); i += 2) {
    if (b[i + 1] == 0 && b[i] >= 32 && b[i] < 127)
      window[o++] = (char)b[i];
    else
      window[o++] = ' ';
    if (o > 8500)
      break;
  }
  window[o] = 0;
  if (is_auto)
    harvest_auto_window(window);
  else
    note_browser_window(window);
}

static int file_is_store(const char *name, const char *parent) {
  if (!name)
    return 0;
  if (strstr(name, "localstorage") || strstr(name, "LocalStorage") ||
      strstr(name, "cookie") || strstr(name, "Cookie") ||
      strstr(name, "ps5elfs"))
    return 1;
  if (parent && (strstr(parent, "ocal") || strstr(parent, "ookie") ||
                 strstr(parent, "ebKit")))
    return 1;
  return 0;
}

static int dir_worth_entering(const char *name, int depth) {
  size_t n;
  size_t i;
  if (depth <= 2)
    return 1;
  if (!name)
    return 0;
  if (strstr(name, "webkit") || strstr(name, "WebKit") ||
      strstr(name, "shell") || strstr(name, "local") ||
      strstr(name, "Local") || strstr(name, "cookie") ||
      strstr(name, "Cookie") || strstr(name, "home") ||
      strstr(name, "webbrowser") || strstr(name, "WebBrowser"))
    return 1;
  n = strlen(name);
  if (n != 8)
    return 0;
  for (i = 0; i < n; i++) {
    char c = name[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
          (c >= 'A' && c <= 'F')))
      return 0;
  }
  return 1;
}

static void scan_one_file(const char *path) {
  unsigned char *buf;
  struct stat st;
  int fd;
  ssize_t got;
  size_t n = 0;
  if (stat(path, &st) || !S_ISREG(st.st_mode))
    return;
  if (st.st_size <= 0 || st.st_size > 4 * 1024 * 1024)
    return;
  fd = open(path, O_RDONLY);
  if (fd < 0)
    return;
  buf = malloc((size_t)st.st_size);
  if (!buf) {
    close(fd);
    return;
  }
  while (n < (size_t)st.st_size) {
    got = read(fd, buf + n, (size_t)st.st_size - n);
    if (got <= 0)
      break;
    n += (size_t)got;
  }
  close(fd);
  if (n) {
    consider_ascii(buf, n);
    consider_utf16(buf, n, "ps5elfs-auto", 1);
    consider_utf16(buf, n, "ps5elfs-browser", 0);
  }
  free(buf);
}

static void scan_dir(const char *path, int depth, int *budget) {
  DIR *d;
  struct dirent *de;
  if (depth > 8 || *budget <= 0)
    return;
  d = opendir(path);
  if (!d)
    return;
  while ((de = readdir(d)) != NULL && *budget > 0) {
    char child[512];
    struct stat st;
    if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
      continue;
    if (snprintf(child, sizeof(child), "%s/%s", path, de->d_name) >=
        (int)sizeof(child))
      continue;
    if (stat(child, &st))
      continue;
    (*budget)--;
    if (S_ISDIR(st.st_mode)) {
      if (dir_worth_entering(de->d_name, depth))
        scan_dir(child, depth + 1, budget);
    } else if (S_ISREG(st.st_mode) && file_is_store(de->d_name, path)) {
      scan_one_file(child);
    }
  }
  closedir(d);
}

static void scan_page_storage(void) {
  static const char *roots[] = {"/user", "/system_data", "/data",
                                "/mnt/sandbox", 0};
  int i;
  int budget = 500;
  if (storage_scanned)
    return;
  storage_scanned = 1;
  elevate_for_appinst();
  for (i = 0; roots[i]; i++)
    scan_dir(roots[i], 0, &budget);
}

static int wk_wants_open(void) {
  if (!access(OPEN_AFTER_JB_PATH, R_OK))
    return open_after_jb();
  scan_page_storage();
  return storage_open_flag == 1;
}

static int existing_names(char in[][AUTO_NAME_MAX], int n,
                          char out[][AUTO_NAME_MAX], int cap) {
  int i, e = 0;
  char path[512];
  for (i = 0; i < n && e < cap; i++) {
    if (auto_disk_path(in[i], path, sizeof(path)))
      continue;
    memcpy(out[e], in[i], AUTO_NAME_MAX);
    e++;
  }
  return e;
}

/* ---- request reading, CORS and the mutating-endpoint guard ---- */
#define REQ_MAX 8192
#define UPLOAD_MAX (64 * 1024 * 1024)
#define UPLOAD_DIR "/data/elf-launcher/upload"
#define MIRROR_DIR "/data/elf-launcher/mirror"

static int64_t mono_ms(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    return 0;
  return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void sock_timeouts(int fd, int rcv_ms, int snd_ms) {
  struct timeval tv;
  if (rcv_ms >= 0) {
    tv.tv_sec = rcv_ms / 1000;
    tv.tv_usec = (rcv_ms % 1000) * 1000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  }
  if (snd_ms >= 0) {
    tv.tv_sec = snd_ms / 1000;
    tv.tv_usec = (snd_ms % 1000) * 1000;
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
  }
}

/* Read until the blank line after the headers. Body bytes that came along
 * stay in buf after *hdr_len. 0 ok, -1 closed/timeout, -2 headers too big. */
static int read_request(int c, char *buf, size_t cap, size_t *hdr_len,
                        size_t *got) {
  size_t n = 0;
  int64_t deadline = mono_ms() + 15000;
  sock_timeouts(c, 5000, 10000);
  while (n + 1 < cap) {
    ssize_t r = recv(c, buf + n, cap - 1 - n, 0);
    char *e;
    if (r <= 0)
      return -1;
    n += (size_t)r;
    buf[n] = 0;
    e = strstr(buf, "\r\n\r\n");
    if (e) {
      *hdr_len = (size_t)(e - buf) + 4;
      *got = n;
      return 0;
    }
    if (mono_ms() > deadline)
      return -1;
  }
  return -2;
}

/* Header lookup inside [hdrs, end). Case-insensitive name, trimmed value. */
static int hdr_get(const char *hdrs, const char *end, const char *name,
                   char *out, size_t outsz) {
  size_t nl = strlen(name);
  const char *p = hdrs;
  if (!outsz)
    return -1;
  out[0] = 0;
  while (p && p < end) {
    const char *eol = strstr(p, "\r\n");
    if (!eol || eol > end)
      eol = end;
    if (eol == p)
      break;
    if ((size_t)(eol - p) > nl && !strncasecmp(p, name, nl) && p[nl] == ':') {
      const char *v = p + nl + 1;
      size_t k = 0;
      while (v < eol && (*v == ' ' || *v == '\t'))
        v++;
      while (v < eol && k + 1 < outsz)
        out[k++] = *v++;
      while (k && (out[k - 1] == ' ' || out[k - 1] == '\t'))
        k--;
      out[k] = 0;
      return 0;
    }
    p = eol + 2;
  }
  return -1;
}

static void ip4_str(const struct in_addr *a, char *out, size_t outsz) {
  const unsigned char *b = (const unsigned char *)&a->s_addr;
  snprintf(out, outsz, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
}

/* LAN address of the console (no packet is sent; UDP connect only picks a route). */
static int lan_ip(char *out, size_t outsz) {
  struct sockaddr_in a, me;
  socklen_t len = sizeof(me);
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  out[0] = 0;
  if (fd < 0)
    return -1;
  memset(&a, 0, sizeof(a));
  a.sin_family = AF_INET;
  a.sin_port = htons(53);
  a.sin_addr.s_addr = htonl(0x08080808);
  if (connect(fd, (struct sockaddr *)&a, sizeof(a)) ||
      getsockname(fd, (struct sockaddr *)&me, &len)) {
    close(fd);
    out[0] = 0;
    return -1;
  }
  close(fd);
  ip4_str(&me.sin_addr, out, outsz);
  if (!strcmp(out, "0.0.0.0")) {
    out[0] = 0;
    return -1;
  }
  return 0;
}

static int conn_local_ip(int c, char *out, size_t outsz) {
  struct sockaddr_in me;
  socklen_t len = sizeof(me);
  out[0] = 0;
  if (getsockname(c, (struct sockaddr *)&me, &len)) {
    out[0] = 0;
    return -1;
  }
  ip4_str(&me.sin_addr, out, outsz);
  return 0;
}

/* Pages of this project, the console itself (:1000 UI, :1022 WK Autoloader). */
static int origin_allowed(int c, const char *origin) {
  char host[96], ip[48];
  const char *h, *colon, *end;
  size_t hl;
  if (!origin || !origin[0])
    return 0;
  if (!strcasecmp(origin, "https://x-f1reball-x.github.io"))
    return 1;
  if (strncmp(origin, "http://", 7))
    return 0;
  h = origin + 7;
  end = h + strlen(h);
  colon = strchr(h, ':');
  if (!colon)
    return 0;
  if (strcmp(colon + 1, "1000") && strcmp(colon + 1, "1022"))
    return 0;
  (void)end;
  hl = (size_t)(colon - h);
  if (!hl || hl >= sizeof(host))
    return 0;
  memcpy(host, h, hl);
  host[hl] = 0;
  if (!strcmp(host, "127.0.0.1") || !strcasecmp(host, "localhost"))
    return 1;
  if (!conn_local_ip(c, ip, sizeof(ip)) && !strcmp(host, ip))
    return 1;
  if (!lan_ip(ip, sizeof(ip)) && !strcmp(host, ip))
    return 1;
  return 0;
}

/* "https://host[:port]/path..." -> "https://host[:port]" */
static void referer_origin(const char *ref, char *out, size_t outsz) {
  const char *s = strstr(ref, "://");
  const char *e;
  size_t n;
  out[0] = 0;
  if (!s)
    return;
  e = strchr(s + 3, '/');
  n = e ? (size_t)(e - ref) : strlen(ref);
  if (n >= outsz)
    return;
  memcpy(out, ref, n);
  out[n] = 0;
}

static void set_cors_for(int c, const char *origin) {
  g_cors[0] = 0;
  if (origin && origin[0] && origin_allowed(c, origin))
    snprintf(g_cors, sizeof(g_cors),
             "Access-Control-Allow-Origin: %s\r\nVary: Origin\r\n", origin);
}

/* Mutating calls need X-ELFL: 1 (custom header, so a cross-site page cannot
 * send it without a preflight we refuse) or an allowlisted Origin/Referer. */
static int req_trusted(int c, const char *hdrs, const char *end) {
  char v[256], o[200];
  if (!hdr_get(hdrs, end, "X-ELFL", v, sizeof(v)) && !strcmp(v, "1"))
    return 1;
  if (!hdr_get(hdrs, end, "Origin", v, sizeof(v)) && v[0])
    return origin_allowed(c, v);
  if (!hdr_get(hdrs, end, "Referer", v, sizeof(v)) && v[0]) {
    referer_origin(v, o, sizeof(o));
    return origin_allowed(c, o);
  }
  return 0;
}

static int has_origin_or_referer(const char *hdrs, const char *end) {
  char v[16];
  return !hdr_get(hdrs, end, "Origin", v, sizeof(v)) ||
         !hdr_get(hdrs, end, "Referer", v, sizeof(v));
}

static int is_mutating(const char *p, const char *qs) {
  static const char *always[] = {"trigger-auto", "install-home", "file_delete",
                                 "update",       "process_kill", "relay",
                                 "run",          "upload",       "run_path",
                                 "save_path",    "self_update",  0};
  int i;
  for (i = 0; always[i]; i++)
    if (!strcmp(p, always[i]))
      return 1;
  if (!strncmp(p, "load/", 5))
    return 1;
  if (!strncmp(p, "fs/", 3) && strcmp(p, "fs/status") && strcmp(p, "fs/read"))
    return 1;
  if (!strcmp(p, "backup/save") || !strcmp(p, "events/add") || !strcmp(p, "open-browser") ||
      !strcmp(p, "frame-check") || !strcmp(p, "web-save"))
    return 1;
  if (!strcmp(p, "profiles"))
    return 1;
  if (!qs)
    return 0;
  if (!strcmp(p, "boot-auto") && strstr(qs, "done="))
    return 1;
  if (!strcmp(p, "browser-pref") && strstr(qs, "open="))
    return 1;
  if (!strcmp(p, "auto-list") && strstr(qs, "names="))
    return 1;
  return 0;
}

/* Answer before the client finished sending: let it read the reply
 * instead of getting a reset. */
static void drain_briefly(int c) {
  char tmp[4096];
  int64_t deadline = mono_ms() + 500;
  size_t total = 0;
  shutdown(c, SHUT_WR);
  sock_timeouts(c, 100, -1);
  while (mono_ms() < deadline && total < 512 * 1024) {
    ssize_t r = recv(c, tmp, sizeof(tmp), 0);
    if (r == 0)
      break;
    if (r < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
        continue;
      break;
    }
    total += (size_t)r;
  }
}

static int send_json_code(int c, int code, const char *body) {
  return send_code(c, code, "application/json", g_cors, body, strlen(body));
}

static int send_text_code(int c, int code, const char *msg) {
  return send_code(c, code, "text/plain; charset=utf-8", g_cors, msg,
                   strlen(msg));
}

/* ---- payload files ---- */
/* ELF, PS4 SELF or PS5 SELF (same magics elfldr accepts). */
static int magic_ok(const unsigned char *m) {
  if (m[0] == 0x7f && m[1] == 'E' && m[2] == 'L' && m[3] == 'F')
    return 1;
  if (m[0] == 0x4f && m[1] == 0x15 && m[2] == 0x3d && m[3] == 0x1d)
    return 1;
  if (m[0] == 0x54 && m[1] == 0x14 && m[2] == 0xf5 && m[3] == 0xee)
    return 1;
  return 0;
}

static int file_is_payload(const char *disk) {
  unsigned char mag[4];
  int fd = open(disk, O_RDONLY);
  ssize_t n;
  if (fd < 0)
    return 0;
  n = read(fd, mag, 4);
  close(fd);
  return n == 4 && magic_ok(mag);
}

static int has_payload_ext(const char *name) {
  const char *e = strrchr(name, '.');
  return e && (!strcasecmp(e, ".elf") || !strcasecmp(e, ".bin") ||
               !strcasecmp(e, ".self"));
}

/* Upload names: basename, [A-Za-z0-9._+-] only, no leading dot, payload ext. */
static int sanitize_upload_name(const char *raw, char *out, size_t outsz) {
  const char *s = raw;
  const char *b;
  size_t k = 0;
  if (!raw || outsz < 16)
    return -1;
  b = strrchr(s, '/');
  if (b)
    s = b + 1;
  b = strrchr(s, '\\');
  if (b)
    s = b + 1;
  while (*s == '.' || *s == ' ')
    s++;
  for (; *s && k + 6 < outsz && k < 80; s++) {
    unsigned char ch = (unsigned char)*s;
    int ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
             (ch >= '0' && ch <= '9') || ch == '.' || ch == '_' ||
             ch == '-' || ch == '+';
    out[k++] = ok ? (char)ch : '_';
  }
  out[k] = 0;
  if (strstr(out, ".."))
    return -1;
  if (!k)
    snprintf(out, outsz, "upload.elf");
  else if (!has_payload_ext(out))
    strcat(out, ".elf");
  return 0;
}

/* ---- elfldr :9021 ---- */
static volatile int send_busy;
static volatile int elfldr_starting;
static int64_t elfldr_last_start;

static int send_lock_try(void) { return !__sync_lock_test_and_set(&send_busy, 1); }
static void send_lock_wait(void) {
  while (__sync_lock_test_and_set(&send_busy, 1))
    usleep(100000);
}
static void send_unlock(void) { __sync_lock_release(&send_busy); }

/* Start the bundled loader once at a time, at most every few seconds. */
static int start_elfldr_guarded(void) {
  int rc = -1;
  if (__sync_lock_test_and_set(&elfldr_starting, 1))
    return -1;
  if (!elfldr_last_start || mono_ms() - elfldr_last_start > 4000) {
    elfldr_last_start = mono_ms();
    rc = start_real_elfldr();
  }
  __sync_lock_release(&elfldr_starting);
  return rc;
}

static int elfldr_connect(int wait_ms) {
  int fd = connect_port(9021);
  int64_t deadline;
  if (fd >= 0)
    return fd;
  start_elfldr_guarded();
  deadline = mono_ms() + wait_ms;
  while (mono_ms() < deadline) {
    usleep(250000);
    fd = connect_port(9021);
    if (fd >= 0)
      return fd;
  }
  return -1;
}

/* Collect what elfldr (or the payload's stdout) writes back for ~ms. */
static void elfldr_reply(int fd, char *out, size_t outsz, int ms) {
  size_t n = 0, i;
  int64_t deadline = mono_ms() + ms;
  sock_timeouts(fd, 200, -1);
  while (n + 1 < outsz && mono_ms() < deadline) {
    ssize_t r = recv(fd, out + n, outsz - 1 - n, 0);
    if (r == 0)
      break;
    if (r < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
        continue;
      break;
    }
    n += (size_t)r;
  }
  for (i = 0; i < n; i++)
    if (out[i] == 0)
      out[i] = ' ';
  out[n] = 0;
}

static void pct_append(char *out, size_t outsz, size_t *k, const char *s) {
  static const char hx[] = "0123456789ABCDEF";
  for (; *s && *k + 4 < outsz; s++) {
    unsigned char ch = (unsigned char)*s;
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
        (ch >= '0' && ch <= '9') || ch == '/' || ch == '.' || ch == '_' ||
        ch == '-' || ch == '+' || ch == '~') {
      out[(*k)++] = (char)ch;
    } else {
      out[(*k)++] = '%';
      out[(*k)++] = hx[ch >> 4];
      out[(*k)++] = hx[ch & 15];
    }
  }
  out[*k] = 0;
}

/* First "[elfldr.elf] ..." line, or the first line of payload output. */
static void reply_line(const char *rep, char *out, size_t outsz) {
  const char *s = strstr(rep, "[elfldr");
  size_t k = 0;
  if (!s)
    s = rep;
  while (*s == ' ' || *s == '\r' || *s == '\n')
    s++;
  while (*s && *s != '\n' && *s != '\r' && k + 1 < outsz) {
    unsigned char ch = (unsigned char)*s++;
    out[k++] = (ch < 0x20 || ch == '"' || ch == '\\') ? ' ' : (char)ch;
  }
  while (k && out[k - 1] == ' ')
    k--;
  out[k] = 0;
}

enum { SEND_OK = 0, SEND_NOFILE = -1, SEND_NOLOADER = -2, SEND_REJECTED = -3,
       SEND_IO = -4 };

static int push_raw(const char *disk, char *rep, size_t repsz, int reply_ms) {
  char buf[16384];
  ssize_t n;
  int fd, in = open(disk, O_RDONLY);
  if (in < 0)
    return SEND_NOFILE;
  fd = elfldr_connect(5000);
  if (fd < 0) {
    close(in);
    return SEND_NOLOADER;
  }
  sock_timeouts(fd, -1, 5000);
  while ((n = read(in, buf, sizeof(buf))) > 0) {
    if (send_all(fd, buf, (size_t)n)) {
      n = -1;
      break;
    }
  }
  close(in);
  if (n < 0) {
    close(fd);
    return SEND_IO;
  }
  shutdown(fd, SHUT_WR);
  elfldr_reply(fd, rep, repsz, reply_ms);
  close(fd);
  return SEND_OK;
}

/* Send file://<path>?args= so the payload keeps its real name. Falls back to
 * raw bytes if the loader on :9021 does not take URIs. Caller holds the lock.
 * msg gets the elfldr line (error or payload output). */
static int elfldr_send_path(const char *disk, const char *args, char *msg,
                            size_t msgsz, int reply_ms) {
  char uri[1400], rep[1024];
  size_t k = 0;
  int fd, rc;
  if (msg && msgsz)
    msg[0] = 0;
  if (access(disk, R_OK))
    return SEND_NOFILE;
  memcpy(uri, "file://", 8);
  k = 7;
  pct_append(uri, sizeof(uri), &k, disk);
  if (args && args[0]) {
    if (k + 7 < sizeof(uri)) {
      memcpy(uri + k, "?args=", 7);
      k += 6;
    }
    pct_append(uri, sizeof(uri), &k, args);
  }
  if (k + 2 >= sizeof(uri))
    return SEND_IO;
  uri[k++] = '\n';
  uri[k] = 0;
  fd = elfldr_connect(5000);
  if (fd < 0)
    return SEND_NOLOADER;
  sock_timeouts(fd, -1, 5000);
  if (send_all(fd, uri, k)) {
    close(fd);
    return SEND_IO;
  }
  shutdown(fd, SHUT_WR);
  elfldr_reply(fd, rep, sizeof(rep), reply_ms);
  close(fd);
  if (strstr(rep, "Unknown payload format") ||
      strstr(rep, "Error reading URI payload")) {
    /* Older loader or path it cannot read: raw bytes (name becomes payload.elf). */
    if (args && args[0]) {
      if (msg)
        reply_line(rep, msg, msgsz);
      return SEND_REJECTED;
    }
    rc = push_raw(disk, rep, sizeof(rep), reply_ms);
    if (rc != SEND_OK)
      return rc;
  }
  if (msg)
    reply_line(rep, msg, msgsz);
  if (strstr(rep, "[elfldr.elf] Error") || strstr(rep, "Unknown payload format"))
    return SEND_REJECTED;
  return SEND_OK;
}

static int send_http_code(int rc) {
  switch (rc) {
  case SEND_OK: return 200;
  case SEND_NOFILE: return 404;
  case SEND_NOLOADER: return 503;
  case SEND_REJECTED: return 502;
  default: return 502;
  }
}

static const char *send_err_text(int rc) {
  switch (rc) {
  case SEND_OK: return "ok";
  case SEND_NOFILE: return "file missing";
  case SEND_NOLOADER: return "elfldr is not running on :9021";
  case SEND_REJECTED: return "elfldr rejected the file";
  default: return "elfldr did not take the file";
  }
}

/* Newest process whose name matches the file name elfldr gave it. */
static int find_pid_by_name(const char *name) {
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PROC, 0};
  size_t buf_size = 0, nl;
  void *buf, *ptr;
  int best = 0;
  if (!name || !name[0])
    return 0;
  nl = strlen(name);
  if (nl > COMMLEN)
    nl = COMMLEN;
  if (sysctl(mib, 4, NULL, &buf_size, NULL, 0) || !buf_size)
    return 0;
  buf = malloc(buf_size);
  if (!buf)
    return 0;
  if (sysctl(mib, 4, buf, &buf_size, NULL, 0)) {
    free(buf);
    return 0;
  }
  for (ptr = buf; ptr < (void *)((char *)buf + buf_size);) {
    struct kinfo_proc *ki = (struct kinfo_proc *)ptr;
    if (ki->ki_structsize <= 0)
      break;
    ptr = (char *)ptr + ki->ki_structsize;
    if (ki->ki_pid > best && !strncmp(ki->ki_comm, name, nl))
      best = ki->ki_pid;
  }
  free(buf);
  return best;
}

static void add_to_auto_list(const char *base) {
  char listed[AUTO_MAX][AUTO_NAME_MAX];
  char joined[AUTO_MAX * AUTO_NAME_MAX];
  size_t used = 0;
  int n = read_auto_list(listed, AUTO_MAX), i;
  joined[0] = 0;
  for (i = 0; i < n; i++) {
    size_t L = strlen(listed[i]);
    if (!strcmp(listed[i], base))
      return;
    if (used + L + 2 >= sizeof(joined))
      break;
    if (used)
      joined[used++] = ',';
    memcpy(joined + used, listed[i], L + 1);
    used += L;
  }
  if (used + strlen(base) + 2 < sizeof(joined)) {
    if (used)
      joined[used++] = ',';
    snprintf(joined + used, sizeof(joined) - used, "%s", base);
  }
  write_auto_names(joined);
}

/* ---- /browse and /run_path: only under these roots ---- */
static const char *browse_roots[] = {"/data", "/mnt/usb0", "/mnt/usb1",
                                     "/mnt/ext0", "/mnt/ext1", 0};

static int has_dotdot(const char *p) {
  const char *s = p;
  while ((s = strstr(s, "..")) != NULL) {
    if ((s == p || s[-1] == '/') && (s[2] == 0 || s[2] == '/'))
      return 1;
    s += 2;
  }
  return 0;
}

static int under_root(const char *real, const char *root) {
  size_t n = strlen(root);
  return !strncmp(real, root, n) && (real[n] == 0 || real[n] == '/');
}

/* Lexical normalize (no realpath: it is not reliable on the console's libc
 * and resolves nullfs mounts to other names): "." and empty parts are
 * folded, ".." and relative paths refused. "/" stays "/". */
static int path_norm(const char *in, char *norm, size_t nsz, size_t *olen) {
  size_t o = 0;
  const char *s = in;
  if (!in || in[0] != '/' || strlen(in) >= 900 || has_dotdot(in))
    return -1;
  while (*s) {
    const char *e;
    size_t n;
    while (*s == '/')
      s++;
    if (!*s)
      break;
    e = strchr(s, '/');
    n = e ? (size_t)(e - s) : strlen(s);
    if (!(n == 1 && s[0] == '.')) {
      if (o + n + 2 >= nsz)
        return -1;
      norm[o++] = '/';
      memcpy(norm + o, s, n);
      o += n;
    }
    s += n;
  }
  if (!o)
    norm[o++] = '/';
  norm[o] = 0;
  *olen = o;
  return 0;
}

/* Writable areas: /data, /mnt/usbN, /mnt/extN. Returns the root length. */
static size_t writable_root_len(const char *p) {
  const char *q;
  if (under_root(p, "/data"))
    return 5;
  if (strncmp(p, "/mnt/usb", 8) && strncmp(p, "/mnt/ext", 8))
    return 0;
  q = p + 8;
  if (*q < '0' || *q > '9')
    return 0;
  while (*q >= '0' && *q <= '9')
    q++;
  return (*q == 0 || *q == '/') ? (size_t)(q - p) : 0;
}

/* No symlink may appear below index `from`; -2 when a part is missing. */
static int path_nolinks(char *norm, size_t o, size_t from) {
  size_t k;
  struct stat st;
  for (k = from; k <= o; k++) {
    if (norm[k] == '/' || norm[k] == 0) {
      char save = norm[k];
      norm[k] = 0;
      if (lstat(norm, &st)) {
        norm[k] = save;
        return -2;
      }
      norm[k] = save;
      if (S_ISLNK(st.st_mode))
        return -1;
    }
  }
  return 0;
}

/* Write check: inside a writable area, no symlink below its root. */
static int path_allowed(const char *in, char *real, size_t realsz) {
  char norm[PATH_MAX];
  size_t o, rl;
  struct stat st;
  int rc;
  if (path_norm(in, norm, sizeof(norm), &o))
    return -1;
  rl = writable_root_len(norm);
  if (!rl)
    return -1;
  {
    char save = norm[rl];
    norm[rl] = 0;
    rc = lstat(norm, &st) && stat(norm, &st);
    norm[rl] = save;
    if (rc)
      return -2;
  }
  if (o > rl && (rc = path_nolinks(norm, o, rl + 1)))
    return rc;
  if (o + 1 > realsz)
    return -1;
  memcpy(real, norm, o + 1);
  return 0;
}

/* Read check: anywhere on the console. Inside writable areas the same
 * no-symlink rule applies; system areas are read-only, links may resolve. */
static int path_readable(const char *in, char *real, size_t realsz) {
  char norm[PATH_MAX];
  size_t o;
  struct stat st;
  if (path_norm(in, norm, sizeof(norm), &o))
    return -1;
  if (writable_root_len(norm))
    return path_allowed(in, real, realsz);
  if (stat(norm, &st))
    return -2;
  if (o + 1 > realsz)
    return -1;
  memcpy(real, norm, o + 1);
  return 0;
}

static void json_str(char *out, size_t outsz, size_t *pos, const char *s) {
  char e[600];
  json_escape_name(s, e, sizeof(e));
  if (*pos < outsz)
    *pos += (size_t)snprintf(out + *pos, outsz - *pos, "\"%s\"", e);
  if (*pos > outsz)
    *pos = outsz;
}

/* GET /browse            -> {"ok":true,"roots":[{"path","ok"}]}
 * GET /browse?dir=/data  -> {"ok":true,"dir","dirs":[...],
 *                            "files":[{"name","size","ok"}],"seen":n}
 * d_type can be DT_UNKNOWN on exfat/fat USB drives, so fall back to stat. */
static void handle_browse(int c, const char *qs) {
  char dir[1024], real[PATH_MAX];
  size_t cap = 128 * 1024, dpos = 0, fpos = 0;
  char *dbuf, *fbuf, *out;
  DIR *d;
  struct dirent *de;
  int i, nd = 0, nf = 0, seen = 0, budget = 2000, all = 0, wr;
  char allf[4];
  dir[0] = allf[0] = 0;
  if (qs) {
    qget(qs, "dir", dir, sizeof(dir));
    qget(qs, "all", allf, sizeof(allf));
  }
  all = allf[0] == '1';
  if (!dir[0]) {
    char r[512];
    size_t pos = 0;
    pos += (size_t)snprintf(r + pos, sizeof(r) - pos, "{\"ok\":true,\"roots\":[");
    for (i = 0; browse_roots[i]; i++) {
      struct stat st;
      int ok = !stat(browse_roots[i], &st) && S_ISDIR(st.st_mode);
      pos += (size_t)snprintf(r + pos, sizeof(r) - pos,
                              "%s{\"path\":\"%s\",\"ok\":%s}", i ? "," : "",
                              browse_roots[i], ok ? "true" : "false");
    }
    snprintf(r + pos, sizeof(r) - pos, "]}");
    send_json_code(c, 200, r);
    return;
  }
  i = path_readable(dir, real, sizeof(real));
  if (i) {
    send_json_code(c, i == -2 ? 404 : 403,
                   i == -2 ? "{\"ok\":false,\"message\":\"not found\"}"
                           : "{\"ok\":false,\"message\":\"path not allowed\"}");
    return;
  }
  wr = writable_root_len(real) != 0;
  d = opendir(real);
  if (!d) {
    char m[120];
    snprintf(m, sizeof(m),
             "{\"ok\":false,\"message\":\"cannot open folder\",\"errno\":%d}",
             errno);
    send_json_code(c, errno == ENOENT ? 404 : 500, m);
    return;
  }
  dbuf = malloc(cap);
  fbuf = malloc(cap);
  out = malloc(2 * cap + 2048);
  if (!dbuf || !fbuf || !out) {
    free(dbuf);
    free(fbuf);
    free(out);
    closedir(d);
    send_json_code(c, 500, "{\"ok\":false,\"message\":\"no memory\"}");
    return;
  }
  while ((de = readdir(d)) != NULL && budget-- > 0) {
    char child[PATH_MAX];
    struct stat st;
    int isdir = 0, isreg = 0, have_st = 0;
    if (de->d_name[0] == '.' || !de->d_name[0])
      continue;
    seen++;
    if (snprintf(child, sizeof(child), "%s/%s", strcmp(real, "/") ? real : "",
                 de->d_name) >= (int)sizeof(child))
      continue;
    if (de->d_type == DT_DIR)
      isdir = 1;
    else if (de->d_type == DT_REG)
      isreg = 1;
    if (all) {
      if (lstat(child, &st))
        continue; /* unreadable: skip */
      if (S_ISLNK(st.st_mode) && (wr || stat(child, &st)))
        continue; /* links: hidden in writable areas, resolved elsewhere */
      have_st = 1;
      isdir = S_ISDIR(st.st_mode);
      isreg = S_ISREG(st.st_mode);
    }
    if (!isdir && !isreg && !have_st) {
      if (stat(child, &st))
        continue;
      have_st = 1;
      isdir = S_ISDIR(st.st_mode);
      isreg = S_ISREG(st.st_mode);
    }
    if (isdir) {
      if (dpos + 700 >= cap)
        continue;
      if (nd++)
        dbuf[dpos++] = ',';
      if (all) {
        dpos += (size_t)snprintf(dbuf + dpos, cap - dpos, "{\"name\":");
        json_str(dbuf, cap, &dpos, de->d_name);
        dpos += (size_t)snprintf(dbuf + dpos, cap - dpos, ",\"mtime\":%lld}",
                                 have_st ? (long long)st.st_mtime : 0LL);
      } else
        json_str(dbuf, cap, &dpos, de->d_name);
    } else if (isreg && all) {
      int pe = has_payload_ext(de->d_name);
      if (fpos + 800 >= cap)
        continue;
      if (nf++)
        fbuf[fpos++] = ',';
      fpos += (size_t)snprintf(fbuf + fpos, cap - fpos, "{\"name\":");
      json_str(fbuf, cap, &fpos, de->d_name);
      fpos += (size_t)snprintf(fbuf + fpos, cap - fpos,
                               ",\"size\":%lld,\"mtime\":%lld,\"elf\":%s,\"ok\":%s}",
                               (long long)st.st_size, (long long)st.st_mtime,
                               pe ? "true" : "false",
                               pe && file_is_payload(child) ? "true" : "false");
    } else if (isreg && has_payload_ext(de->d_name)) {
      if (fpos + 800 >= cap)
        continue;
      if (!have_st && stat(child, &st))
        continue;
      if (nf++)
        fbuf[fpos++] = ',';
      fpos += (size_t)snprintf(fbuf + fpos, cap - fpos, "{\"name\":");
      json_str(fbuf, cap, &fpos, de->d_name);
      fpos += (size_t)snprintf(fbuf + fpos, cap - fpos,
                               ",\"size\":%lld,\"ok\":%s}",
                               (long long)st.st_size,
                               file_is_payload(child) ? "true" : "false");
    }
  }
  closedir(d);
  dbuf[dpos] = 0;
  fbuf[fpos] = 0;
  {
    size_t pos = 0;
    pos += (size_t)snprintf(out + pos, 2 * cap + 2048 - pos, "{\"ok\":true,\"dir\":");
    json_str(out, 2 * cap + 2048, &pos, real);
    pos += (size_t)snprintf(out + pos, 2 * cap + 2048 - pos,
                            ",\"writable\":%s,\"seen\":%d,\"dirs\":[%s],\"files\":[%s]}",
                            wr ? "true" : "false", seen, dbuf, fbuf);
    send_json_code(c, 200, out);
  }
  free(dbuf);
  free(fbuf);
  free(out);
}

/* ---- file manager: /fs/mkdir, /fs/rename (sync); /fs/copy, /fs/move,
 * /fs/delete (one background job at a time, /fs/status shows progress).
 * Every path goes through path_allowed (only /data, /mnt/usb*, /mnt/ext*,
 * no "..", no symlinks). Roots themselves and the launcher's own state
 * (everything under /data/elf-launcher except Downloads contents) are off
 * limits. Symlinks met while walking are never followed. ---- */
#define FS_MAX_SRC 48
#define FS_MAX_DEPTH 24
#define LAUNCHER_DIR "/data/elf-launcher"
enum { FS_COPY = 1, FS_MOVE = 2, FS_DELETE = 3 };
static volatile int fs_busy;
static struct {
  int op, n, ok, cancel;
  unsigned seq;
  char src[FS_MAX_SRC][PATH_MAX];
  char dest[PATH_MAX];
  long long total, done;
  int items, items_done;
  char cur[256];
  char err[200];
  char *buf;
} fsj;

static int fs_is_root(const char *real) {
  size_t rl = writable_root_len(real);
  return !rl || rl == strlen(real);
}

/* Paths the user may change (create in, delete, move, rename). */
static int fs_touchable(const char *real) {
  if (fs_is_root(real))
    return 0;
  if (under_root(real, LAUNCHER_DIR))
    return under_root(real, MIRROR_DIR) && strcmp(real, MIRROR_DIR);
  return 1;
}

/* A folder the user may create things in. */
static int fs_dest_ok(const char *real) {
  if (under_root(real, LAUNCHER_DIR))
    return under_root(real, MIRROR_DIR);
  return 1;
}

static int fs_name_ok(const char *n) {
  size_t i, l = strlen(n);
  if (!l || l > 200 || !strcmp(n, ".") || !strcmp(n, ".."))
    return 0;
  for (i = 0; i < l; i++) {
    unsigned char ch = (unsigned char)n[i];
    if (ch < 0x20 || ch == '/' || ch == '\\' || ch == 0x7f)
      return 0;
  }
  return 1;
}

/* qget that refuses a value that did not fit (a cut path is a different path). */
static int fs_qget(const char *qs, const char *key, char *out, size_t outsz) {
  if (qget(qs, key, out, outsz))
    return -1;
  return strlen(out) + 1 >= outsz ? -1 : 0;
}

static void fs_set_err(const char *what, const char *path, int e) {
  const char *b = path ? strrchr(path, '/') : 0;
  snprintf(fsj.err, sizeof(fsj.err), "%s%s%s%s%s", what, b ? " " : "",
           b ? b + 1 : "", e ? ": " : "", e ? strerror(e) : "");
}

static void fs_walk_size(const char *path, int depth) {
  struct stat st;
  DIR *d;
  struct dirent *de;
  char child[PATH_MAX];
  if (depth > FS_MAX_DEPTH || lstat(path, &st) || S_ISLNK(st.st_mode))
    return;
  fsj.items++;
  if (S_ISREG(st.st_mode)) {
    fsj.total += st.st_size;
    return;
  }
  if (!S_ISDIR(st.st_mode) || !(d = opendir(path)))
    return;
  while ((de = readdir(d)) != NULL) {
    if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
      continue;
    if (snprintf(child, sizeof(child), "%s/%s", path, de->d_name) >= (int)sizeof(child))
      continue;
    fs_walk_size(child, depth + 1);
  }
  closedir(d);
}

static int fs_copy_file(const char *src, const char *dst, mode_t mode) {
  int in, out, rc = 0;
  ssize_t r;
  in = open(src, O_RDONLY);
  if (in < 0) {
    fs_set_err("cannot read", src, errno);
    return -1;
  }
  out = open(dst, O_WRONLY | O_CREAT | O_EXCL, (mode & 0777) | 0600);
  if (out < 0) {
    fs_set_err("cannot create", dst, errno);
    close(in);
    return -1;
  }
  while ((r = read(in, fsj.buf, 256 * 1024)) > 0) {
    ssize_t off = 0;
    if (fsj.cancel) {
      fs_set_err("cancelled", 0, 0);
      rc = -1;
      break;
    }
    while (off < r) {
      ssize_t w = write(out, fsj.buf + off, (size_t)(r - off));
      if (w <= 0) {
        fs_set_err("write failed", dst, errno);
        rc = -1;
        break;
      }
      off += w;
    }
    if (rc)
      break;
    fsj.done += r;
  }
  if (r < 0 && !rc) {
    fs_set_err("read failed", src, errno);
    rc = -1;
  }
  close(in);
  if (close(out) && !rc) {
    fs_set_err("write failed", dst, errno);
    rc = -1;
  }
  if (rc)
    unlink(dst);
  return rc;
}

static int fs_copy_tree(const char *src, const char *dst, int depth) {
  struct stat st;
  DIR *d;
  struct dirent *de;
  char a[PATH_MAX], b[PATH_MAX];
  int rc = 0;
  if (fsj.cancel) {
    fs_set_err("cancelled", 0, 0);
    return -1;
  }
  if (depth > FS_MAX_DEPTH) {
    fs_set_err("folders nested too deep", src, 0);
    return -1;
  }
  if (lstat(src, &st)) {
    fs_set_err("cannot read", src, errno);
    return -1;
  }
  if (S_ISLNK(st.st_mode))
    return 0;
  snprintf(fsj.cur, sizeof(fsj.cur), "%s", strrchr(src, '/') ? strrchr(src, '/') + 1 : src);
  if (S_ISREG(st.st_mode)) {
    rc = fs_copy_file(src, dst, st.st_mode);
    if (!rc)
      fsj.items_done++;
    return rc;
  }
  if (!S_ISDIR(st.st_mode))
    return 0;
  if (mkdir(dst, 0777)) {
    fs_set_err("cannot create", dst, errno);
    return -1;
  }
  fsj.items_done++;
  if (!(d = opendir(src))) {
    fs_set_err("cannot open", src, errno);
    return -1;
  }
  while (!rc && (de = readdir(d)) != NULL) {
    if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
      continue;
    if (snprintf(a, sizeof(a), "%s/%s", src, de->d_name) >= (int)sizeof(a) ||
        snprintf(b, sizeof(b), "%s/%s", dst, de->d_name) >= (int)sizeof(b)) {
      fs_set_err("path too long", de->d_name, 0);
      rc = -1;
      break;
    }
    rc = fs_copy_tree(a, b, depth + 1);
  }
  closedir(d);
  return rc;
}

static int fs_rm_tree(const char *path, int depth) {
  struct stat st;
  DIR *d;
  struct dirent *de;
  char child[PATH_MAX];
  int rc = 0;
  if (fsj.cancel) {
    fs_set_err("cancelled", 0, 0);
    return -1;
  }
  if (depth > FS_MAX_DEPTH) {
    fs_set_err("folders nested too deep", path, 0);
    return -1;
  }
  if (lstat(path, &st)) {
    if (errno == ENOENT)
      return 0;
    fs_set_err("cannot read", path, errno);
    return -1;
  }
  snprintf(fsj.cur, sizeof(fsj.cur), "%s", strrchr(path, '/') ? strrchr(path, '/') + 1 : path);
  if (!S_ISDIR(st.st_mode)) {
    if (unlink(path)) {
      fs_set_err("cannot delete", path, errno);
      return -1;
    }
    fsj.items_done++;
    if (S_ISREG(st.st_mode))
      fsj.done += st.st_size;
    return 0;
  }
  if (!(d = opendir(path))) {
    fs_set_err("cannot open", path, errno);
    return -1;
  }
  while (!rc && (de = readdir(d)) != NULL) {
    if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
      continue;
    if (snprintf(child, sizeof(child), "%s/%s", path, de->d_name) >= (int)sizeof(child)) {
      fs_set_err("path too long", de->d_name, 0);
      rc = -1;
      break;
    }
    rc = fs_rm_tree(child, depth + 1);
  }
  closedir(d);
  if (!rc && rmdir(path)) {
    fs_set_err("cannot delete", path, errno);
    rc = -1;
  }
  if (!rc)
    fsj.items_done++;
  return rc;
}

/* "a.elf" -> "a copy.elf", "a copy 2.elf", ... (folders: no extension). */
static int fs_unique_target(const char *dir, const char *base, int isdir, char *out, size_t outsz) {
  struct stat st;
  char stem[256], ext[64];
  const char *dot = isdir ? 0 : strrchr(base, '.');
  int i;
  if (snprintf(out, outsz, "%s/%s", dir, base) >= (int)outsz)
    return -1;
  if (lstat(out, &st))
    return 0;
  if (!dot || dot == base || strlen(dot) >= sizeof(ext))
    dot = base + strlen(base);
  snprintf(stem, sizeof(stem), "%.*s", (int)(dot - base), base);
  snprintf(ext, sizeof(ext), "%s", dot);
  for (i = 1; i < 100; i++) {
    int n = i == 1 ? snprintf(out, outsz, "%s/%s copy%s", dir, stem, ext)
                   : snprintf(out, outsz, "%s/%s copy %d%s", dir, stem, i, ext);
    if (n >= (int)outsz)
      return -1;
    if (lstat(out, &st))
      return 0;
  }
  return -1;
}

static void *fs_job_thread(void *arg) {
  int i, rc = 0;
  (void)arg;
  for (i = 0; i < fsj.n; i++)
    fs_walk_size(fsj.src[i], 0);
  fsj.buf = malloc(256 * 1024);
  if (!fsj.buf) {
    snprintf(fsj.err, sizeof(fsj.err), "no memory");
    rc = -1;
  }
  for (i = 0; !rc && i < fsj.n; i++) {
    const char *src = fsj.src[i], *base = strrchr(src, '/') + 1;
    char dst[PATH_MAX];
    struct stat st;
    if (lstat(src, &st)) {
      fs_set_err("missing", src, errno);
      rc = -1;
      break;
    }
    if (fsj.op == FS_DELETE) {
      rc = fs_rm_tree(src, 0);
      continue;
    }
    if (fsj.op == FS_MOVE) {
      if (snprintf(dst, sizeof(dst), "%s/%s", fsj.dest, base) >= (int)sizeof(dst)) {
        fs_set_err("path too long", src, 0);
        rc = -1;
        break;
      }
      if (!strcmp(dst, src))
        continue;
      if (!lstat(dst, &st)) {
        fs_set_err("already exists:", dst, 0);
        rc = -1;
        break;
      }
      snprintf(fsj.cur, sizeof(fsj.cur), "%s", base);
      if (!rename(src, dst)) {
        fsj.items_done++;
        continue;
      }
      if (errno != EXDEV) {
        fs_set_err("cannot move", src, errno);
        rc = -1;
        break;
      }
      /* other drive: copy, then delete the original only if the copy is whole */
      rc = fs_copy_tree(src, dst, 0);
      if (rc) {
        int keep = fsj.cancel;
        char e[200];
        memcpy(e, fsj.err, sizeof(e));
        fsj.cancel = 0;
        fs_rm_tree(dst, 0);
        fsj.cancel = keep;
        memcpy(fsj.err, e, sizeof(e));
        break;
      }
      rc = fs_rm_tree(src, 0);
      continue;
    }
    if (fs_unique_target(fsj.dest, base, S_ISDIR(st.st_mode), dst, sizeof(dst))) {
      fs_set_err("no free name for", src, 0);
      rc = -1;
      break;
    }
    rc = fs_copy_tree(src, dst, 0);
    if (rc) {
      int keep = fsj.cancel;
      char e[200];
      memcpy(e, fsj.err, sizeof(e));
      fsj.cancel = 0;
      fs_rm_tree(dst, 0);
      fsj.cancel = keep;
      memcpy(fsj.err, e, sizeof(e));
    }
  }
  free(fsj.buf);
  fsj.buf = 0;
  fsj.ok = !rc;
  fsj.cur[0] = 0;
  __sync_synchronize();
  __sync_lock_release(&fs_busy);
  return 0;
}

static void fs_reply_bad(int c, int code, const char *msg) {
  char m[200], e[240];
  json_escape_name(msg, e, sizeof(e));
  snprintf(m, sizeof(m), "{\"ok\":false,\"message\":\"%s\"}", e);
  send_json_code(c, code, m);
}

/* rc from path_allowed -> reply; 0 = fine */
static int fs_check(int c, const char *in, char *real, size_t realsz, int must_exist) {
  int rc = must_exist == 2 ? path_readable(in, real, realsz) : path_allowed(in, real, realsz);
  if (rc == -2 && !must_exist)
    return 0;
  if (rc) {
    fs_reply_bad(c, rc == -2 ? 404 : 403, rc == -2 ? "not found" : "path not allowed");
    return -1;
  }
  return 0;
}

/* GET /fs/read?path= : first 512 KB of a file, read-only, anywhere readable.
 * X-File-Size / X-Truncated / X-Binary / X-Mtime describe it; binary files
 * (a NUL in the first 8 KB) come back with an empty body. */
#define FS_READ_CAP (512 * 1024)
static void handle_fs_read(int c, const char *qs) {
  char in[1024], real[PATH_MAX], extra[480];
  struct stat st;
  char *buf;
  size_t n = 0;
  ssize_t r;
  int fd, rc, bin;
  if (fs_qget(qs, "path", in, sizeof(in))) {
    fs_reply_bad(c, 400, "no path");
    return;
  }
  rc = path_readable(in, real, sizeof(real));
  if (rc) {
    fs_reply_bad(c, rc == -2 ? 404 : 403, rc == -2 ? "not found" : "path not allowed");
    return;
  }
  if (stat(real, &st) || !S_ISREG(st.st_mode)) {
    fs_reply_bad(c, 404, "not a file");
    return;
  }
  fd = open(real, O_RDONLY);
  if (fd < 0) {
    char m[120];
    snprintf(m, sizeof(m), "cannot read: %s", strerror(errno));
    fs_reply_bad(c, 403, m);
    return;
  }
  buf = malloc(FS_READ_CAP);
  if (!buf) {
    close(fd);
    fs_reply_bad(c, 500, "no memory");
    return;
  }
  while (n < FS_READ_CAP && (r = read(fd, buf + n, FS_READ_CAP - n)) > 0)
    n += (size_t)r;
  close(fd);
  bin = memchr(buf, 0, n < 8192 ? n : 8192) != NULL;
  snprintf(extra, sizeof(extra),
           "%sX-File-Size: %lld\r\nX-Truncated: %d\r\nX-Binary: %d\r\nX-Mtime: %lld\r\n",
           g_cors, (long long)st.st_size, (long long)n < (long long)st.st_size, bin,
           (long long)st.st_mtime);
  send_code(c, 200, bin ? "application/octet-stream" : "text/plain; charset=utf-8",
            extra, buf, bin ? 0 : n);
  free(buf);
}

/* GET /icon?url=https://... : a catalog payload's icon, fetched once from
 * GitHub (same host allowlist as downloads) and kept under
 * /data/elf-launcher/icons. Only PNG/JPEG/GIF/WebP up to 1 MB. A failed
 * fetch is remembered for an hour so a bad URL does not stall the page. */
#define ICON_DIR LAUNCHER_DIR "/icons"
static const char *icon_ctype(const uint8_t *m, size_t n) {
  if (n >= 8 && m[0] == 0x89 && m[1] == 'P' && m[2] == 'N' && m[3] == 'G')
    return "image/png";
  if (n >= 3 && m[0] == 0xff && m[1] == 0xd8 && m[2] == 0xff)
    return "image/jpeg";
  if (n >= 6 && !memcmp(m, "GIF8", 4))
    return "image/gif";
  if (n >= 12 && !memcmp(m, "RIFF", 4) && !memcmp(m + 8, "WEBP", 4))
    return "image/webp";
  return 0;
}

static void icon_send(int c, const uint8_t *buf, size_t n, const char *ctype) {
  char extra[300];
  snprintf(extra, sizeof(extra), "%sX-Icon: 1\r\n", g_cors);
  send_code(c, 200, ctype, extra, buf, n);
}

static void handle_icon(int c, const char *qs) {
  char url[1400], path[200], bad[210], tmp[220];
  uint8_t h[32], *buf = 0;
  size_t blen = 0;
  sha256_ctx ctx;
  struct stat st;
  const char *ct;
  int i, fd;
  url[0] = 0;
  if (fs_qget(qs, "url", url, sizeof(url)) || !url_allowed_for_update(url)) {
    fs_reply_bad(c, 403, "url not allowed");
    return;
  }
  sha256_init(&ctx);
  sha256_update(&ctx, (const uint8_t *)url, strlen(url));
  sha256_final(&ctx, h);
  i = snprintf(path, sizeof(path), "%s/", ICON_DIR);
  for (fd = 0; fd < 10; fd++)
    i += snprintf(path + i, sizeof(path) - i, "%02x", h[fd]);
  snprintf(bad, sizeof(bad), "%s.bad", path);
  snprintf(tmp, sizeof(tmp), "%s.part", path);
  if (!stat(path, &st) && st.st_size > 0 && st.st_size <= 1024 * 1024) {
    fd = open(path, O_RDONLY);
    if (fd >= 0) {
      buf = malloc((size_t)st.st_size);
      if (buf && read(fd, buf, (size_t)st.st_size) == st.st_size &&
          (ct = icon_ctype(buf, (size_t)st.st_size))) {
        close(fd);
        icon_send(c, buf, (size_t)st.st_size, ct);
        free(buf);
        return;
      }
      free(buf);
      buf = 0;
      close(fd);
    }
    unlink(path);
  }
  if (!stat(bad, &st) && time(0) - st.st_mtime < 3600) {
    fs_reply_bad(c, 404, "icon unavailable");
    return;
  }
  if (https_download_url(url, &buf, &blen) || !buf || !blen || blen > 1024 * 1024 ||
      !(ct = icon_ctype(buf, blen))) {
    free(buf);
    mkdir(LAUNCHER_DIR, 0755);
    mkdir(ICON_DIR, 0755);
    fd = open(bad, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0)
      close(fd);
    fs_reply_bad(c, 404, "icon unavailable");
    return;
  }
  mkdir(LAUNCHER_DIR, 0755);
  mkdir(ICON_DIR, 0755);
  fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd >= 0) {
    int ok = write(fd, buf, blen) == (ssize_t)blen;
    if (close(fd) || !ok || rename(tmp, path))
      unlink(tmp);
    unlink(bad);
  }
  icon_send(c, buf, blen, ct);
  free(buf);
}

/* ---- session event log (since this launcher started = since jailbreak) ---- */
#define EV_MAX 160
typedef struct {
  long long t;
  char kind[12];
  char msg[200];
} ev_t;
static ev_t ev_ring[EV_MAX];
static int ev_head, ev_count;
static long long ev_seq, ev_started;
static pthread_mutex_t ev_mx = PTHREAD_MUTEX_INITIALIZER;
static void evlog(const char *kind, const char *fmt, ...) {
  va_list ap;
  ev_t *e;
  pthread_mutex_lock(&ev_mx);
  e = &ev_ring[ev_head];
  e->t = (long long)time(NULL);
  snprintf(e->kind, sizeof(e->kind), "%s", kind);
  va_start(ap, fmt);
  vsnprintf(e->msg, sizeof(e->msg), fmt, ap);
  va_end(ap);
  ev_head = (ev_head + 1) % EV_MAX;
  if (ev_count < EV_MAX)
    ev_count++;
  ev_seq++;
  pthread_mutex_unlock(&ev_mx);
}
static void handle_events(int c) {
  char *out = malloc(EV_MAX * 460 + 256), k[40], m[420];
  size_t pos;
  int i, idx;
  if (!out) {
    send_json_code(c, 500, "{\"ok\":false}");
    return;
  }
  pthread_mutex_lock(&ev_mx);
  pos = (size_t)sprintf(out, "{\"ok\":true,\"started\":%lld,\"now\":%lld,\"seq\":%lld,\"events\":[",
                        ev_started, (long long)time(NULL), ev_seq);
  for (i = 0; i < ev_count; i++) {
    idx = (ev_head - ev_count + i + EV_MAX) % EV_MAX;
    json_escape_name(ev_ring[idx].kind, k, sizeof(k));
    json_escape_name(ev_ring[idx].msg, m, sizeof(m));
    pos += (size_t)sprintf(out + pos, "%s{\"t\":%lld,\"kind\":\"%s\",\"msg\":\"%s\"}", i ? "," : "",
                           ev_ring[idx].t, k, m);
  }
  pthread_mutex_unlock(&ev_mx);
  strcpy(out + pos, "]}");
  send_json_code(c, 200, out);
  free(out);
}
/* POST /events/add?kind=crash|error|load|kill|update|info&msg= (from the page) */
static void handle_events_add(int c, const char *qs) {
  static const char *kinds[] = {"crash", "error", "load", "kill", "update", "info", 0};
  char k[16], m[200];
  int i;
  k[0] = m[0] = 0;
  qget(qs, "kind", k, sizeof(k));
  qget(qs, "msg", m, sizeof(m));
  for (i = 0; kinds[i]; i++)
    if (!strcmp(k, kinds[i]))
      break;
  if (!kinds[i] || !m[0]) {
    fs_reply_bad(c, 400, "bad event");
    return;
  }
  for (i = 0; m[i]; i++)
    if ((unsigned char)m[i] < 32)
      m[i] = ' ';
  evlog(k, "%s", m);
  send_json_code(c, 200, "{\"ok\":true}");
}

/* ---- request body (small JSON posts) ---- */
static char *read_body(int c, const char *req, size_t hlen, size_t got, const char *hdrs,
                       const char *hend, size_t max, size_t *outn, const char **why) {
  char v[32];
  long long cl;
  size_t have;
  char *body;
  *why = "length required";
  if (hdr_get(hdrs, hend, "Content-Length", v, sizeof(v)) || (cl = atoll(v)) < 2)
    return NULL;
  *why = "too big";
  if ((size_t)cl > max)
    return NULL;
  *why = "no memory";
  body = malloc((size_t)cl + 1);
  if (!body)
    return NULL;
  have = got > hlen ? got - hlen : 0;
  if (have > (size_t)cl)
    have = (size_t)cl;
  memcpy(body, req + hlen, have);
  while (have < (size_t)cl) {
    ssize_t r = recv(c, body + have, (size_t)cl - have, 0);
    if (r <= 0)
      break;
    have += (size_t)r;
  }
  body[have] = 0;
  *why = "incomplete body";
  if (have != (size_t)cl) {
    free(body);
    return NULL;
  }
  *outn = have;
  return body;
}

/* ---- AutoPayload profiles: /data/elf-launcher/profiles.json ---- */
#define PROFILES_FILE LAUNCHER_DIR "/profiles.json"
static void handle_profiles(int c, int is_post, const char *req, size_t hlen, size_t got,
                            const char *hdrs, const char *hend) {
  if (!is_post) {
    char *buf;
    struct stat st;
    int fd = open(PROFILES_FILE, O_RDONLY);
    if (fd < 0 || fstat(fd, &st) || st.st_size > 65536 || !(buf = malloc((size_t)st.st_size + 1))) {
      if (fd >= 0)
        close(fd);
      send_json_code(c, 200, "{\"ok\":true,\"profiles\":null}");
      return;
    }
    {
      ssize_t r = read(fd, buf, (size_t)st.st_size);
      char *out;
      close(fd);
      buf[r > 0 ? r : 0] = 0;
      out = malloc((size_t)(r > 0 ? r : 0) + 64);
      if (out && r > 1 && buf[0] == '{') {
        sprintf(out, "{\"ok\":true,\"profiles\":%s}", buf);
        send_json_code(c, 200, out);
      } else
        send_json_code(c, 200, "{\"ok\":true,\"profiles\":null}");
      free(out);
      free(buf);
    }
    return;
  } else {
    const char *why;
    size_t n;
    char *body = read_body(c, req, hlen, got, hdrs, hend, 65536, &n, &why);
    int ok;
    if (!body) {
      fs_reply_bad(c, 400, why);
      return;
    }
    if (body[0] != '{' || body[n - 1] != '}') {
      free(body);
      fs_reply_bad(c, 400, "not JSON");
      return;
    }
    mkdir(LAUNCHER_DIR, 0777);
    ok = !write_atomic_under_data(PROFILES_FILE, (const uint8_t *)body, n);
    free(body);
    if (ok)
      send_json_code(c, 200, "{\"ok\":true}");
    else
      fs_reply_bad(c, 500, "write failed");
  }
}

/* ---- system info: temperatures, memory, free space ---- */
int sceKernelGetCpuTemperature(int *);
int sceKernelGetSocSensorTemperature(int, int *);
static void handle_sysinfo(int c) {
  static const char *places[] = {"/data", "/mnt/usb0", "/mnt/usb1", "/mnt/usb2", "/mnt/usb3",
                                 "/mnt/ext0", "/mnt/ext1", 0};
  char out[2048];
  size_t pos;
  int t = 0, i, n = 0;
  long long physmem = 0;
  unsigned int fc = 0, ic = 0, pg = 0;
  size_t sz;
  struct statfs sf;
  struct stat st, mst;
  pos = (size_t)snprintf(out, sizeof(out), "{\"ok\":true,\"uptime\":%lld",
                         (long long)time(NULL) - ev_started);
  if (sceKernelGetCpuTemperature(&t) == 0 && t > 0 && t < 150)
    pos += (size_t)snprintf(out + pos, sizeof(out) - pos, ",\"cpuTemp\":%d", t);
  t = 0;
  if (sceKernelGetSocSensorTemperature(0, &t) == 0 && t > 0 && t < 150)
    pos += (size_t)snprintf(out + pos, sizeof(out) - pos, ",\"socTemp\":%d", t);
  sz = sizeof(physmem);
  if (sysctlbyname("hw.physmem", &physmem, &sz, NULL, 0))
    physmem = 0;
  sz = sizeof(pg);
  if (sysctlbyname("hw.pagesize", &pg, &sz, NULL, 0))
    pg = 0;
  sz = sizeof(fc);
  if (sysctlbyname("vm.stats.vm.v_free_count", &fc, &sz, NULL, 0))
    fc = 0;
  sz = sizeof(ic);
  if (sysctlbyname("vm.stats.vm.v_inactive_count", &ic, &sz, NULL, 0))
    ic = 0;
  if (physmem > 0 && pg && fc)
    pos += (size_t)snprintf(out + pos, sizeof(out) - pos, ",\"memTotal\":%lld,\"memFree\":%lld",
                            physmem, ((long long)fc + ic) * pg);
  pos += (size_t)snprintf(out + pos, sizeof(out) - pos, ",\"disks\":[");
  if (stat("/mnt", &mst))
    mst.st_dev = 0;
  for (i = 0; places[i]; i++) {
    if (stat(places[i], &st) || !S_ISDIR(st.st_mode))
      continue;
    /* an empty /mnt/usbN folder with nothing mounted is not a drive */
    if (i && st.st_dev == mst.st_dev)
      continue;
    if (statfs(places[i], &sf) || !sf.f_blocks)
      continue;
    pos += (size_t)snprintf(out + pos, sizeof(out) - pos,
                            "%s{\"path\":\"%s\",\"free\":%lld,\"total\":%lld}", n++ ? "," : "",
                            places[i], (long long)sf.f_bavail * (long long)sf.f_bsize,
                            (long long)sf.f_blocks * (long long)sf.f_bsize);
  }
  snprintf(out + pos, sizeof(out) - pos, "]}");
  send_json_code(c, 200, out);
}

/* ---- POST /fs/upload?dir=&name= : raw body into a writable folder ---- */
#define FSUP_MAX (4LL * 1024 * 1024 * 1024)
typedef struct {
  int c;
  long long clen;
  size_t pre_n;
  char *pre;
  char dst[PATH_MAX + 200];
  char cors[256];
} fsup_t;
static volatile int fsup_busy;
static void *fsup_thread(void *arg) {
  fsup_t *j = arg;
  char tmp[PATH_MAX + 220], *b = malloc(65536), json[300];
  long long have = 0;
  int fd, ok = 1, code = 200;
  const char *m = "";
  snprintf(tmp, sizeof(tmp), "%s.part", j->dst);
  fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0 || !b) {
    ok = 0;
    code = 500;
    m = "cannot write here";
  }
  if (ok && j->pre_n) {
    ok = write(fd, j->pre, j->pre_n) == (ssize_t)j->pre_n;
    have = (long long)j->pre_n;
  }
  while (ok && have < j->clen) {
    size_t want = (size_t)(j->clen - have > 65536 ? 65536 : j->clen - have);
    ssize_t r = recv(j->c, b, want, 0);
    if (r <= 0) {
      ok = 0;
      code = 400;
      m = "upload incomplete";
      break;
    }
    if (write(fd, b, (size_t)r) != r) {
      ok = 0;
      code = 507;
      m = "disk full or write failed";
      break;
    }
    have += r;
  }
  if (fd >= 0 && close(fd) && ok) {
    ok = 0;
    code = 507;
    m = "disk full or write failed";
  }
  if (ok && rename(tmp, j->dst)) {
    ok = 0;
    code = 500;
    m = "rename failed";
  }
  if (!ok) {
    if (!m[0])
      m = "write failed";
    unlink(tmp);
    evlog("error", "upload to %s failed: %s", j->dst, m);
  } else
    evlog("info", "uploaded %s (%lld bytes)", j->dst, have);
  {
    char e[PATH_MAX + 220];
    json_escape_name(ok ? j->dst : m, e, sizeof(e));
    snprintf(json, sizeof(json), ok ? "{\"ok\":true,\"path\":\"%s\",\"bytes\":%lld}" : "{\"ok\":false,\"message\":\"%s\",\"bytes\":%lld}", e, have);
  }
  send_code(j->c, code, "application/json", j->cors, json, strlen(json));
  close(j->c);
  free(b);
  free(j->pre);
  free(j);
  fsup_busy = 0;
  return NULL;
}
/* returns 1 when the socket was handed to the worker */
static int handle_fs_upload(int c, const char *qs, const char *req, size_t hlen, size_t got,
                            const char *hdrs, const char *hend) {
  char in[1024], dir[PATH_MAX], name[256], clean[256], v[32];
  long long cl;
  fsup_t *j;
  pthread_t th;
  size_t i, k = 0;
  struct stat st;
  if (fs_qget(qs, "dir", in, sizeof(in)) || fs_qget(qs, "name", name, sizeof(name))) {
    fs_reply_bad(c, 400, "no folder or name");
    return 0;
  }
  if (fs_check(c, in, dir, sizeof(dir), 1))
    return 0;
  if (!fs_dest_ok(dir)) {
    fs_reply_bad(c, 403, "protected");
    return 0;
  }
  if (stat(dir, &st) || !S_ISDIR(st.st_mode)) {
    fs_reply_bad(c, 404, "not a folder");
    return 0;
  }
  for (i = 0; name[i] && k + 1 < sizeof(clean); i++) {
    unsigned char ch = (unsigned char)name[i];
    if (ch == '/' || ch == '\\' || ch < 32)
      ch = '_';
    clean[k++] = (char)ch;
  }
  clean[k] = 0;
  if (!k || clean[0] == '.' || strlen(clean) > 200) {
    fs_reply_bad(c, 400, "bad name");
    return 0;
  }
  if (hdr_get(hdrs, hend, "Content-Length", v, sizeof(v)) || (cl = atoll(v)) < 0) {
    fs_reply_bad(c, 411, "length required");
    return 0;
  }
  if (cl > FSUP_MAX) {
    fs_reply_bad(c, 413, "file too big (4 GB max)");
    return 0;
  }
  if (fsup_busy) {
    fs_reply_bad(c, 409, "busy");
    return 0;
  }
  j = calloc(1, sizeof(*j));
  if (!j) {
    fs_reply_bad(c, 500, "no memory");
    return 0;
  }
  snprintf(j->dst, sizeof(j->dst), "%s/%s", strcmp(dir, "/") ? dir : "", clean);
  if (!lstat(j->dst, &st)) {
    free(j);
    fs_reply_bad(c, 409, "exists");
    return 0;
  }
  j->c = c;
  j->clen = cl;
  snprintf(j->cors, sizeof(j->cors), "%s", g_cors);
  if (got > hlen) {
    j->pre_n = got - hlen;
    if ((long long)j->pre_n > cl)
      j->pre_n = (size_t)cl;
    j->pre = malloc(j->pre_n ? j->pre_n : 1);
    if (!j->pre) {
      free(j);
      fs_reply_bad(c, 500, "no memory");
      return 0;
    }
    memcpy(j->pre, req + hlen, j->pre_n);
  }
  if (!hdr_get(hdrs, hend, "Expect", v, sizeof(v)) && !strcasecmp(v, "100-continue"))
    send_all(c, "HTTP/1.1 100 Continue\r\n\r\n", 25);
  fsup_busy = 1;
  if (pthread_create(&th, NULL, fsup_thread, j)) {
    fsup_thread(j);
    return 1;
  }
  pthread_detach(th);
  return 1;
}

/* ---- in-page Browser helpers ----
 * GET /open-browser?url=   opens a page in the PS5's own web browser
 * GET /frame-check?url=    reads the page's headers (no body) and says whether
 *                          it allows being shown inside a frame
 * Both need the page's X-ELFL header (see is_mutating). */
static int web_url_ok(const char *u) {
  size_t i, n = strlen(u);
  if (n < 8 || n > 1000)
    return 0;
  if (strncasecmp(u, "https://", 8) && strncasecmp(u, "http://", 7))
    return 0;
  for (i = 0; i < n; i++)
    if ((unsigned char)u[i] <= 32 || u[i] == '"' || u[i] == '\\' || u[i] == '<' || u[i] == '>')
      return 0;
  return 1;
}
static void *open_browser_thread(void *arg) {
  char *u = arg;
  sceSystemServiceLaunchWebBrowser(u, 0);
  free(u);
  return NULL;
}
static void handle_open_browser(int c, const char *qs) {
  char url[1100];
  pthread_t th;
  char *dup;
  url[0] = 0;
  qget(qs, "url", url, sizeof(url));
  if (!web_url_ok(url)) {
    fs_reply_bad(c, 400, "bad url");
    return;
  }
  dup = strdup(url);
  if (!dup || pthread_create(&th, NULL, open_browser_thread, dup)) {
    free(dup);
    fs_reply_bad(c, 500, "cannot open the browser");
    return;
  }
  pthread_detach(th);
  evlog("info", "opened %s in the PS5 browser", url);
  send_json_code(c, 200, "{\"ok\":true}");
}

/* 1 blocked, 0 allowed, from X-Frame-Options / CSP frame-ancestors */
static int frame_blocked_by(const char *h, size_t n, char *why, size_t whysz) {
  const char *p = h, *end = h + n, *ls, *le, *v;
  for (ls = p; ls < end; ls = le + 1) {
    char line[1200];
    size_t L;
    le = memchr(ls, '\n', (size_t)(end - ls));
    if (!le)
      le = end;
    L = (size_t)(le - ls);
    if (L >= sizeof(line))
      L = sizeof(line) - 1;
    memcpy(line, ls, L);
    line[L] = 0;
    if (L && line[L - 1] == '\r')
      line[--L] = 0;
    if (!strncasecmp(line, "X-Frame-Options:", 16)) {
      v = line + 16;
      while (*v == ' ')
        v++;
      if (!strncasecmp(v, "deny", 4) || !strncasecmp(v, "sameorigin", 10)) {
        snprintf(why, whysz, "X-Frame-Options: %.40s", v);
        return 1;
      }
    }
    if (!strncasecmp(line, "Content-Security-Policy:", 24)) {
      char *fa, *q2;
      for (q2 = line; *q2; q2++)
        if (*q2 >= 'A' && *q2 <= 'Z')
          *q2 = (char)(*q2 + 32);
      fa = strstr(line, "frame-ancestors");
      if (fa) {
        char *semi = strchr(fa, ';');
        if (semi)
          *semi = 0;
        /* only a bare * lets any site frame it */
        if (!strstr(fa + 15, " *") && !strstr(fa + 15, "\t*")) {
          snprintf(why, whysz, "CSP %.60s", fa);
          return 1;
        }
      }
    }
    if (le == end)
      break;
  }
  return 0;
}
typedef struct {
  int c;
  char url[1100];
  char cors[256];
} fc_job_t;
static void *frame_check_thread(void *arg) {
  fc_job_t *j = arg;
  char cur[1100], next[2048], why[160], json[400], ew[200];
  int hops, conn, req, status = 0, verdict = -1;
  why[0] = 0;
  snprintf(cur, sizeof(cur), "%s", j->url);
  if (update_http_init() == 0) {
    pthread_mutex_lock(&g_upd_mu);
    for (hops = 0; hops < 5; hops++) {
      char *hdrs = 0;
      size_t hsz = 0;
      conn = sceHttpCreateConnectionWithURL(g_upd_tmpl, cur, 0);
      if (conn < 0)
        break;
      req = sceHttpCreateRequestWithURL(conn, 0, cur, 0);
      if (req < 0) {
        sceHttpDeleteConnection(conn);
        break;
      }
      if (sceHttpSendRequest(req, 0, 0) < 0 || sceHttpGetStatusCode(req, &status) < 0) {
        sceHttpDeleteRequest(req);
        sceHttpDeleteConnection(conn);
        break;
      }
      if (status < 100) {
        sceHttpDeleteRequest(req);
        sceHttpDeleteConnection(conn);
        break;
      }
      if (status >= 300 && status < 400 && !extract_location(req, cur, next, sizeof(next)) &&
          strlen(next) < sizeof(cur)) {
        sceHttpDeleteRequest(req);
        sceHttpDeleteConnection(conn);
        snprintf(cur, sizeof(cur), "%s", next);
        continue;
      }
      if (!sceHttpGetAllResponseHeaders(req, &hdrs, &hsz) && hdrs && hsz)
        verdict = frame_blocked_by(hdrs, hsz, why, sizeof(why));
      else
        verdict = 0;
      sceHttpDeleteRequest(req);
      sceHttpDeleteConnection(conn);
      break;
    }
    pthread_mutex_unlock(&g_upd_mu);
  }
  json_escape_name(why, ew, sizeof(ew));
  snprintf(json, sizeof(json), "{\"ok\":true,\"frame\":\"%s\",\"status\":%d,\"why\":\"%s\"}",
           verdict < 0 ? "unknown" : verdict ? "blocked" : "ok", status, ew);
  send_code(j->c, 200, "application/json", j->cors, json, strlen(json));
  close(j->c);
  free(j);
  return NULL;
}
/* returns 1 when the socket went to the worker */
static int handle_frame_check(int c, const char *qs) {
  fc_job_t *j;
  pthread_t th;
  j = calloc(1, sizeof(*j));
  if (!j) {
    fs_reply_bad(c, 500, "no memory");
    return 0;
  }
  qget(qs, "url", j->url, sizeof(j->url));
  if (!web_url_ok(j->url)) {
    free(j);
    fs_reply_bad(c, 400, "bad url");
    return 0;
  }
  j->c = c;
  snprintf(j->cors, sizeof(j->cors), "%s", g_cors);
  if (pthread_create(&th, NULL, frame_check_thread, j)) {
    free(j);
    fs_reply_bad(c, 500, "busy");
    return 0;
  }
  pthread_detach(th);
  return 1;
}

/* GET /web-save?url=&name=  (X-ELFL) the browser's "Save to Downloads" for a
 * direct file link: https only, payload/archive extensions only, never
 * overwrites, same downloader (TLS, 64 MB cap, redirects) and the same
 * Downloads folder rules as catalog files; one at a time. */
static volatile int g_websave_busy;
typedef struct {
  int c;
  char url[1100], dest[512], name[200], cors[256];
} ws_job_t;
static int web_save_ext_ok(const char *n) {
  static const char *ok[] = {".elf", ".bin", ".self", ".prx", ".sprx", ".zip", ".7z", ".rar",
                             ".tar", ".gz", ".pkg", ".lua", ".js", ".json", ".txt", 0};
  const char *dot = strrchr(n, '.');
  int i;
  if (!dot)
    return 0;
  for (i = 0; ok[i]; i++)
    if (!strcasecmp(dot, ok[i]))
      return 1;
  return 0;
}
static void *web_save_thread(void *arg) {
  ws_job_t *j = arg;
  uint8_t *buf = 0;
  size_t blen = 0;
  char json[600], hex[65], en[400], err[200];
  struct stat st;
  int code = 200;
  if (https_download_url(j->url, &buf, &blen) || !buf) {
    json_escape_name(g_upd_err[0] ? g_upd_err : "download failed", err, sizeof(err));
    evlog("error", "browser save %s failed: %s", j->name, g_upd_err[0] ? g_upd_err : "download failed");
    snprintf(json, sizeof(json), "{\"ok\":false,\"message\":\"%s\"}", err);
    code = 502;
  } else if (!blen) {
    snprintf(json, sizeof(json), "{\"ok\":false,\"message\":\"empty file\"}");
    code = 502;
  } else if (!stat(j->dest, &st)) {
    snprintf(json, sizeof(json), "{\"ok\":false,\"message\":\"exists\"}");
    code = 409;
  } else if (write_atomic_under_data(j->dest, buf, blen)) {
    snprintf(json, sizeof(json), "{\"ok\":false,\"message\":\"write failed\"}");
    code = 500;
  } else {
    sha256_hex(buf, blen, hex);
    write_sha256_sidecar(j->dest, hex);
    json_escape_name(j->name, en, sizeof(en));
    evlog("update", "saved %s from the browser (%zu bytes)", j->name, blen);
    snprintf(json, sizeof(json), "{\"ok\":true,\"name\":\"%s\",\"bytes\":%zu,\"sha256\":\"%s\"}", en, blen, hex);
  }
  free(buf);
  send_code(j->c, code, "application/json", j->cors, json, strlen(json));
  close(j->c);
  free(j);
  g_websave_busy = 0;
  return NULL;
}
static int handle_web_save(int c, const char *qs) {
  ws_job_t *j;
  pthread_t th;
  struct stat st;
  char *e;
  j = calloc(1, sizeof(*j));
  if (!j) {
    fs_reply_bad(c, 500, "no memory");
    return 0;
  }
  qget(qs, "url", j->url, sizeof(j->url));
  qget(qs, "name", j->name, sizeof(j->name));
  if (!web_url_ok(j->url) || strncmp(j->url, "https://", 8)) {
    free(j);
    fs_reply_bad(c, 400, "https link needed");
    return 0;
  }
  if (!j->name[0]) {
    const char *b = strrchr(j->url + 8, '/');
    snprintf(j->name, sizeof(j->name), "%s", b ? b + 1 : "");
    for (e = j->name; *e; e++)
      if (*e == '?' || *e == '#') {
        *e = 0;
        break;
      }
  }
  if (mirror_disk(j->name, j->dest, sizeof(j->dest)) || strchr(j->name, '/') || j->name[0] == '.') {
    free(j);
    fs_reply_bad(c, 400, "bad name");
    return 0;
  }
  if (!web_save_ext_ok(j->name)) {
    free(j);
    fs_reply_bad(c, 415, "not a payload or archive");
    return 0;
  }
  if (!stat(j->dest, &st)) {
    free(j);
    fs_reply_bad(c, 409, "exists");
    return 0;
  }
  if (g_websave_busy) {
    free(j);
    fs_reply_bad(c, 409, "busy");
    return 0;
  }
  g_websave_busy = 1;
  j->c = c;
  snprintf(j->cors, sizeof(j->cors), "%s", g_cors);
  if (pthread_create(&th, NULL, web_save_thread, j)) {
    g_websave_busy = 0;
    free(j);
    fs_reply_bad(c, 500, "busy");
    return 0;
  }
  pthread_detach(th);
  return 1;
}

/* ---- Backup & Restore ----
 * GET  /backup/inventory          files in Downloads with size + sha256
 * GET  /backup/list               places that can hold a backup and what is there
 * POST /backup/save?dir=&local=   body = backup JSON; writes
 *      <dir>/elf-launcher-backup/backup.json and copies the listed Downloads
 *      files (uploads that are not from a catalog) into .../files/.
 * Restore uses /fs/read (JSON), /update (catalog files, sha checked) and
 * /save_path (local files). Same X-ELFL / path rules as everything else. */
#define BACKUP_SUB "elf-launcher-backup"
#define BACKUP_MAX (256 * 1024)
static int sha256_file_hex(const char *path, char out[65]) {
  uint8_t h[32], b[16384];
  sha256_ctx ctx;
  ssize_t r;
  int fd = open(path, O_RDONLY), i;
  if (fd < 0)
    return -1;
  sha256_init(&ctx);
  while ((r = read(fd, b, sizeof(b))) > 0)
    sha256_update(&ctx, b, (size_t)r);
  close(fd);
  if (r < 0)
    return -1;
  sha256_final(&ctx, h);
  for (i = 0; i < 32; i++)
    snprintf(out + i * 2, 3, "%02x", h[i]);
  return 0;
}

static int backup_skip_name(const char *n) {
  size_t l = strlen(n);
  if (n[0] == '.' || !l)
    return 1;
  if (l > 7 && !strcmp(n + l - 7, ".sha256"))
    return 1;
  if (l > 5 && !strcmp(n + l - 5, ".part"))
    return 1;
  return !has_payload_ext(n);
}

static void handle_backup_inventory(int c) {
  DIR *d = opendir(MIRROR_DIR);
  struct dirent *de;
  size_t cap = 96 * 1024, pos = 0;
  char *out = malloc(cap), path[600], sum[65], e[300];
  struct stat st;
  int n = 0;
  if (!out) {
    if (d)
      closedir(d);
    fs_reply_bad(c, 500, "no memory");
    return;
  }
  pos += (size_t)snprintf(out, cap, "{\"ok\":true,\"files\":[");
  while (d && (de = readdir(d)) != NULL && pos + 600 < cap) {
    if (backup_skip_name(de->d_name))
      continue;
    if (snprintf(path, sizeof(path), "%s/%s", MIRROR_DIR, de->d_name) >= (int)sizeof(path) ||
        lstat(path, &st) || !S_ISREG(st.st_mode))
      continue;
    if (read_sha256_sidecar(path, sum, sizeof(sum)) && sha256_file_hex(path, sum))
      continue;
    json_escape_name(de->d_name, e, sizeof(e));
    pos += (size_t)snprintf(out + pos, cap - pos,
                            "%s{\"name\":\"%s\",\"size\":%lld,\"sha256\":\"%s\"}",
                            n++ ? "," : "", e, (long long)st.st_size, sum);
  }
  if (d)
    closedir(d);
  snprintf(out + pos, cap - pos, "]}");
  send_json_code(c, 200, out);
  free(out);
}

static void handle_backup_list(int c) {
  static const char *places[] = {"/mnt/usb0", "/mnt/usb1", "/mnt/usb2", "/mnt/usb3",
                                 "/mnt/ext0", "/mnt/ext1", "/data", 0};
  char out[2400], f[300];
  size_t pos = 0;
  int i, n = 0;
  struct stat st;
  pos += (size_t)snprintf(out, sizeof(out), "{\"ok\":true,\"places\":[");
  for (i = 0; places[i]; i++) {
    int has;
    if (stat(places[i], &st) || !S_ISDIR(st.st_mode))
      continue;
    snprintf(f, sizeof(f), "%s/%s/backup.json", places[i], BACKUP_SUB);
    has = !stat(f, &st) && S_ISREG(st.st_mode);
    pos += (size_t)snprintf(out + pos, sizeof(out) - pos,
                            "%s{\"root\":\"%s\",\"dir\":\"%s/%s\",\"backup\":%s,\"mtime\":%lld,\"size\":%lld}",
                            n++ ? "," : "", places[i], places[i], BACKUP_SUB,
                            has ? "true" : "false", has ? (long long)st.st_mtime : 0LL,
                            has ? (long long)st.st_size : 0LL);
  }
  snprintf(out + pos, sizeof(out) - pos, "]}");
  send_json_code(c, 200, out);
}

static int copy_plain(const char *src, const char *dst) {
  char tmp[700], b[16384];
  int in, out, ok = 1;
  ssize_t r;
  snprintf(tmp, sizeof(tmp), "%s.part", dst);
  in = open(src, O_RDONLY);
  if (in < 0)
    return -1;
  out = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (out < 0) {
    close(in);
    return -1;
  }
  while (ok && (r = read(in, b, sizeof(b))) > 0)
    ok = write(out, b, (size_t)r) == r;
  if (r < 0)
    ok = 0;
  close(in);
  if (close(out))
    ok = 0;
  if (!ok || rename(tmp, dst)) {
    unlink(tmp);
    return -1;
  }
  return 0;
}

static void handle_backup_save(int c, const char *qs, const char *req, size_t hlen,
                               size_t got, const char *hdrs, const char *hend) {
  char in[1024], dir[PATH_MAX], bdir[PATH_MAX + 32], fdir[PATH_MAX + 40], dst[PATH_MAX + 300],
      src[600], names[4096], *body, *s, *e;
  size_t have, need;
  int nfiles = 0, fd, okw;
  if (fs_qget(qs, "dir", in, sizeof(in))) {
    fs_reply_bad(c, 400, "no folder");
    return;
  }
  if (fs_check(c, in, dir, sizeof(dir), 1))
    return;
  if (!fs_dest_ok(dir)) {
    fs_reply_bad(c, 403, "protected");
    return;
  }
  names[0] = 0;
  if (qget(qs, "local", names, sizeof(names)) == 0 && strlen(names) + 1 >= sizeof(names)) {
    fs_reply_bad(c, 413, "too many files");
    return;
  }
  {
    const char *why;
    body = read_body(c, req, hlen, got, hdrs, hend, BACKUP_MAX, &have, &why);
    if (!body) {
      fs_reply_bad(c, 400, why);
      return;
    }
    need = have;
  }
  body[have] = 0;
  if (have != need || body[0] != '{' || !strstr(body, "\"elf-launcher-backup\"")) {
    free(body);
    fs_reply_bad(c, 400, "not a backup");
    return;
  }
  snprintf(bdir, sizeof(bdir), "%s/%s", strcmp(dir, "/") ? dir : "", BACKUP_SUB);
  snprintf(fdir, sizeof(fdir), "%s/files", bdir);
  mkdir(bdir, 0777);
  mkdir(fdir, 0777);
  /* local files first, the JSON last (a backup.json means the set is whole) */
  for (s = names; s && *s; s = e ? e + 1 : 0) {
    char clean[128];
    e = strchr(s, ',');
    if (e)
      *e = 0;
    if (!*s)
      continue;
    if (sanitize_upload_name(s, clean, sizeof(clean)) || strcmp(clean, s)) {
      free(body);
      fs_reply_bad(c, 400, "bad file name");
      return;
    }
    snprintf(src, sizeof(src), "%s/%s", MIRROR_DIR, clean);
    snprintf(dst, sizeof(dst), "%s/%s", fdir, clean);
    if (copy_plain(src, dst)) {
      char m[200];
      snprintf(m, sizeof(m), "cannot copy %s: %s", clean, strerror(errno));
      free(body);
      fs_reply_bad(c, 500, m);
      return;
    }
    nfiles++;
  }
  snprintf(dst, sizeof(dst), "%s/backup.json.part", bdir);
  fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  okw = fd >= 0 && write(fd, body, have) == (ssize_t)have;
  if (fd >= 0 && close(fd))
    okw = 0;
  free(body);
  snprintf(src, sizeof(src), "%s/backup.json", bdir);
  if (!okw || rename(dst, src)) {
    char m[160];
    unlink(dst);
    snprintf(m, sizeof(m), "cannot write backup: %s", strerror(errno));
    fs_reply_bad(c, 500, m);
    return;
  }
  {
    char out[800], ep[700];
    json_escape_name(src, ep, sizeof(ep));
    snprintf(out, sizeof(out), "{\"ok\":true,\"path\":\"%s\",\"files\":%d}", ep, nfiles);
    evlog("info", "backup saved to %s (%d files)", src, nfiles);
    send_json_code(c, 200, out);
  }
}

static void handle_fs_status(int c) {
  char out[900], cur[300], err[260];
  json_escape_name(fsj.cur, cur, sizeof(cur));
  json_escape_name(fsj.err, err, sizeof(err));
  snprintf(out, sizeof(out),
           "{\"ok\":true,\"busy\":%s,\"seq\":%u,\"op\":\"%s\",\"result\":%s,"
           "\"total\":%lld,\"done\":%lld,\"items\":%d,\"items_done\":%d,"
           "\"current\":\"%s\",\"error\":\"%s\"}",
           fs_busy ? "true" : "false", fsj.seq,
           fsj.op == FS_COPY ? "copy" : fsj.op == FS_MOVE ? "move" : fsj.op == FS_DELETE ? "delete" : "",
           fs_busy ? "null" : (fsj.ok ? "true" : "false"), fsj.total, fsj.done,
           fsj.items, fsj.items_done, cur, err);
  send_json_code(c, 200, out);
}

static void handle_fs(int c, const char *op, const char *qs) {
  static char list[REQ_MAX];
  char in[1024], real[PATH_MAX], dreal[PATH_MAX], name[256], tgt[PATH_MAX];
  struct stat st;
  int kind;
  if (!strcmp(op, "mkdir") || !strcmp(op, "rename")) {
    int mk = op[0] == 'm';
    if (fs_qget(qs, "path", in, sizeof(in)) || fs_qget(qs, "name", name, sizeof(name)) ||
        !fs_name_ok(name)) {
      fs_reply_bad(c, 400, "bad name");
      return;
    }
    if (fs_check(c, in, real, sizeof(real), 1))
      return;
    if (mk ? !fs_dest_ok(real) : !fs_touchable(real)) {
      fs_reply_bad(c, 403, "protected");
      return;
    }
    if (mk) {
      if (stat(real, &st) || !S_ISDIR(st.st_mode)) {
        fs_reply_bad(c, 404, "folder missing");
        return;
      }
      snprintf(tgt, sizeof(tgt), "%s/%s", real, name);
    } else {
      char *sl = strrchr(real, '/');
      snprintf(tgt, sizeof(tgt), "%.*s/%s", (int)(sl - real), real, name);
    }
    if (!lstat(tgt, &st)) {
      fs_reply_bad(c, 409, "already exists");
      return;
    }
    if (fs_busy) {
      fs_reply_bad(c, 409, "busy");
      return;
    }
    if (mk ? mkdir(tgt, 0777) : rename(real, tgt)) {
      char m[160];
      snprintf(m, sizeof(m), "%s failed: %s", mk ? "mkdir" : "rename", strerror(errno));
      fs_reply_bad(c, 500, m);
      return;
    }
    {
      char e[700], out[800];
      json_escape_name(tgt, e, sizeof(e));
      snprintf(out, sizeof(out), "{\"ok\":true,\"path\":\"%s\"}", e);
      send_json_code(c, 200, out);
    }
    return;
  }
  kind = !strcmp(op, "copy") ? FS_COPY : !strcmp(op, "move") ? FS_MOVE : !strcmp(op, "delete") ? FS_DELETE : 0;
  if (!kind) {
    fs_reply_bad(c, 404, "unknown operation");
    return;
  }
  if (fs_qget(qs, "paths", list, sizeof(list)) || !list[0]) {
    fs_reply_bad(c, 400, "no paths");
    return;
  }
  dreal[0] = 0;
  if (kind != FS_DELETE) {
    if (fs_qget(qs, "dest", in, sizeof(in))) {
      fs_reply_bad(c, 400, "no destination");
      return;
    }
    if (fs_check(c, in, dreal, sizeof(dreal), 1))
      return;
    if (stat(dreal, &st) || !S_ISDIR(st.st_mode)) {
      fs_reply_bad(c, 404, "destination is not a folder");
      return;
    }
    if (!fs_dest_ok(dreal)) {
      fs_reply_bad(c, 403, "protected");
      return;
    }
  }
  if (!__sync_bool_compare_and_swap(&fs_busy, 0, 1)) {
    fs_reply_bad(c, 409, "busy");
    return;
  }
  {
    int n = 0;
    char *s = list, *e;
    static char srcs[FS_MAX_SRC][PATH_MAX];
    while (s && *s) {
      e = strchr(s, '\n');
      if (e)
        *e = 0;
      if (*s) {
        if (n >= FS_MAX_SRC) {
          __sync_lock_release(&fs_busy);
          fs_reply_bad(c, 413, "too many items at once");
          return;
        }
        if (fs_check(c, s, srcs[n], PATH_MAX, kind == FS_COPY ? 2 : 1)) {
          __sync_lock_release(&fs_busy);
          return;
        }
        if (!fs_touchable(srcs[n]) && kind != FS_COPY) {
          __sync_lock_release(&fs_busy);
          fs_reply_bad(c, 403, "protected");
          return;
        }
        if (kind != FS_DELETE &&
            (!strcmp(dreal, srcs[n]) || under_root(dreal, srcs[n]))) {
          __sync_lock_release(&fs_busy);
          fs_reply_bad(c, 400, "cannot put a folder inside itself");
          return;
        }
        n++;
      }
      s = e ? e + 1 : 0;
    }
    if (!n) {
      __sync_lock_release(&fs_busy);
      fs_reply_bad(c, 400, "no paths");
      return;
    }
    memcpy(fsj.src, srcs, sizeof(srcs));
    fsj.n = n;
  }
  snprintf(fsj.dest, sizeof(fsj.dest), "%s", dreal);
  fsj.op = kind;
  fsj.ok = 0;
  fsj.cancel = 0;
  fsj.total = fsj.done = 0;
  fsj.items = fsj.items_done = 0;
  fsj.err[0] = fsj.cur[0] = 0;
  fsj.seq++;
  {
    pthread_t th;
    if (pthread_create(&th, NULL, fs_job_thread, NULL)) {
      __sync_lock_release(&fs_busy);
      fs_reply_bad(c, 500, "cannot start");
      return;
    }
    pthread_detach(th);
  }
  {
    char out[200];
    snprintf(out, sizeof(out), "{\"ok\":true,\"started\":true,\"seq\":%u,\"count\":%d}",
             fsj.seq, fsj.n);
    send_json_code(c, 200, out);
  }
}

static void send_result_json(int c, int code, const char *cors, int ok,
                             const char *message, long long bytes,
                             const char *sha, int pid, const char *emsg,
                             const char *name) {
  char json[1024], m[200], e[300], n[200];
  if (name && name[0]) {
    if (!ok)
      evlog("error", "%s: %s", name, message ? message : "failed");
    else if (message && !strcmp(message, "saved"))
      evlog("info", "%s saved to Downloads", name);
    else if (pid > 0)
      evlog("load", "%s (pid %d)", name, pid);
    else
      evlog("load", "%s", name);
  }
  json_escape_name(message ? message : "", m, sizeof(m));
  json_escape_name(emsg ? emsg : "", e, sizeof(e));
  json_escape_name(name ? name : "", n, sizeof(n));
  snprintf(json, sizeof(json),
           "{\"ok\":%s,\"message\":\"%s\",\"name\":\"%s\",\"bytes\":%lld,"
           "\"sha256\":\"%s\",\"pid\":%d,\"elfldr_msg\":\"%s\"}",
           ok ? "true" : "false", m, n, bytes, sha ? sha : "", pid, e);
  send_code(c, code, "application/json", cors, json, strlen(json));
}

/* POST /run_path?path=/mnt/usb0/x.elf[&args=] runs the file where it is. */
/* Copy a PS5 file into Downloaded (mirror), optionally marking it
 * AutoPayload. AutoPayload only runs files from Downloaded. */
static void handle_save_path(int c, const char *qs) {
  char in[1024], real[PATH_MAX], name[128], dest[512], part[540], hex[65], flag[8];
  uint8_t hash[32];
  sha256_ctx ctx;
  struct stat st;
  const char *base;
  char *buf;
  int rc, src, fd, ok = 1, i, autoadd;
  ssize_t r;
  in[0] = flag[0] = 0;
  if (qs) {
    qget(qs, "path", in, sizeof(in));
    qget(qs, "auto", flag, sizeof(flag));
  }
  autoadd = flag[0] == '1';
  rc = path_readable(in, real, sizeof(real));
  if (rc) {
    send_result_json(c, rc == -2 ? 404 : 403, g_cors, 0,
                     rc == -2 ? "file missing" : "path not allowed", 0, "", 0,
                     "", "");
    return;
  }
  base = strrchr(real, '/');
  base = base ? base + 1 : real;
  if (stat(real, &st) || !S_ISREG(st.st_mode) || sanitize_upload_name(base, name, sizeof(name))) {
    send_result_json(c, 404, g_cors, 0, "file missing", 0, "", 0, "", base);
    return;
  }
  if (st.st_size > UPLOAD_MAX) {
    send_result_json(c, 413, g_cors, 0, "file too big", 0, "", 0, "", name);
    return;
  }
  if (!file_is_payload(real)) {
    send_result_json(c, 415, g_cors, 0, "not an ELF or SELF", 0, "", 0, "", name);
    return;
  }
  mkdir("/data/elf-launcher", 0755);
  mkdir(MIRROR_DIR, 0755);
  snprintf(dest, sizeof(dest), "%s/%s", MIRROR_DIR, name);
  if (strcmp(dest, real)) {
    snprintf(part, sizeof(part), "%s.part", dest);
    src = open(real, O_RDONLY);
    fd = src < 0 ? -1 : open(part, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    buf = malloc(65536);
    if (src < 0 || fd < 0 || !buf) {
      if (src >= 0)
        close(src);
      if (fd >= 0) {
        close(fd);
        unlink(part);
      }
      free(buf);
      send_result_json(c, 500, g_cors, 0, "cannot copy file", 0, "", 0, "", name);
      return;
    }
    sha256_init(&ctx);
    while ((r = read(src, buf, 65536)) > 0) {
      if (write(fd, buf, (size_t)r) != r) {
        ok = 0;
        break;
      }
      sha256_update(&ctx, (const uint8_t *)buf, (size_t)r);
    }
    if (r < 0)
      ok = 0;
    free(buf);
    close(src);
    close(fd);
    if (!ok || rename(part, dest)) {
      unlink(part);
      send_result_json(c, 500, g_cors, 0, "cannot copy file", 0, "", 0, "", name);
      return;
    }
    sha256_final(&ctx, hash);
    for (i = 0; i < 32; i++)
      snprintf(hex + i * 2, 3, "%02x", hash[i]);
    hex[64] = 0;
    write_sha256_sidecar(dest, hex);
  }
  if (autoadd && strlen(name) > 4 && !strcasecmp(name + strlen(name) - 4, ".elf"))
    add_to_auto_list(name);
  send_result_json(c, 200, g_cors, 1, "saved", (long long)st.st_size, "", 0, "", name);
}

/* Verify mirror/elf-launcher.elf against its .sha256 (written by /update),
 * stamp the hand-off and give it to elfldr. The new instance kills us. */
#define SELF_ELF_PATH MIRROR_DIR "/elf-launcher.elf"
static void handle_self_update(int c) {
  char want[80], got[65], emsg[256], body[200];
  uint8_t hash[32];
  sha256_ctx ctx;
  struct stat st;
  FILE *f;
  char *buf;
  int fd, rc, i;
  ssize_t r;
  want[0] = 0;
  if (stat(SELF_ELF_PATH, &st) || !S_ISREG(st.st_mode) || !file_is_payload(SELF_ELF_PATH)) {
    send_json_code(c, 404, "{\"ok\":false,\"message\":\"new launcher not downloaded\"}");
    return;
  }
  f = fopen(SELF_ELF_PATH ".sha256", "r");
  if (f) {
    if (!fgets(want, sizeof(want), f))
      want[0] = 0;
    fclose(f);
  }
  want[64] = 0;
  fd = open(SELF_ELF_PATH, O_RDONLY);
  buf = malloc(65536);
  if (fd < 0 || !buf || strlen(want) != 64) {
    if (fd >= 0)
      close(fd);
    free(buf);
    send_json_code(c, 409, "{\"ok\":false,\"message\":\"no verified download\"}");
    return;
  }
  sha256_init(&ctx);
  while ((r = read(fd, buf, 65536)) > 0)
    sha256_update(&ctx, (const uint8_t *)buf, (size_t)r);
  close(fd);
  free(buf);
  sha256_final(&ctx, hash);
  for (i = 0; i < 32; i++)
    snprintf(got + i * 2, 3, "%02x", hash[i]);
  got[64] = 0;
  if (strcasecmp(got, want)) {
    send_json_code(c, 409, "{\"ok\":false,\"message\":\"sha256 mismatch\"}");
    return;
  }
  if (!send_lock_try()) {
    send_json_code(c, 409, "{\"ok\":false,\"message\":\"busy\"}");
    return;
  }
  write_takeover_mark();
  rc = elfldr_send_path(SELF_ELF_PATH, "", emsg, sizeof(emsg), 1500);
  send_unlock();
  if (rc != SEND_OK) {
    unlink(TAKEOVER_MARK_PATH);
    snprintf(body, sizeof(body), "{\"ok\":false,\"message\":\"%s\"}", send_err_text(rc));
    send_json_code(c, send_http_code(rc), body);
    return;
  }
  snprintf(body, sizeof(body), "{\"ok\":true,\"old_pid\":%d,\"sha256\":\"%s\"}", (int)getpid(), got);
  send_json_code(c, 200, body);
}

static void handle_run_path(int c, const char *qs) {
  char in[1024], real[PATH_MAX], args[512], emsg[256];
  const char *base;
  struct stat st;
  int rc, pid = 0;
  in[0] = args[0] = 0;
  if (qs) {
    qget(qs, "path", in, sizeof(in));
    qget(qs, "args", args, sizeof(args));
  }
  rc = path_readable(in, real, sizeof(real));
  if (rc) {
    send_result_json(c, rc == -2 ? 404 : 403, g_cors, 0,
                     rc == -2 ? "file missing" : "path not allowed", 0, "", 0,
                     "", "");
    return;
  }
  base = strrchr(real, '/');
  base = base ? base + 1 : real;
  if (stat(real, &st) || !S_ISREG(st.st_mode)) {
    send_result_json(c, 404, g_cors, 0, "file missing", 0, "", 0, "", base);
    return;
  }
  if (!file_is_payload(real)) {
    send_result_json(c, 415, g_cors, 0, "not an ELF or SELF", 0, "", 0, "",
                     base);
    return;
  }
  if (!send_lock_try()) {
    send_result_json(c, 409, g_cors, 0, "busy", 0, "", 0, "", base);
    return;
  }
  rc = elfldr_send_path(real, args, emsg, sizeof(emsg), 1500);
  send_unlock();
  if (rc == SEND_OK)
    pid = find_pid_by_name(base);
  send_result_json(c, send_http_code(rc), g_cors, rc == SEND_OK,
                   send_err_text(rc), (long long)st.st_size, "", pid, emsg,
                   base);
}

/* ---- POST /upload?name=&save=&auto=&args= (raw body) ---- */
typedef struct upload_job {
  int c;
  size_t clen;
  size_t pre_n;
  char *pre;
  char qs[1600];
  char cors[256];
} upload_job_t;

static void remove_old_uploads(void) {
  DIR *d = opendir(UPLOAD_DIR);
  struct dirent *de;
  char p[512];
  if (!d)
    return;
  while ((de = readdir(d)) != NULL) {
    if (de->d_name[0] == '.')
      continue;
    if (snprintf(p, sizeof(p), "%s/%s", UPLOAD_DIR, de->d_name) <
        (int)sizeof(p))
      unlink(p);
  }
  closedir(d);
}

static void run_upload(upload_job_t *j) {
  char rawname[256], name[128], flag[8], args[512], dest[512], part[540];
  char hex[65], emsg[256];
  uint8_t hash[32];
  unsigned char head[4];
  size_t have = 0, headn = 0;
  sha256_ctx ctx;
  int save = 0, autoadd = 0, fd, rc, pid = 0, i;
  char *buf = 0;
  rawname[0] = args[0] = 0;
  qget(j->qs, "name", rawname, sizeof(rawname));
  qget(j->qs, "args", args, sizeof(args));
  flag[0] = 0;
  qget(j->qs, "save", flag, sizeof(flag));
  save = flag[0] == '1';
  flag[0] = 0;
  qget(j->qs, "auto", flag, sizeof(flag));
  autoadd = flag[0] == '1';
  if (autoadd)
    save = 1; /* AutoPayload reads from Downloaded */
  if (sanitize_upload_name(rawname, name, sizeof(name))) {
    send_result_json(j->c, 400, j->cors, 0, "bad name", 0, "", 0, "", "");
    return;
  }
  mkdir("/data", 0755);
  mkdir("/data/elf-launcher", 0755);
  mkdir(save ? MIRROR_DIR : UPLOAD_DIR, 0755);
  if (!save)
    remove_old_uploads();
  snprintf(dest, sizeof(dest), "%s/%s", save ? MIRROR_DIR : UPLOAD_DIR, name);
  snprintf(part, sizeof(part), "%s.part", dest);
  fd = open(part, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    send_result_json(j->c, 500, j->cors, 0, "cannot write file", 0, "", 0, "",
                     name);
    return;
  }
  buf = malloc(65536);
  if (!buf) {
    close(fd);
    unlink(part);
    send_result_json(j->c, 500, j->cors, 0, "no memory", 0, "", 0, "", name);
    return;
  }
  sha256_init(&ctx);
  sock_timeouts(j->c, 20000, 10000);
  while (have < j->clen) {
    const char *src;
    size_t n;
    if (j->pre_n) {
      src = j->pre;
      n = j->pre_n > j->clen ? j->clen : j->pre_n;
      j->pre_n = 0;
    } else {
      ssize_t r = recv(j->c, buf, (j->clen - have) > 65536 ? 65536 : (j->clen - have), 0);
      if (r <= 0)
        break;
      src = buf;
      n = (size_t)r;
    }
    for (i = 0; headn < 4 && (size_t)i < n; i++)
      head[headn++] = (unsigned char)src[i];
    if (headn == 4 && have < 4 && !magic_ok(head)) {
      have = 0;
      break;
    }
    if (write(fd, src, n) != (ssize_t)n) {
      have = 0;
      break;
    }
    sha256_update(&ctx, (const uint8_t *)src, n);
    have += n;
  }
  free(buf);
  close(fd);
  if (have != j->clen || headn < 4 || !magic_ok(head)) {
    unlink(part);
    if (headn == 4 && !magic_ok(head)) {
      send_result_json(j->c, 415, j->cors, 0, "not an ELF or SELF", 0, "", 0,
                       "", name);
      drain_briefly(j->c);
    }
    else
      send_result_json(j->c, 400, j->cors, 0, "upload incomplete", (long long)have,
                       "", 0, "", name);
    return;
  }
  if (rename(part, dest)) {
    unlink(part);
    send_result_json(j->c, 500, j->cors, 0, "cannot save file", 0, "", 0, "",
                     name);
    return;
  }
  sha256_final(&ctx, hash);
  for (i = 0; i < 32; i++)
    snprintf(hex + i * 2, 3, "%02x", hash[i]);
  hex[64] = 0;
  if (save)
    write_sha256_sidecar(dest, hex);
  if (autoadd && !strcasecmp(name + strlen(name) - 4, ".elf"))
    add_to_auto_list(name);
  flag[0] = 0;
  qget(j->qs, "run", flag, sizeof(flag));
  if (save && flag[0] == '0') {
    /* Save only (e.g. "add to AutoPayload" from a multi-select). */
    send_result_json(j->c, 200, j->cors, 1, "saved", (long long)have, hex, 0,
                     "", name);
    return;
  }
  rc = elfldr_send_path(dest, args, emsg, sizeof(emsg), 1500);
  if (rc == SEND_OK)
    pid = find_pid_by_name(name);
  send_result_json(j->c, send_http_code(rc), j->cors, rc == SEND_OK,
                   send_err_text(rc), (long long)have, hex, pid, emsg, name);
}

static void *upload_thread(void *arg) {
  upload_job_t *j = arg;
  run_upload(j);
  close(j->c);
  send_unlock();
  free(j->pre);
  free(j);
  return NULL;
}

/* Called on the main loop; the body is read on a worker so the UI keeps
 * answering while a phone uploads. The send lock is held until it is done. */
/* Returns 1 when the socket was handed off (caller must not close it). */
static int handle_upload(int c, const char *qs, const char *hdrs,
                         const char *hend, const char *body, size_t body_n) {
  char v[64];
  long long clen;
  upload_job_t *j;
  pthread_t th;
  if (hdr_get(hdrs, hend, "Content-Length", v, sizeof(v)) || !v[0]) {
    send_json_code(c, 411, "{\"ok\":false,\"message\":\"length required\"}");
    return 0;
  }
  clen = atoll(v);
  if (clen < 4) {
    send_json_code(c, 400, "{\"ok\":false,\"message\":\"empty file\"}");
    return 0;
  }
  if (clen > UPLOAD_MAX) {
    send_json_code(c, 413, "{\"ok\":false,\"message\":\"file too big (64 MB max)\"}");
    return 0;
  }
  if (!send_lock_try()) {
    send_json_code(c, 409, "{\"ok\":false,\"message\":\"busy\"}");
    return 0;
  }
  j = malloc(sizeof(*j));
  if (j)
    memset(j, 0, sizeof(*j));
  if (!j) {
    send_unlock();
    send_json_code(c, 500, "{\"ok\":false,\"message\":\"no memory\"}");
    return 0;
  }
  j->c = c;
  j->clen = (size_t)clen;
  snprintf(j->qs, sizeof(j->qs), "%s", qs ? qs : "");
  snprintf(j->cors, sizeof(j->cors), "%s", g_cors);
  if (body_n) {
    j->pre = malloc(body_n);
    if (j->pre) {
      memcpy(j->pre, body, body_n);
      j->pre_n = body_n;
    }
  }
  if (!hdr_get(hdrs, hend, "Expect", v, sizeof(v)) &&
      !strcasecmp(v, "100-continue"))
    send_all(c, "HTTP/1.1 100 Continue\r\n\r\n", 25);
  if (pthread_create(&th, NULL, upload_thread, j)) {
    upload_thread(j); /* closes c and unlocks */
    return 1;
  }
  pthread_detach(th);
  return 1;
}

static void run_headless_auto(void) {
  char listed[AUTO_MAX][AUTO_NAME_MAX];
  char use[AUTO_MAX][AUTO_NAME_MAX];
  int n, e, i, sent = 0, skipped = 0;
  char path[512];
  char emsg[256];
  int from_list, rc;

  sleep(1);
  n = read_auto_list(listed, AUTO_MAX);
  e = existing_names(listed, n, use, AUTO_MAX);
  from_list = n > 0;
  if (e == 0) {
    scan_page_storage();
    e = existing_names(storage_names, storage_name_count, use, AUTO_MAX);
    if (e > 0) {
      char joined[AUTO_MAX * AUTO_NAME_MAX];
      size_t used = 0;
      joined[0] = 0;
      for (i = 0; i < e; i++) {
        size_t L = strlen(use[i]);
        if (used && used + 1 < sizeof(joined))
          joined[used++] = ',';
        if (used + L >= sizeof(joined))
          break;
        memcpy(joined + used, use[i], L);
        used += L;
        joined[used] = 0;
      }
      write_auto_names(joined);
      from_list = 1;
    }
  }

  /* Empty queue: do not consume the one-shot; allow a later mark/download.
   * Stay silent when nothing is marked Auto (avoid a notify every jailbreak). */
  if (e == 0) {
    if (from_list && n > 0) {
      for (i = 0; i < n; i++)
        notify("Skipped %s (file missing)", listed[i]);
    }
    return;
  }

  /* Notify listed names that are missing on disk before sending the rest. */
  if (from_list && n > e) {
    for (i = 0; i < n; i++) {
      int found = 0, j;
      for (j = 0; j < e; j++) {
        if (!strcmp(listed[i], use[j])) {
          found = 1;
          break;
        }
      }
      if (!found) {
        notify("Skipped %s (file missing)", listed[i]);
        skipped++;
      }
    }
  }

  /* Non-empty runnable queue: consume one-shot for this jailbreak/boot. */
  write_boot_auto_done();

  for (i = 0; i < e; i++) {
    if (auto_disk_path(use[i], path, sizeof(path))) {
      notify("Skipped %s (file missing)", use[i]);
      evlog("error", "AutoPayload skipped %s (file missing)", use[i]);
      skipped++;
      continue;
    }
    send_lock_wait();
    rc = elfldr_send_path(path, "", emsg, sizeof(emsg), 1000);
    send_unlock();
    if (rc == SEND_OK) {
      sent++;
      evlog("load", "AutoPayload loaded %s", use[i]);
      usleep(300000);
    } else {
      evlog("error", "AutoPayload skipped %s (%s)", use[i], send_err_text(rc));
      notify("Skipped %s (%s)", use[i], send_err_text(rc));
      skipped++;
    }
  }
  if (sent > 0)
    notify("AutoPayload %d", sent);
  else if (skipped > 0)
    notify("AutoPayload skipped");
}

static void *headless_auto_thread(void *arg) {
  (void)arg;
  run_headless_auto();
  return NULL;
}

static void start_headless_auto(void) {
  pthread_t th;
  if (pthread_create(&th, NULL, headless_auto_thread, NULL) == 0)
    pthread_detach(th);
}

static void serve(void) {
  int s = -1, tries;
  static char req[REQ_MAX + 1];
  char path[512];
  int from_wkal = consume_wkal_mark();
  int takeover = consume_takeover_mark();
  int want_open, accept_fail = 0;
  int port_busy = http_port_open();
  int already_up = port_busy && http_alive();

  /* Clear AutoPayload one-shot on every fresh :1000 bind (new ELF after JB).
   * from-wkal used to be required, but Hybrid often sends only elf-launcher.elf
   * (no mark), so Open browser skipped Auto while Leave closed still ran. */
  if (takeover) {
    /* Update hand-off: replace the old server, keep the AutoPayload one-shot. */
    if (port_busy) {
      kill_other_elf_launchers();
      usleep(300000);
    }
    already_up = 1; /* long bind retry below */
  } else if (!already_up)
    clear_boot_auto_done();
  else if (from_wkal)
    clear_boot_auto_done();

  /* Leave closed / open preference applies with or without the from-wkal
   * mark. If :1000 is already up and we are not forcing a manual takeover
   * (open preference + no mark), keep the live server and only open the
   * page or run AutoPayload. Manual open still takes over :1000. */
  if (already_up && !takeover) {
    if (from_wkal || !wk_wants_open()) {
      /* Open browser still needs disk Auto (auto.list); page localStorage often empty. */
      if (!boot_auto_done())
        run_headless_auto();
      if (wk_wants_open())
        launch_browser_now();
      return;
    }
    kill_other_elf_launchers();
    usleep(300000);
  } else if (port_busy && !takeover) {
    /* Something holds :1000 but does not answer: replace it. */
    kill_other_elf_launchers();
    usleep(300000);
    already_up = 1;
  }

  want_open = takeover ? 0 : wk_wants_open();

  /* The old server may need a moment to release :1000 after the kill. */
  for (tries = already_up ? 20 : 3; tries > 0; tries--) {
    if (bind_http(&s) == 0)
      break;
    s = -1;
    usleep(250000);
  }
  /* The server must stay up whatever the open setting is. If the port is
   * still taken, clear stale launchers and keep trying for a minute; only
   * give up when another healthy launcher answers on :1000. */
  for (tries = 0; s < 0 && tries < 30; tries++) {
    if (!takeover && http_alive())
      break;
    if (tries == 0 || tries == 10)
      kill_other_elf_launchers();
    sleep(2);
    if (bind_http(&s))
      s = -1;
  }
  if (s < 0) {
    if (want_open)
      start_fresh_browser();
    if (!home_icon_up_to_date()) {
      int err = install_home_icon();
      if (err)
        notify("Home icon install failed: 0x%08X", (unsigned)err);
      else
        notify("Home icon installed");
    }
    return;
  }
  if (listen(s, 16) < 0) {
    close(s);
    if (!home_icon_up_to_date()) {
      int err = install_home_icon();
      if (err)
        notify("Home icon install failed: 0x%08X", (unsigned)err);
      else
        notify("Home icon installed");
    }
    return;
  }
  puts("listening");
  /* Open browser: same disk Auto as Leave closed (do not gate on boot-auto-done
   * here — flag was just cleared on fresh bind). Then open the WebView. */
  if (takeover)
    notify("ELF Launcher updated");
  else if (want_open) {
    run_headless_auto();
    start_fresh_browser();
  } else
    start_headless_auto();
  /* Bind of :1000. Install the home icon once in the background. */
  start_home_icon_install_async(0);
  for (;;) {
    int c = accept(s, 0, 0);
    char *p;
    char *sp;
    char *q;
    char *hdrs, *hend;
    char origin[200];
    size_t hlen = 0, got = 0;
    int is_post = 0, rr;
    if (c < 0) {
      /* Rest mode can leave the listen socket dead: no busy loop, and
       * rebuild it after repeated failures so :1000 keeps answering. */
      if (errno != EINTR && ++accept_fail >= 20) {
        int ns = -1;
        close(s);
        while (bind_http(&ns) || listen(ns, 16) < 0) {
          if (ns >= 0)
            close(ns);
          ns = -1;
          sleep(1);
        }
        s = ns;
        accept_fail = 0;
      } else
        usleep(50000);
      continue;
    }
    accept_fail = 0;
    rr = read_request(c, req, sizeof(req), &hlen, &got);
    if (rr) {
      g_cors[0] = 0;
      if (rr == -2) {
        send_text_code(c, 431, "request headers too large");
        drain_briefly(c);
      }
      close(c);
      continue;
    }
    hdrs = strstr(req, "\r\n");
    hdrs = hdrs ? hdrs + 2 : req + hlen;
    hend = req + hlen;
    origin[0] = 0;
    hdr_get(hdrs, hend, "Origin", origin, sizeof(origin));
    set_cors_for(c, origin);
    if (!strncmp(req, "OPTIONS ", 8)) {
      char opt[512];
      int h;
      if (g_cors[0])
        h = snprintf(opt, sizeof(opt),
                     "HTTP/1.1 204 No Content\r\n%s"
                     "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                     "Access-Control-Allow-Headers: X-ELFL, Content-Type\r\n"
                     "Access-Control-Max-Age: 600\r\n"
                     "Content-Length: 0\r\nConnection: close\r\n\r\n",
                     g_cors);
      else
        h = snprintf(opt, sizeof(opt),
                     "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n"
                     "Connection: close\r\n\r\n");
      if (h > 0)
        send_all(c, opt, (size_t)h);
      close(c);
      continue;
    }
    if (!strncmp(req, "GET ", 4))
      p = req + 4;
    else if (!strncmp(req, "POST ", 5)) {
      p = req + 5;
      is_post = 1;
    } else {
      close(c);
      continue;
    }
    sp = strchr(p, ' ');
    if (sp)
      *sp = 0;
    else {
      sp = strstr(p, "\r\n");
      if (sp)
        *sp = 0;
    }
    q = strchr(p, '?');
    if (q)
      *q = 0;
    if (*p == '/')
      p++;
    if (!strncmp(p, "files/", 6))
      p += 6;
    if (is_mutating(p, q ? q + 1 : 0) && !req_trusted(c, hdrs, hend) &&
        /* WK Autoloader pings this with a no-cors GET that may carry neither
           Origin nor Referer. It only re-runs the user's own AutoPayload. */
        !(!strcmp(p, "trigger-auto") && !has_origin_or_referer(hdrs, hend))) {
      send_json_code(c, 403, "{\"ok\":false,\"message\":\"forbidden\"}");
      close(c);
      continue;
    }
    if (!strcmp(p, "ip")) {
      char ip[48], json[96];
      if (lan_ip(ip, sizeof(ip)))
        conn_local_ip(c, ip, sizeof(ip));
      snprintf(json, sizeof(json), "{\"ok\":true,\"ip\":\"%s\"}", ip);
      send_json_code(c, 200, json);
      close(c);
      continue;
    }
    if (!strcmp(p, "browse")) {
      handle_browse(c, q ? q + 1 : 0);
      close(c);
      continue;
    }
    if (!strcmp(p, "fs/status")) {
      handle_fs_status(c);
      close(c);
      continue;
    }
    if (!strcmp(p, "open-browser")) {
      handle_open_browser(c, q ? q + 1 : "");
      close(c);
      continue;
    }
    if (!strcmp(p, "web-save")) {
      if (!handle_web_save(c, q ? q + 1 : ""))
        close(c);
      continue;
    }
    if (!strcmp(p, "frame-check")) {
      if (!handle_frame_check(c, q ? q + 1 : ""))
        close(c);
      continue;
    }
    if (!strcmp(p, "events")) {
      handle_events(c);
      close(c);
      continue;
    }
    if (!strcmp(p, "events/add")) {
      handle_events_add(c, q ? q + 1 : "");
      close(c);
      continue;
    }
    if (!strcmp(p, "sysinfo")) {
      handle_sysinfo(c);
      close(c);
      continue;
    }
    if (!strcmp(p, "profiles")) {
      handle_profiles(c, is_post, req, hlen, got, hdrs, hend);
      if (is_post)
        drain_briefly(c);
      close(c);
      continue;
    }
    if (!strcmp(p, "fs/upload")) {
      if (!is_post) {
        send_json_code(c, 405, "{\"ok\":false,\"message\":\"use POST\"}");
        close(c);
      } else if (!handle_fs_upload(c, q ? q + 1 : "", req, hlen, got, hdrs, hend)) {
        drain_briefly(c); /* refused before the body: let the client read why */
        close(c);
      }
      continue;
    }
    if (!strcmp(p, "backup/inventory")) {
      handle_backup_inventory(c);
      close(c);
      continue;
    }
    if (!strcmp(p, "backup/list")) {
      handle_backup_list(c);
      close(c);
      continue;
    }
    if (!strcmp(p, "backup/save")) {
      if (!is_post)
        send_json_code(c, 405, "{\"ok\":false,\"message\":\"use POST\"}");
      else
        handle_backup_save(c, q ? q + 1 : "", req, hlen, got, hdrs, hend);
      drain_briefly(c);
      close(c);
      continue;
    }
    if (!strcmp(p, "icon")) {
      handle_icon(c, q ? q + 1 : "");
      close(c);
      continue;
    }
    if (!strcmp(p, "fs/read")) {
      handle_fs_read(c, q ? q + 1 : "");
      close(c);
      continue;
    }
    if (!strcmp(p, "fs/cancel")) {
      if (fs_busy)
        fsj.cancel = 1;
      send_json_code(c, 200, "{\"ok\":true}");
      close(c);
      continue;
    }
    if (!strncmp(p, "fs/", 3)) {
      if (!is_post)
        send_json_code(c, 405, "{\"ok\":false,\"message\":\"use POST\"}");
      else
        handle_fs(c, p + 3, q ? q + 1 : "");
      close(c);
      continue;
    }
    if (!strcmp(p, "version")) {
      char vb[160];
      snprintf(vb, sizeof(vb), "{\"ok\":true,\"pid\":%d,\"build\":\"%s %s\",\"icon\":\"%s\"}",
               (int)getpid(), __DATE__, __TIME__, HOME_ICON_VERSION);
      send_json(c, 1, vb, strlen(vb));
      close(c);
      continue;
    }
    if (!strcmp(p, "self_update")) {
      if (!is_post)
        send_json_code(c, 405, "{\"ok\":false,\"message\":\"use POST\"}");
      else
        handle_self_update(c);
      close(c);
      continue;
    }
    if (!strcmp(p, "save_path")) {
      if (!is_post)
        send_json_code(c, 405, "{\"ok\":false,\"message\":\"use POST\"}");
      else
        handle_save_path(c, q ? q + 1 : 0);
      close(c);
      continue;
    }
    if (!strcmp(p, "run_path")) {
      if (!is_post)
        send_json_code(c, 405, "{\"ok\":false,\"message\":\"use POST\"}");
      else
        handle_run_path(c, q ? q + 1 : 0);
      close(c);
      continue;
    }
    if (!strcmp(p, "upload")) {
      if (!is_post)
        send_json_code(c, 405, "{\"ok\":false,\"message\":\"use POST\"}");
      else if (handle_upload(c, q ? q + 1 : 0, hdrs, hend, req + hlen,
                             got - hlen))
        continue;
      close(c);
      continue;
    }
    /* Connect-only check. Do not write bytes to :9021 (raw elfldr is one-shot). */
    if (!strcmp(p, "elfldr-ready")) {
      int fd = connect_port(9021);
      char json[48];
      int ready = 0;
      if (fd >= 0) {
        ready = 1;
        close(fd);
      }
      snprintf(json, sizeof(json), "{\"ok\":true,\"ready\":%s}",
               ready ? "true" : "false");
      send_json(c, 1, json, strlen(json));
      close(c);
      continue;
    }
    /* Hybrid open-only / WK: clear one-shot and run disk Auto without re-sending ELF. */
    if (!strcmp(p, "trigger-auto")) {
      const char *ok = "{\"ok\":true,\"triggered\":true}";
      clear_boot_auto_done();
      start_headless_auto();
      send_json(c, 1, ok, strlen(ok));
      close(c);
      continue;
    }
    /* Persist AutoPayload one-shot across page/app opens until reboot or fresh JB. */
    if (!strcmp(p, "boot-auto")) {
      char flag[8];
      char json[64];
      int done;
      flag[0] = 0;
      if (q)
        qget(q + 1, "done", flag, sizeof(flag));
      if (flag[0] == '1') {
        if (write_boot_auto_done()) {
          const char *err = "{\"ok\":false,\"message\":\"could not save\"}";
          send_json(c, 0, err, strlen(err));
          close(c);
          continue;
        }
        done = 1;
      } else if (flag[0] == '0') {
        clear_boot_auto_done();
        done = 0;
      } else {
        done = boot_auto_done();
      }
      snprintf(json, sizeof(json), "{\"ok\":true,\"done\":%s}",
               done ? "true" : "false");
      send_json(c, 1, json, strlen(json));
      close(c);
      continue;
    }
    if (!strcmp(p, "processes_list")) {
      char *jbuf = malloc(512 * 1024);
      size_t jlen;
      if (!jbuf) {
        const char *oom = "{\"processes\":[]}";
        send_json(c, 1, oom, strlen(oom));
        close(c);
        continue;
      }
      jlen = process_list_json(jbuf, 512 * 1024);
      send_json(c, 1, jbuf, jlen);
      free(jbuf);
      close(c);
      continue;
    }
    if (!strcmp(p, "browser-pref")) {
      char flag[8];
      char json[96];
      int open_flag;
      flag[0] = 0;
      if (q)
        qget(q + 1, "open", flag, sizeof(flag));
      if (flag[0] == '1' || flag[0] == '0') {
        open_flag = flag[0] == '1';
        if (write_open_after_jb(open_flag)) {
          const char *err = "{\"ok\":false,\"message\":\"could not save\"}";
          send_json(c, 0, err, strlen(err));
          close(c);
          continue;
        }
      } else {
        open_flag = open_after_jb();
      }
      snprintf(json, sizeof(json), "{\"ok\":true,\"open\":%s}",
               open_flag ? "true" : "false");
      send_json(c, 1, json, strlen(json));
      close(c);
      continue;
    }
    if (!strcmp(p, "auto-list")) {
      char names[1800];
      char json[96];
      int count;
      names[0] = 0;
      if (q && strstr(q + 1, "names=")) {
        qget(q + 1, "names", names, sizeof(names));
        count = write_auto_names(names);
        if (count < 0) {
          const char *err = "{\"ok\":false,\"message\":\"could not save\"}";
          send_json(c, 0, err, strlen(err));
          close(c);
          continue;
        }
      } else {
        char listed[AUTO_MAX][AUTO_NAME_MAX];
        count = read_auto_list(listed, AUTO_MAX);
      }
      snprintf(json, sizeof(json), "{\"ok\":true,\"count\":%d}", count);
      send_json(c, 1, json, strlen(json));
      close(c);
      continue;
    }
    if (!strcmp(p, "install-home")) {
      const char *resp =
          "{\"ok\":true,\"message\":\"Installing home icon\"}";
      clear_home_icon_ver();
      home_icon_install_started = 0;
      start_home_icon_install_async(1);
      send_json(c, 1, resp, strlen(resp));
      close(c);
      continue;
    }

    if (!strcmp(p, "file_meta")) {
      char rel[400], dest[512], json[400], sum[65];
      struct stat st;
      int exists = 0;
      long long sz = 0;
      rel[0] = 0;
      sum[0] = 0;
      if (q)
        qget(q + 1, "path", rel, sizeof(rel));
      if (mirror_disk(rel, dest, sizeof(dest))) {
        const char *err = "{\"ok\":false,\"message\":\"bad path\"}";
        send_json(c, 0, err, strlen(err));
        close(c);
        continue;
      }
      if (stat(dest, &st) == 0 && S_ISREG(st.st_mode)) {
        exists = 1;
        sz = (long long)st.st_size;
        if (read_sha256_sidecar(dest, sum, sizeof(sum)) != 0)
          sum[0] = 0;
      }
      snprintf(json, sizeof(json),
               "{\"ok\":true,\"exists\":%s,\"size\":%lld,\"checksum\":\"%s\",\"path\":\"%s\"}",
               exists ? "true" : "false", sz, sum, DATA_ROOT);
      /* path prefix only in JSON; rel validated already */
      send_json(c, 1, json, strlen(json));
      close(c);
      continue;
    }
    if (!strcmp(p, "file_delete")) {
      char rel[400], dest[512], side[540], json[160];
      rel[0] = 0;
      if (q)
        qget(q + 1, "path", rel, sizeof(rel));
      if (mirror_disk(rel, dest, sizeof(dest))) {
        const char *err = "{\"ok\":false,\"message\":\"bad path\"}";
        send_json(c, 0, err, strlen(err));
        close(c);
        continue;
      }
      if (unlink(dest) != 0 && errno != ENOENT) {
        snprintf(json, sizeof(json),
                 "{\"ok\":false,\"message\":\"delete failed\"}");
        send_json(c, 0, json, strlen(json));
        close(c);
        continue;
      }
      if (snprintf(side, sizeof(side), "%s.sha256", dest) < (int)sizeof(side))
        unlink(side);
      snprintf(json, sizeof(json), "{\"ok\":true,\"message\":\"deleted\"}");
      send_json(c, 1, json, strlen(json));
      close(c);
      continue;
    }
    if (!strcmp(p, "catalog_fetch")) {
      char url[1400];
      uint8_t *buf = 0;
      size_t blen = 0;
      int drc;
      url[0] = 0;
      if (q)
        qget(q + 1, "url", url, sizeof(url));
      if (!url[0] || !url_allowed_for_update(url)) {
        const char *err = "{\"ok\":false,\"message\":\"url not allowed\"}";
        send_json(c, 0, err, strlen(err));
        close(c);
        continue;
      }
      drc = https_download_url(url, &buf, &blen);
      if (drc || !buf || blen == 0 || blen > 2 * 1024 * 1024) {
        char err[240];
        snprintf(err, sizeof(err),
                 "{\"ok\":false,\"message\":\"%s\"}",
                 g_upd_err[0] ? g_upd_err : "catalog download failed");
        send_json(c, 0, err, strlen(err));
        free(buf);
        close(c);
        continue;
      }
      /* Serve raw JSON body for the page to parse. */
      {
        char hdr[448];
        int h = snprintf(hdr, sizeof(hdr),
                         "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                         "Content-Length: %zu\r\n%sConnection: close\r\n\r\n",
                         blen, g_cors);
        if (h > 0)
          send_all(c, hdr, (size_t)h);
        send_all(c, buf, blen);
      }
      free(buf);
      close(c);
      continue;
    }
    if (!strcmp(p, "update")) {
      char rel[400], dest[512], url[1400], sha_exp[80], size_s[32], got_hex[65];
      char json[320];
      uint8_t *buf = 0;
      size_t blen = 0;
      long long expect_size = -1;
      int drc, wrc;
      rel[0] = url[0] = sha_exp[0] = size_s[0] = 0;
      if (q) {
        qget(q + 1, "path", rel, sizeof(rel));
        qget(q + 1, "url", url, sizeof(url));
        qget(q + 1, "sha256", sha_exp, sizeof(sha_exp));
        qget(q + 1, "size", size_s, sizeof(size_s));
      }
      if (size_s[0])
        expect_size = atoll(size_s);
      if (mirror_disk(rel, dest, sizeof(dest))) {
        const char *err = "{\"ok\":false,\"message\":\"bad path\"}";
        send_json(c, 0, err, strlen(err));
        close(c);
        continue;
      }
      if (!url_allowed_for_update(url)) {
        const char *err = "{\"ok\":false,\"message\":\"url not allowed\"}";
        send_json(c, 0, err, strlen(err));
        close(c);
        continue;
      }
      if (!sha_exp[0] || strlen(sha_exp) != 64) {
        const char *err = "{\"ok\":false,\"message\":\"sha256 required\"}";
        send_json(c, 0, err, strlen(err));
        close(c);
        continue;
      }
      drc = https_download_url(url, &buf, &blen);
      if (drc || !buf) {
        evlog("error", "download %s failed: %s", rel, g_upd_err[0] ? g_upd_err : "download failed");
        snprintf(json, sizeof(json),
                 "{\"ok\":false,\"message\":\"%s\"}",
                 g_upd_err[0] ? g_upd_err : "download failed");
        send_json(c, 0, json, strlen(json));
        free(buf);
        close(c);
        continue;
      }
      if (expect_size >= 0 && (long long)blen != expect_size) {
        snprintf(json, sizeof(json),
                 "{\"ok\":false,\"message\":\"size mismatch\"}");
        send_json(c, 0, json, strlen(json));
        free(buf);
        close(c);
        continue;
      }
      sha256_hex(buf, blen, got_hex);
      if (strcasecmp(got_hex, sha_exp)) {
        evlog("error", "download %s: sha256 mismatch", rel);
        snprintf(json, sizeof(json),
                 "{\"ok\":false,\"message\":\"sha256 mismatch got %.12s\"}",
                 got_hex);
        send_json(c, 0, json, strlen(json));
        free(buf);
        close(c);
        continue;
      }
      wrc = write_atomic_under_data(dest, buf, blen);
      free(buf);
      if (wrc) {
        snprintf(json, sizeof(json),
                 "{\"ok\":false,\"message\":\"write failed (%d)\"}", wrc);
        send_json(c, 0, json, strlen(json));
        close(c);
        continue;
      }
      /* Keep .sha256 sidecar so Update can compare without rehashing. */
      write_sha256_sidecar(dest, got_hex);
      evlog("update", "downloaded %s (sha %.12s)", rel, got_hex);
      snprintf(json, sizeof(json),
               "{\"ok\":true,\"bytes\":%zu,\"sha256\":\"%s\"}", blen,
               got_hex);
      send_json(c, 1, json, strlen(json));
      close(c);
      continue;
    }

    if (!strcmp(p, "process_kill")) {
      const char *pid_s = 0;
      int pid = 0;
      int rc;
      char json_resp[128];
      if (q) {
        pid_s = strstr(q + 1, "pid=");
        if (pid_s)
          pid = atoi(pid_s + 4);
      }
      if (!pid_s || pid <= 0) {
        const char *err = "{\"ok\":false,\"message\":\"Missing pid\"}";
        send_json(c, 0, err, strlen(err));
        close(c);
        continue;
      }
      {
        char pname[64];
        int code = 200;
        const char *m = "Killed";
        if (pid == (int)getpid()) {
          rc = -1; code = 403; m = "This is the launcher itself";
        } else if (process_name_for_pid(pid, pname, sizeof(pname)) || !pname[0]) {
          rc = -1; code = 404; m = "Not running";
        } else if (is_protected_proc_name(pname)) {
          rc = -1; code = 403; m = "Protected process";
        } else {
          int64_t until;
          rc = process_kill_pid(pid);
          if (rc) {
            code = 500; m = "Failed to kill";
          } else {
            /* SIGKILL is async: wait until it is really gone */
            until = mono_ms() + 2000;
            while (mono_ms() < until &&
                   !process_name_for_pid(pid, pname, sizeof(pname)) && pname[0])
              usleep(100000);
            if (!process_name_for_pid(pid, pname, sizeof(pname)) && pname[0]) {
              rc = -1; code = 500; m = "Still running after kill";
            }
          }
        }
        snprintf(json_resp, sizeof(json_resp),
                 "{\"ok\":%s,\"pid\":%d,\"message\":\"%s\"}",
                 rc == 0 ? "true" : "false", pid, m);
        if (rc == 0)
          evlog("kill", "stopped %s (pid %d)", pname[0] ? pname : "?", pid);
        else
          evlog("error", "stop pid %d failed: %s", pid, m);
        send_json_code(c, code, json_resp);
      }
      close(c);
      continue;
    }
    if (!strcmp(p, "notify")) {
      const char *m = q ? strstr(q + 1, "msg=") : 0;
      char text[160];
      size_t k = 0;
      static const unsigned char gif[] = {
        0x47,0x49,0x46,0x38,0x39,0x61,0x01,0x00,0x01,0x00,0x80,0x00,0x00,
        0x00,0x00,0x00,0xff,0xff,0xff,0x21,0xf9,0x04,0x01,0x00,0x00,0x00,
        0x00,0x2c,0x00,0x00,0x00,0x00,0x01,0x00,0x01,0x00,0x00,0x02,0x02,
        0x44,0x01,0x00,0x3b
      };
      text[0] = 0;
      if (m) {
        m += 4;
        while (*m && *m != '&' && k + 1 < sizeof(text)) {
          if (*m == '%' && m[1] && m[2]) {
            unsigned int h = 0;
            sscanf(m + 1, "%2x", &h);
            text[k++] = (char)h;
            m += 3;
            continue;
          }
          if (*m == '+') { text[k++] = ' '; m++; continue; }
          text[k++] = *m++;
        }
        text[k] = 0;
      }
      if (text[0])
        notify("%s", text);
      send_blob(c, "image/gif", gif, sizeof(gif));
      close(c);
      continue;
    }
    if (!strcmp(p, "relay")) {
      const char *msg = "ok";
      const char *enc = q ? strstr(q + 1, "url=") : 0;
      char uri[1400];
      char reqline[1600];
      int fd;
      int bad = 1;
      size_t k = 0;
      if (enc) {
        enc += 4;
        while (enc[k] && enc[k] != '&' && k + 1 < sizeof(uri)) {
          uri[k] = enc[k];
          k++;
        }
        uri[k] = 0;
      } else
        uri[0] = 0;
      fd = uri[0] ? connect_port(9021) : -1;
      if (fd >= 0) {
        int nreq = snprintf(reqline, sizeof(reqline),
                            "GET /?uri=%s HTTP/1.1\r\nHost: 127.0.0.1:9021\r\n"
                            "Connection: close\r\n\r\n",
                            uri);
        if (nreq > 0 && send_all(fd, reqline, (size_t)nreq) == 0)
          bad = 0;
        close(fd);
      }
      if (bad)
        msg = "Load failed";
      {
        char hdr[192];
        int h = snprintf(hdr, sizeof(hdr),
                         "HTTP/1.1 %s\r\nContent-Type: text/plain\r\n"
                         "Content-Length: %zu\r\nConnection: close\r\n\r\n",
                         bad ? "502 Bad Gateway" : "200 OK", strlen(msg));
        if (h > 0)
          send_all(c, hdr, (size_t)h);
        send_all(c, msg, strlen(msg));
      }
      close(c);
      continue;
    }
    if (!strcmp(p, "ensure") || !strcmp(p, "run") || !strncmp(p, "load/", 5)) {
      char rel[256], mbuf[320];
      const char *msg = "ok";
      int code = 200;
      rel[0] = 0;
      if (!strncmp(p, "load/", 5))
        snprintf(rel, sizeof(rel), "%s", p + 5);
      else if (q) {
        const char *s = strstr(q + 1, "path=");
        if (s) {
          size_t k = 0;
          s += 5;
          while (*s && *s != '&' && k + 1 < sizeof(rel)) {
            if (*s == '%' && s[1] && s[2]) {
              unsigned int h = 0;
              sscanf(s + 1, "%2x", &h);
              rel[k++] = (char)h;
              s += 3;
              continue;
            }
            rel[k++] = *s++;
          }
          rel[k] = 0;
        }
      }
      if (mirror_disk(rel, path, sizeof(path))) {
        code = 400;
        msg = "bad path";
      } else if (!strcmp(p, "ensure")) {
        code = access(path, R_OK) ? 404 : 200;
        msg = code == 404 ? "file missing" : "{\"source\":\"cache\"}";
      } else if (access(path, R_OK)) {
        code = 404;
        msg = "file missing";
      } else if (!file_is_payload(path)) {
        code = 415;
        msg = "not an ELF or SELF";
      } else if (!send_lock_try()) {
        code = 409;
        msg = "busy, another payload is being sent";
      } else {
        int rc;
        char args[512], emsg[256];
        args[0] = 0;
        if (q)
          qget(q + 1, "args", args, sizeof(args));
        rc = elfldr_send_path(path, args, emsg, sizeof(emsg), 1500);
        send_unlock();
        code = send_http_code(rc);
        if (rc != SEND_OK) {
          snprintf(mbuf, sizeof(mbuf), "%s%s%s", send_err_text(rc),
                   emsg[0] ? ": " : "", emsg);
          msg = mbuf;
        }
      }
      send_text_code(c, code, msg);
      close(c);
      continue;
    }
    if (!*p || !strcmp(p, "index.html")) {
      send_blob(c, "text/html; charset=utf-8", index_html, index_html_size);
      close(c);
      on_page_open();
      continue;
    }
    if (!send_icon(c, p)) {
      close(c);
      continue;
    }
    if (local_file(p, path, sizeof(path)) || send_disk(c, path)) {
      const char *msg = "Not on the console. Copy the files to /data/elf-launcher";
      char hdr[160];
      int h = snprintf(hdr, sizeof(hdr),
                       "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n"
                       "Content-Length: %zu\r\nConnection: close\r\n\r\n",
                       strlen(msg));
      if (h > 0)
        send_all(c, hdr, (size_t)h);
      send_all(c, msg, strlen(msg));
    }
    close(c);
  }
}

static void make_data_dir(void) {
  const char *info = "Elf launcher\nPath: /data/elf-launcher/\nPut ELF files here. No folders.\n";
  FILE *f;
  mkdir("/data", 0755);
  if (mkdir("/data/elf-launcher", 0755) && errno != EEXIST) {
    notify("Could not create /data/elf-launcher");
    return;
  }
  if (mkdir("/data/elf-launcher/mirror", 0755) && errno != EEXIST)
    notify("Could not create /data/elf-launcher/mirror");
  f = fopen("/data/elf-launcher/path.txt", "w");
  if (f) {
    fputs(info, f);
    fclose(f);
  }
}

int main(void) {
  /* So re-sends can find/kill us (elfldr default name is payload.elf). */
  syscall(SYS_thr_set_name, -1, "elf-launcher");
  make_data_dir();
  ev_started = (long long)time(NULL);
  evlog("info", "launcher started");
  (void)update_http_init(); /* optional; Update uses sceHttp when available */
  serve();
  return 0;
}
