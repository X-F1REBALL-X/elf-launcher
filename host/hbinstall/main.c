#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <ps5/kernel.h>

#define IOVEC_SIZE(x) (sizeof(x) / sizeof(struct iovec))
#define IOVEC_ENTRY(x) {x ? x : 0, x ? strlen(x) + 1 : 0}
#define TITLE_ID "BREW10090"
#define PORT 1000

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
INCASSET(ico_auto, "icons/auto.jpg");
INCASSET(ico_debug, "icons/debug.jpg");
INCASSET(ico_hen, "icons/hen.jpg");
INCASSET(ico_homebrew, "icons/homebrew.jpg");
INCASSET(ico_launcher, "icons/launcher.jpg");
INCASSET(ico_loader, "icons/loader.jpg");
INCASSET(ico_media, "icons/media.jpg");
INCASSET(ico_network, "icons/network.jpg");
INCASSET(ico_own, "icons/own.jpg");
INCASSET(ico_storage, "icons/storage.jpg");
INCASSET(ico_system, "icons/system.jpg");
INCASSET(ico_utils, "icons/utils.jpg");

static int send_blob(int c, const char *ctype, const void *body, size_t n);

static int send_icon(int c, const char *path) {
  const char *name = path;
  if (!strncmp(name, "icons/", 6))
    name += 6;
  if (!strcmp(name, "auto.jpg"))
    return send_blob(c, "image/jpeg", ico_auto, ico_auto_size);
  if (!strcmp(name, "debug.jpg"))
    return send_blob(c, "image/jpeg", ico_debug, ico_debug_size);
  if (!strcmp(name, "hen.jpg"))
    return send_blob(c, "image/jpeg", ico_hen, ico_hen_size);
  if (!strcmp(name, "homebrew.jpg"))
    return send_blob(c, "image/jpeg", ico_homebrew, ico_homebrew_size);
  if (!strcmp(name, "launcher.jpg"))
    return send_blob(c, "image/jpeg", ico_launcher, ico_launcher_size);
  if (!strcmp(name, "loader.jpg"))
    return send_blob(c, "image/jpeg", ico_loader, ico_loader_size);
  if (!strcmp(name, "media.jpg"))
    return send_blob(c, "image/jpeg", ico_media, ico_media_size);
  if (!strcmp(name, "network.jpg"))
    return send_blob(c, "image/jpeg", ico_network, ico_network_size);
  if (!strcmp(name, "own.jpg"))
    return send_blob(c, "image/jpeg", ico_own, ico_own_size);
  if (!strcmp(name, "storage.jpg"))
    return send_blob(c, "image/jpeg", ico_storage, ico_storage_size);
  if (!strcmp(name, "system.jpg"))
    return send_blob(c, "image/jpeg", ico_system, ico_system_size);
  if (!strcmp(name, "utils.jpg"))
    return send_blob(c, "image/jpeg", ico_utils, ico_utils_size);
  return -1;
}

static int remount_system_ex(void) {
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

int sceAppInstUtilInitialize(void);
int sceAppInstUtilTerminate(void);
int sceAppInstUtilAppInstallAll(void *);
int sceAppInstUtilAppUnInstall(const char *);

static int install_app(const char *title_id, const char *dir) {
  int (*fn)(const char *, const char *, void *) = 0;
  uint32_t handle;
  if (!kernel_dynlib_handle(-1, "libSceAppInstUtil.sprx", &handle))
    fn = (void *)kernel_dynlib_resolve(-1, handle, "Wudg3Xe3heE");
  if (fn)
    return fn(title_id, dir, 0);
  return sceAppInstUtilAppInstallAll(0);
}

static int install_home_icon(void) {
  int err;
  if ((err = sceAppInstUtilInitialize()))
    return err;
  sceAppInstUtilAppUnInstall(TITLE_ID);
  if (mkdir("/user/app/" TITLE_ID, 0755) && errno != EEXIST)
    return -1;
  if (mkdir("/user/app/" TITLE_ID "/sce_sys", 0755) && errno != EEXIST)
    return -1;
  if (install_file("/user/app/" TITLE_ID "/sce_sys/icon0.png", icon0_png,
                   icon0_png_size) ||
      install_file("/user/app/" TITLE_ID "/sce_sys/param.json", param_json,
                   param_json_size))
    return -1;
  err = install_app(TITLE_ID, "/user/app/");
  sceAppInstUtilTerminate();
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
  }
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

static int send_blob(int c, const char *ctype, const void *body, size_t n) {
  char hdr[192];
  int h = snprintf(hdr, sizeof(hdr),
                   "HTTP/1.1 200 OK\r\nContent-Type: %s\r\n"
                   "Content-Length: %zu\r\nCache-Control: no-store\r\n"
                   "Connection: close\r\n\r\n",
                   ctype, n);
  if (h <= 0 || send_all(c, hdr, (size_t)h))
    return -1;
  return send_all(c, body, n);
}

static int send_disk(int c, const char *path) {
  int fd = open(path, O_RDONLY);
  char buf[16384];
  char hdr[192];
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
               "Content-Length: %lld\r\nConnection: close\r\n\r\n",
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

static int push_elfldr(const char *disk) {
  int in;
  int fd;
  char buf[16384];
  ssize_t n;
  fd = connect_port(9021);
  if (fd < 0) {
    start_real_elfldr();
    sleep(1);
    fd = connect_port(9021);
  }
  if (fd < 0)
    return -1;
  in = open(disk, O_RDONLY);
  if (in < 0) {
    close(fd);
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

static void serve(void) {
  int s = socket(AF_INET, SOCK_STREAM, 0);
  int yes = 1;
  struct sockaddr_in addr;
  char req[2048];
  char path[512];
  if (s < 0)
    return;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(s);
    notify("Old launcher is still running. Reboot, then send this ELF.");
    return;
  }
  if (listen(s, 16) < 0) {
    close(s);
    return;
  }
  puts("listening");
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
      if (!strcmp(p, "ensure")) {
        bad = local_file(rel, path, sizeof(path));
        msg = bad ? "file missing" : "{\"source\":\"cache\"}";
      } else {
        bad = !rel[0] || local_file(rel, path, sizeof(path)) || push_elfldr(path);
        if (bad)
          msg = "elfldr did not take the file";
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
  f = fopen("/data/elf-launcher/path.txt", "w");
  if (f) {
    fputs(info, f);
    fclose(f);
  }
}

int main(void) {
  make_data_dir();
  install_home_icon();
  serve();
  return 0;
}
