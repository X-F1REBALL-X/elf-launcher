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
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <ps5/kernel.h>

int sceSystemServiceLaunchWebBrowser(const char *uri, void *);
#define IOVEC_SIZE(x) (sizeof(x) / sizeof(struct iovec))
#define IOVEC_ENTRY(x) {x ? x : 0, x ? strlen(x) + 1 : 0}
#define TITLE_ID "ELFL00001"
#define PORT 1000
#define HOME_ICON_VERSION "1.0.21"
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
  if (start_real_elfldr() == 0)
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
                     "%s {\"pid\":%d,\"name\":\"%s\",\"memory\":%.1f,\"is_daemon\":%s}",
                     (count > 0) ? ",\n" : "", (int)ki->ki_pid, name_e, mem_mib,
                     is_daemon ? "true" : "false");
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

static int send_json(int c, int http_ok, const char *body, size_t n) {
  char hdr[320];
  int h = snprintf(hdr, sizeof(hdr),
                   "HTTP/1.1 %s\r\nContent-Type: application/json\r\n"
                   "Content-Length: %zu\r\nCache-Control: no-store\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Connection: close\r\n\r\n",
                   http_ok ? "200 OK" : "500 Internal Server Error", n);
  if (h <= 0 || send_all(c, hdr, (size_t)h))
    return -1;
  return send_all(c, body, n);
}

static int send_blob(int c, const char *ctype, const void *body, size_t n) {
  char hdr[288];
  int h = snprintf(hdr, sizeof(hdr),
                   "HTTP/1.1 200 OK\r\nContent-Type: %s\r\n"
                   "Content-Length: %zu\r\nCache-Control: no-store\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Connection: close\r\n\r\n",
                   ctype, n);
  if (h <= 0 || send_all(c, hdr, (size_t)h))
    return -1;
  return send_all(c, body, n);
}

static int send_disk(int c, const char *path) {
  int fd = open(path, O_RDONLY);
  char buf[16384];
  char hdr[288];
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
               "Access-Control-Allow-Origin: *\r\n"
               "Connection: close\r\n\r\n",
               ctype, (long long)st.st_size);
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

/* Stream raw ELF bytes to elfldr :9021 (same approach as payload-manager). */
static int push_elfldr(const char *disk) {
  int in;
  int fd;
  char buf[16384];
  ssize_t n;
  /* Open first so a missing file never touches :9021. */
  in = open(disk, O_RDONLY);
  if (in < 0)
    return -1;
  fd = connect_port(9021);
  if (fd < 0) {
    start_real_elfldr();
    sleep(1);
    fd = connect_port(9021);
  }
  if (fd < 0) {
    close(in);
    return -1;
  }
  while ((n = read(in, buf, sizeof(buf))) > 0) {
    if (send_all(fd, buf, (size_t)n)) {
      close(in);
      close(fd);
      return -1;
    }
  }
  close(in);
  shutdown(fd, SHUT_WR);
  close(fd);
  return n < 0 ? -1 : 0;
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
    /* elfldr often leaves ki_comm as payload.elf until thr_set_name. */
    if (!strncmp(ki->ki_comm, "elf-launcher", 12) ||
        !strcmp(ki->ki_comm, "payload.elf"))
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

static void run_headless_auto(void) {
  char listed[AUTO_MAX][AUTO_NAME_MAX];
  char use[AUTO_MAX][AUTO_NAME_MAX];
  int n, e, i, sent = 0, skipped = 0;
  char path[512];
  int from_list;

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
      skipped++;
      continue;
    }
    if (push_elfldr(path) == 0) {
      sent++;
      usleep(400000);
    } else {
      notify("Skipped %s (send failed)", use[i]);
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
  int s = -1;
  char req[2048];
  char path[512];
  int from_wkal = consume_wkal_mark();
  int want_open;
  int already_up = http_port_open();

  /* Clear AutoPayload one-shot only on a real fresh JB / first bind of :1000.
   * Attaching to an already-up server (later WK page opens) must keep the flag. */
  if (from_wkal && !already_up)
    clear_boot_auto_done();

  /* Leave closed / open preference applies with or without the from-wkal
   * mark. If :1000 is already up and we are not forcing a manual takeover
   * (open preference + no mark), keep the live server and only open the
   * page or run AutoPayload. Manual open still takes over :1000. */
  if (already_up) {
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
  }

  want_open = wk_wants_open();

  if (bind_http(&s) != 0) {
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
  /* Always run Auto from disk when the one-shot is free. Open browser used to
   * rely on the WebView alone (localStorage), which often skipped the queue. */
  if (want_open) {
    if (!boot_auto_done())
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
    int n;
    if (c < 0)
      continue;
    n = recv(c, req, sizeof(req) - 1, 0);
    if (n <= 0) {
      close(c);
      continue;
    }
    req[n] = 0;
    if (!strncmp(req, "OPTIONS ", 8)) {
      const char *opt =
          "HTTP/1.1 204 No Content\r\n"
          "Access-Control-Allow-Origin: *\r\n"
          "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
          "Access-Control-Allow-Headers: *\r\n"
          "Content-Length: 0\r\n"
          "Connection: close\r\n\r\n";
      send_all(c, opt, strlen(opt));
      close(c);
      continue;
    }
    if (!strncmp(req, "GET ", 4))
      p = req + 4;
    else if (!strncmp(req, "POST ", 5))
      p = req + 5;
    else {
      close(c);
      continue;
    }
    sp = strchr(p, ' ');
    if (sp)
      *sp = 0;
    q = strchr(p, '?');
    if (q)
      *q = 0;
    if (*p == '/')
      p++;
    if (!strncmp(p, "files/", 6))
      p += 6;
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
        char hdr[192];
        int h = snprintf(hdr, sizeof(hdr),
                         "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                         "Content-Length: %zu\r\nConnection: close\r\n\r\n",
                         blen);
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
      rc = process_kill_pid(pid);
      snprintf(json_resp, sizeof(json_resp),
               "{\"ok\":%s,\"message\":\"%s\"}",
               rc == 0 ? "true" : "false",
               rc == 0 ? "Killed" : "Failed to kill");
      send_json(c, rc == 0, json_resp, strlen(json_resp));
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
      char rel[256];
      const char *msg = "ok";
      int bad = 0;
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
        bad = 1;
        msg = "bad path";
      } else if (!strcmp(p, "ensure")) {
        bad = access(path, R_OK);
        msg = bad ? "file missing" : "{\"source\":\"cache\"}";
      } else if (access(path, R_OK)) {
        bad = 1;
        msg = "file missing";
      } else if (!file_is_elf(path)) {
        bad = 1;
        msg = "not an elf";
      } else {
        /* Raw ELF bytes to elfldr :9021. No ?uri= and no toast. */
        bad = push_elfldr(path);
        msg = bad ? "elfldr did not take the file" : "ok";
      }
      {
        char hdr[192];
        int h = snprintf(hdr, sizeof(hdr),
                         "HTTP/1.1 %s\r\nContent-Type: text/plain\r\n"
                         "Content-Length: %zu\r\nConnection: close\r\n\r\n",
                         bad ? "404 Not Found" : "200 OK", strlen(msg));
        if (h > 0)
          send_all(c, hdr, (size_t)h);
        send_all(c, msg, strlen(msg));
      }
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
  (void)update_http_init(); /* optional; Update uses sceHttp when available */
  serve();
  return 0;
}
