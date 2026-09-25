/* ps5-elfs-host: local HTTP UI + cache-first download + send to elfldr :9021
 * GPLv3-compatible structure inspired by ps5-payload-dev samples.
 */
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "miniz.h"

extern const unsigned char index_html[];
extern const size_t index_html_len;

int sceUserServiceInitialize(void *);
int sceUserServiceTerminate(void);
int sceSystemServiceLaunchWebBrowser(const char *uri, void *);

int sceNetInit(void);
int sceNetPoolCreate(const char *, int, int);
int sceNetPoolDestroy(int);
int sceSslInit(size_t);
int sceSslTerm(int);
int sceHttpInit(int, int, size_t);
int sceHttpTerm(int);
int sceHttpCreateTemplate(int, const char *, int, int);
int sceHttpDeleteTemplate(int);
int sceHttpsSetSslCallback(int, void *, void *);
int sceHttpSetResponseHeaderMaxSize(int, size_t);
int sceHttpSetAutoRedirect(int, int);
int sceHttpCreateConnectionWithURL(int, const char *, int);
int sceHttpDeleteConnection(int);
int sceHttpCreateRequestWithURL(int, int, const char *, uint64_t);
int sceHttpDeleteRequest(int);
int sceHttpSendRequest(int, const void *, size_t);
int sceHttpGetStatusCode(int, int *);
int sceHttpGetResponseContentLength(int, int *, uint64_t *);
int sceHttpReadData(int, void *, size_t);
int sceHttpGetAllResponseHeaders(int, char **, size_t *);
int sceHttpParseResponseHeader(const char *, size_t, const char *, const char **,
                               size_t *);
int sceHttpGetLastErrno(int, int *);

#define PORT 18182
#define ELFLDR_PORT 9021
#define MAX_HDR 8192
#define MAX_FILE (64 * 1024 * 1024)
#define CACHE_ROOT "/data/ps5-elfs/"

static const char *roots[] = {
    "/data/ps5-elfs/",
    "/mnt/usb0/ps5-elfs/",
    "/mnt/usb1/ps5-elfs/",
    "/mnt/usb2/ps5-elfs/",
    "/data/pldmgr/payloads/",
    NULL};

#include "payload_urls.inc"

/* ---- progress (polled by UI) ---- */
static pthread_mutex_t prog_mu = PTHREAD_MUTEX_INITIALIZER;
static char prog_phase[24] = "idle";
static int prog_pct = 0;
static char prog_path[256] = "";

static void progress_set(const char *phase, int pct, const char *path) {
  pthread_mutex_lock(&prog_mu);
  snprintf(prog_phase, sizeof(prog_phase), "%s", phase ? phase : "idle");
  prog_pct = pct;
  if (path)
    snprintf(prog_path, sizeof(prog_path), "%s", path);
  pthread_mutex_unlock(&prog_mu);
}

static void progress_get(char *phase, size_t psz, int *pct, char *path,
                         size_t pathsz) {
  pthread_mutex_lock(&prog_mu);
  snprintf(phase, psz, "%s", prog_phase);
  *pct = prog_pct;
  snprintf(path, pathsz, "%s", prog_path);
  pthread_mutex_unlock(&prog_mu);
}

/* ---- http helpers ---- */
static int send_all(int fd, const void *buf, size_t len) {
  const char *p = (const char *)buf;
  while (len) {
    ssize_t n = send(fd, p, len, 0);
    if (n < 0) {
      if (errno == EINTR)
        continue;
      return -1;
    }
    p += n;
    len -= (size_t)n;
  }
  return 0;
}

static int http_reply(int cfd, int code, const char *ctype, const void *body,
                      size_t blen) {
  char hdr[256];
  const char *msg = (code == 200)   ? "OK"
                    : (code == 404) ? "Not Found"
                    : (code == 400) ? "Bad Request"
                    : (code == 502) ? "Bad Gateway"
                                    : "Error";
  int n = snprintf(hdr, sizeof(hdr),
                   "HTTP/1.1 %d %s\r\n"
                   "Content-Type: %s\r\n"
                   "Content-Length: %zu\r\n"
                   "Connection: close\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Cache-Control: no-store\r\n"
                   "\r\n",
                   code, msg, ctype, blen);
  if (n < 0 || send_all(cfd, hdr, (size_t)n))
    return -1;
  if (blen && body && send_all(cfd, body, blen))
    return -1;
  return 0;
}

static int path_ok(const char *rel) {
  if (!rel || !rel[0])
    return 0;
  if (strstr(rel, ".."))
    return 0;
  if (rel[0] == '/' || rel[0] == '\\')
    return 0;
  return 1;
}

static int url_decode_path(const char *q, char *rel, size_t relsz) {
  char *p = strstr(q, "path=");
  if (!p)
    return -1;
  p += 5;
  size_t i = 0;
  while (*p && *p != '&' && i + 1 < relsz) {
    if (*p == '%') {
      unsigned v = 0;
      if (sscanf(p, "%%%2x", &v) == 1) {
        rel[i++] = (char)v;
        p += 3;
        continue;
      }
    }
    if (*p == '+') {
      rel[i++] = ' ';
      p++;
      continue;
    }
    rel[i++] = *p++;
  }
  rel[i] = 0;
  return 0;
}

static int read_file(const char *path, unsigned char **out, size_t *outlen) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return -1;
  if (fseek(f, 0, SEEK_END)) {
    fclose(f);
    return -1;
  }
  long sz = ftell(f);
  if (sz < 0 || sz > MAX_FILE) {
    fclose(f);
    return -1;
  }
  rewind(f);
  unsigned char *buf = (unsigned char *)malloc((size_t)sz);
  if (!buf) {
    fclose(f);
    return -1;
  }
  if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
    free(buf);
    fclose(f);
    return -1;
  }
  fclose(f);
  *out = buf;
  *outlen = (size_t)sz;
  return 0;
}

static int find_payload(const char *rel, unsigned char **out, size_t *outlen,
                        char *found, size_t foundsz) {
  char path[512];
  const char *base = strrchr(rel, '/');
  base = base ? base + 1 : rel;
  for (int i = 0; roots[i]; i++) {
    snprintf(path, sizeof(path), "%s%s", roots[i], rel);
    if (read_file(path, out, outlen) == 0) {
      snprintf(found, foundsz, "%s", path);
      return 0;
    }
    snprintf(path, sizeof(path), "%s%s", roots[i], base);
    if (read_file(path, out, outlen) == 0) {
      snprintf(found, foundsz, "%s", path);
      return 0;
    }
  }
  return -1;
}

static int payload_cached(const char *rel) {
  unsigned char *buf = NULL;
  size_t blen = 0;
  char found[512];
  if (find_payload(rel, &buf, &blen, found, sizeof(found)))
    return 0;
  free(buf);
  return 1;
}

static int send_to_elfldr(const unsigned char *data, size_t len) {
  int s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0)
    return -1;
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(ELFLDR_PORT);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(s);
    return -2;
  }
  int rc = send_all(s, data, len);
  close(s);
  return rc ? -3 : 0;
}

static int mkdir_p(const char *dir) {
  char tmp[512];
  size_t len = strlen(dir);
  if (len == 0 || len >= sizeof(tmp))
    return -1;
  memcpy(tmp, dir, len + 1);
  for (char *p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = 0;
      if (mkdir(tmp, 0777) && errno != EEXIST)
        return -1;
      *p = '/';
    }
  }
  if (mkdir(tmp, 0777) && errno != EEXIST)
    return -1;
  return 0;
}

static int ensure_parent_dirs(const char *filepath) {
  char dir[512];
  snprintf(dir, sizeof(dir), "%s", filepath);
  char *slash = strrchr(dir, '/');
  if (!slash)
    return 0;
  *slash = 0;
  return mkdir_p(dir);
}

static int write_file_atomic(const char *path, const unsigned char *data,
                             size_t len) {
  char tmp[540];
  if (ensure_parent_dirs(path))
    return -1;
  snprintf(tmp, sizeof(tmp), "%s.tmp", path);
  FILE *f = fopen(tmp, "wb");
  if (!f)
    return -2;
  if (fwrite(data, 1, len, f) != len) {
    fclose(f);
    unlink(tmp);
    return -3;
  }
  if (fclose(f)) {
    unlink(tmp);
    return -4;
  }
  if (rename(tmp, path)) {
    unlink(tmp);
    return -5;
  }
  return 0;
}

/* ---- HTTPS download via sceHttp (elfldr uri.c pattern) ---- */
static int g_libnetMemId = -1;
static int g_libsslCtxId = -1;
static int g_libhttpCtxId = -1;
static int g_tmplId = -1;
static pthread_mutex_t http_mu = PTHREAD_MUTEX_INITIALIZER;
static int g_last_http_status = 0;
static int g_last_http_errno = 0;
static char g_last_http_err[160] = "";

#define HTTP_MAX_REDIRECTS 5

/* Accept any cert — GitHub/CDN chains often fail stock verify on console.
 * Signature matches elfldr uri.c (ignore args). */
static int http_ssl_cb(void) {
  return 0;
}

static void http_set_err(const char *msg, int status, int errn) {
  g_last_http_status = status;
  g_last_http_errno = errn;
  if (status > 0)
    snprintf(g_last_http_err, sizeof(g_last_http_err), "%s (HTTP %d)", msg,
             status);
  else if (errn)
    snprintf(g_last_http_err, sizeof(g_last_http_err), "%s (errno %d)", msg,
             errn);
  else
    snprintf(g_last_http_err, sizeof(g_last_http_err), "%s", msg);
}

static int http_global_init(void) {
  int err;
  if (sceNetInit()) {
    perror("sceNetInit");
    return -1;
  }
  if ((g_libnetMemId = sceNetPoolCreate("ps5-elfs-host", 256 * 1024, 0)) < 0) {
    perror("sceNetPoolCreate");
    return -1;
  }
  if ((g_libsslCtxId = sceSslInit(1024 * 1024)) < 0) {
    perror("sceSslInit");
    return -1;
  }
  if ((g_libhttpCtxId =
           sceHttpInit(g_libnetMemId, g_libsslCtxId, 1024 * 1024)) < 0) {
    perror("sceHttpInit");
    return -1;
  }
  /* httpVer=2 (HTTP/1.1), proxy=1 like elfldr */
  if ((g_tmplId = sceHttpCreateTemplate(g_libhttpCtxId, "ps5-elfs-host/1.1", 2,
                                        1)) < 0) {
    perror("sceHttpCreateTemplate");
    return -1;
  }
  if ((err = sceHttpSetResponseHeaderMaxSize(g_tmplId, 0x2000)) < 0) {
    printf("sceHttpSetResponseHeaderMaxSize: 0x%x\n", err);
    return -1;
  }
  if ((err = sceHttpsSetSslCallback(g_tmplId, http_ssl_cb, 0))) {
    printf("sceHttpsSetSslCallback: 0x%x\n", err);
    return -1;
  }
  /* Best-effort; we still follow redirects manually for GitHub. */
  sceHttpSetAutoRedirect(g_tmplId, 1);
  return 0;
}

/* Resolve Location (absolute or relative) into out_url. */
static int resolve_redirect(const char *base, const char *loc, char *out,
                            size_t outsz) {
  if (!loc || !loc[0])
    return -1;
  if (!strncmp(loc, "http://", 7) || !strncmp(loc, "https://", 8)) {
    snprintf(out, outsz, "%s", loc);
    return 0;
  }
  /* protocol-relative //host/path */
  if (loc[0] == '/' && loc[1] == '/') {
    const char *scheme = "https:";
    if (!strncmp(base, "http://", 7))
      scheme = "http:";
    snprintf(out, outsz, "%s%s", scheme, loc);
    return 0;
  }
  /* absolute path on same origin */
  if (loc[0] == '/') {
    const char *p = strstr(base, "://");
    if (!p)
      return -1;
    p += 3;
    const char *slash = strchr(p, '/');
    size_t origin_len = slash ? (size_t)(slash - base) : strlen(base);
    if (origin_len + strlen(loc) + 1 > outsz)
      return -1;
    memcpy(out, base, origin_len);
    out[origin_len] = 0;
    strncat(out, loc, outsz - origin_len - 1);
    return 0;
  }
  /* relative path — append after last slash of base */
  {
    const char *slash = strrchr(base, '/');
    if (!slash)
      return -1;
    size_t prefix = (size_t)(slash - base + 1);
    if (prefix + strlen(loc) + 1 > outsz)
      return -1;
    memcpy(out, base, prefix);
    out[prefix] = 0;
    strncat(out, loc, outsz - prefix - 1);
    return 0;
  }
}

static int extract_location(int reqId, const char *base_url, char *out,
                            size_t outsz) {
  char *hdrs = NULL;
  size_t hdrsz = 0;
  const char *val = NULL;
  size_t vlen = 0;
  char locbuf[2048];

  if (sceHttpGetAllResponseHeaders(reqId, &hdrs, &hdrsz) || !hdrs || !hdrsz)
    return -1;

  if (sceHttpParseResponseHeader(hdrs, hdrsz, "Location", &val, &vlen) ||
      !val || !vlen) {
    /* case-insensitive fallback scan */
    const char *p = hdrs;
    const char *end = hdrs + hdrsz;
    val = NULL;
    while (p + 10 < end) {
      if ((p == hdrs || p[-1] == '\n') &&
          (p[0] == 'L' || p[0] == 'l') &&
          !strncasecmp(p, "Location:", 9)) {
        p += 9;
        while (p < end && (*p == ' ' || *p == '\t'))
          p++;
        const char *vstart = p;
        while (p < end && *p != '\r' && *p != '\n')
          p++;
        vlen = (size_t)(p - vstart);
        val = vstart;
        break;
      }
      p++;
    }
    if (!val || !vlen)
      return -1;
  }

  if (vlen >= sizeof(locbuf))
    vlen = sizeof(locbuf) - 1;
  memcpy(locbuf, val, vlen);
  locbuf[vlen] = 0;
  /* trim trailing CR/space */
  while (vlen && (locbuf[vlen - 1] == '\r' || locbuf[vlen - 1] == '\n' ||
                  locbuf[vlen - 1] == ' '))
    locbuf[--vlen] = 0;

  return resolve_redirect(base_url, locbuf, out, outsz);
}

/* One GET attempt; on redirect fills next_url and returns 1.
 * On success fills out/outlen and returns 0. Negative = error. */
static int https_get_once(const char *url, char *next_url, size_t next_sz,
                          unsigned char **out, size_t *outlen,
                          const char *rel_for_progress) {
  int connId = -1, reqId = -1;
  int status = -1;
  int rc = -1;
  int cl_result = 0;
  uint64_t content_len = 0;
  unsigned char *buf = NULL;
  size_t cap = 0, got = 0;

  connId = sceHttpCreateConnectionWithURL(g_tmplId, url, 0);
  if (connId < 0) {
    http_set_err("create connection failed", 0, connId);
    return -10;
  }
  /* method 0 = GET */
  reqId = sceHttpCreateRequestWithURL(connId, 0, url, 0);
  if (reqId < 0) {
    http_set_err("create request failed", 0, reqId);
    sceHttpDeleteConnection(connId);
    return -11;
  }
  if ((rc = sceHttpSendRequest(reqId, NULL, 0)) != 0) {
    int errn = 0;
    sceHttpGetLastErrno(reqId, &errn);
    http_set_err("send request failed", 0, errn ? errn : rc);
    sceHttpDeleteRequest(reqId);
    sceHttpDeleteConnection(connId);
    return -12;
  }
  if (sceHttpGetStatusCode(reqId, &status) || status < 0) {
    http_set_err("get status failed", status, 0);
    sceHttpDeleteRequest(reqId);
    sceHttpDeleteConnection(connId);
    return -13;
  }
  g_last_http_status = status;

  if (status == 301 || status == 302 || status == 303 || status == 307 ||
      status == 308) {
    if (extract_location(reqId, url, next_url, next_sz)) {
      http_set_err("redirect without Location", status, 0);
      sceHttpDeleteRequest(reqId);
      sceHttpDeleteConnection(connId);
      return -14;
    }
    sceHttpDeleteRequest(reqId);
    sceHttpDeleteConnection(connId);
    return 1; /* redirect */
  }

  if (status != 200) {
    http_set_err("unexpected status", status, 0);
    sceHttpDeleteRequest(reqId);
    sceHttpDeleteConnection(connId);
    return -15;
  }

  sceHttpGetResponseContentLength(reqId, &cl_result, &content_len);

  for (;;) {
    unsigned char chunk[16 * 1024];
    int n = sceHttpReadData(reqId, chunk, sizeof(chunk));
    if (n < 0) {
      free(buf);
      http_set_err("read data failed", status, n);
      sceHttpDeleteRequest(reqId);
      sceHttpDeleteConnection(connId);
      return -16;
    }
    if (n == 0)
      break;
    if (got + (size_t)n > MAX_FILE) {
      free(buf);
      http_set_err("file too large", status, 0);
      sceHttpDeleteRequest(reqId);
      sceHttpDeleteConnection(connId);
      return -17;
    }
    if (got + (size_t)n > cap) {
      size_t ncap = cap ? cap * 2 : 256 * 1024;
      while (ncap < got + (size_t)n)
        ncap *= 2;
      unsigned char *nb = (unsigned char *)realloc(buf, ncap);
      if (!nb) {
        free(buf);
        http_set_err("oom", status, 0);
        sceHttpDeleteRequest(reqId);
        sceHttpDeleteConnection(connId);
        return -18;
      }
      buf = nb;
      cap = ncap;
    }
    memcpy(buf + got, chunk, (size_t)n);
    got += (size_t)n;
    if (content_len > 0 && cl_result == 0) {
      int pct = (int)((got * 100) / (size_t)content_len);
      if (pct > 99)
        pct = 99;
      progress_set("download", pct, rel_for_progress);
    } else {
      int pct = (int)((got / (512 * 1024)) % 90) + 5;
      progress_set("download", pct, rel_for_progress);
    }
  }

  sceHttpDeleteRequest(reqId);
  sceHttpDeleteConnection(connId);

  if (!got) {
    free(buf);
    http_set_err("empty body", status, 0);
    return -19;
  }
  *out = buf;
  *outlen = got;
  return 0;
}

static int https_download(const char *url, unsigned char **out, size_t *outlen,
                          const char *rel_for_progress) {
  char cur[2048];
  char next[2048];
  int hops = 0;
  int rc;

  g_last_http_status = 0;
  g_last_http_errno = 0;
  g_last_http_err[0] = 0;
  progress_set("download", 0, rel_for_progress);

  if (g_tmplId < 0) {
    http_set_err("http not initialized", 0, 0);
    progress_set("idle", 0, "");
    return -1;
  }

  snprintf(cur, sizeof(cur), "%s", url);

  pthread_mutex_lock(&http_mu);
  for (;;) {
    rc = https_get_once(cur, next, sizeof(next), out, outlen, rel_for_progress);
    if (rc == 0) {
      pthread_mutex_unlock(&http_mu);
      progress_set("download", 100, rel_for_progress);
      return 0;
    }
    if (rc == 1) {
      hops++;
      if (hops > HTTP_MAX_REDIRECTS) {
        http_set_err("too many redirects", g_last_http_status, 0);
        pthread_mutex_unlock(&http_mu);
        progress_set("idle", 0, "");
        return -20;
      }
      snprintf(cur, sizeof(cur), "%s", next);
      continue;
    }
    pthread_mutex_unlock(&http_mu);
    progress_set("idle", 0, "");
    return rc;
  }
}

static int zip_extract_inner(const unsigned char *zipdata, size_t ziplen,
                             const char *inner, unsigned char **out,
                             size_t *outlen) {
  mz_zip_archive zip;
  memset(&zip, 0, sizeof(zip));
  progress_set("extract", 50, inner);
  if (!mz_zip_reader_init_mem(&zip, zipdata, ziplen, 0))
    return -20;
  int idx = mz_zip_reader_locate_file(&zip, inner, NULL, 0);
  if (idx < 0) {
    /* try basename match */
    const char *base = strrchr(inner, '/');
    base = base ? base + 1 : inner;
    for (unsigned i = 0; i < mz_zip_reader_get_num_files(&zip); i++) {
      mz_zip_archive_file_stat st;
      if (!mz_zip_reader_file_stat(&zip, i, &st))
        continue;
      const char *bn = strrchr(st.m_filename, '/');
      bn = bn ? bn + 1 : st.m_filename;
      if (!strcmp(bn, base)) {
        idx = (int)i;
        break;
      }
    }
  }
  if (idx < 0) {
    mz_zip_reader_end(&zip);
    return -21;
  }
  size_t uncomp = 0;
  void *data = mz_zip_reader_extract_to_heap(&zip, (mz_uint)idx, &uncomp, 0);
  mz_zip_reader_end(&zip);
  if (!data || uncomp == 0 || uncomp > MAX_FILE) {
    mz_free(data);
    return -22;
  }
  *out = (unsigned char *)data;
  *outlen = uncomp;
  progress_set("extract", 100, inner);
  return 0;
}

/* Ensure payload is on disk under /data/ps5-elfs/REL.
 * Returns: 0 cached, 1 downloaded, negative error.
 * errbuf optional.
 */
static int ensure_payload(const char *rel, char *errbuf, size_t errsz,
                          size_t *bytes_out, const char **source_out) {
  char dest[512];
  snprintf(dest, sizeof(dest), "%s%s", CACHE_ROOT, rel);

  if (payload_cached(rel)) {
    /* Prefer promoting to cache root if found elsewhere? not required.
     * If already under cache root or anywhere - OK. */
    unsigned char *buf = NULL;
    size_t blen = 0;
    char found[512];
    if (find_payload(rel, &buf, &blen, found, sizeof(found)) == 0) {
      if (bytes_out)
        *bytes_out = blen;
      free(buf);
    }
    if (source_out)
      *source_out = "cache";
    progress_set("idle", 100, rel);
    return 0;
  }

  const payload_url_t *u = find_payload_url(rel);
  if (!u || !u->url) {
    if (errbuf)
      snprintf(errbuf, errsz,
               "not cached and no public download URL (cache-only payload)");
    progress_set("idle", 0, "");
    return -1;
  }

  unsigned char *dl = NULL;
  size_t dllen = 0;
  int drc = https_download(u->url, &dl, &dllen, rel);
  if (drc) {
    if (errbuf) {
      if (g_last_http_err[0])
        snprintf(errbuf, errsz, "download failed (%d): %s", drc,
                 g_last_http_err);
      else if (g_last_http_status > 0)
        snprintf(errbuf, errsz, "download failed (%d) HTTP %d", drc,
                 g_last_http_status);
      else
        snprintf(errbuf, errsz, "download failed (%d) from public URL", drc);
    }
    progress_set("idle", 0, "");
    return -2;
  }

  unsigned char *payload = dl;
  size_t plen = dllen;
  int extracted = 0;
  if (u->zip_inner) {
    unsigned char *inner = NULL;
    size_t ilen = 0;
    int zrc = zip_extract_inner(dl, dllen, u->zip_inner, &inner, &ilen);
    free(dl);
    dl = NULL;
    if (zrc) {
      if (errbuf)
        snprintf(errbuf, errsz, "zip extract failed (%d)", zrc);
      progress_set("idle", 0, "");
      return -3;
    }
    payload = inner;
    plen = ilen;
    extracted = 1;
  }

  progress_set("save", 90, rel);
  if (write_file_atomic(dest, payload, plen)) {
    if (extracted)
      mz_free(payload);
    else
      free(payload);
    if (errbuf)
      snprintf(errbuf, errsz, "failed to save to %s", dest);
    progress_set("idle", 0, "");
    return -4;
  }
  if (extracted)
    mz_free(payload);
  else
    free(payload);

  if (bytes_out)
    *bytes_out = plen;
  if (source_out)
    *source_out = "download";
  progress_set("idle", 100, rel);
  return 1;
}

static void handle_client(int cfd) {
  char req[MAX_HDR];
  size_t got = 0;
  while (got < sizeof(req) - 1) {
    ssize_t n = recv(cfd, req + got, sizeof(req) - 1 - got, 0);
    if (n <= 0)
      break;
    got += (size_t)n;
    req[got] = 0;
    if (strstr(req, "\r\n\r\n"))
      break;
  }
  if (got == 0) {
    close(cfd);
    return;
  }

  char method[16] = {0}, url[1024] = {0};
  sscanf(req, "%15s %1023s", method, url);

  if (!strcmp(url, "/") || !strncmp(url, "/index", 6)) {
    http_reply(cfd, 200, "text/html; charset=utf-8", index_html,
               index_html_len);
    close(cfd);
    return;
  }

  if (!strncmp(url, "/progress", 9)) {
    char phase[24], path[256], body[384];
    int pct = 0;
    progress_get(phase, sizeof(phase), &pct, path, sizeof(path));
    int n = snprintf(body, sizeof(body),
                     "{\"phase\":\"%s\",\"pct\":%d,\"path\":\"%s\"}", phase, pct,
                     path);
    http_reply(cfd, 200, "application/json", body, (size_t)n);
    close(cfd);
    return;
  }

  if (!strncmp(url, "/ensure", 7) || !strncmp(url, "/run", 4)) {
    int is_run = !strncmp(url, "/run", 4);
    char *q = strchr(url, '?');
    char rel[512] = {0};
    if (q)
      url_decode_path(q, rel, sizeof(rel));
    if (!path_ok(rel)) {
      const char *msg = "bad path";
      http_reply(cfd, 400, "text/plain", msg, strlen(msg));
      close(cfd);
      return;
    }

    char err[256] = {0};
    size_t bytes = 0;
    const char *source = "cache";
    int erc = ensure_payload(rel, err, sizeof(err), &bytes, &source);
    if (erc < 0) {
      if (!err[0])
        snprintf(err, sizeof(err), "ensure failed");
      http_reply(cfd, 404, "text/plain", err, strlen(err));
      close(cfd);
      return;
    }

    if (!is_run) {
      char ok[320];
      int n = snprintf(ok, sizeof(ok),
                       "{\"ok\":true,\"source\":\"%s\",\"bytes\":%zu,\"path\":"
                       "\"%s%s\"}",
                       source, bytes, CACHE_ROOT, rel);
      http_reply(cfd, 200, "application/json", ok, (size_t)n);
      close(cfd);
      return;
    }

    unsigned char *buf = NULL;
    size_t blen = 0;
    char found[512];
    if (find_payload(rel, &buf, &blen, found, sizeof(found))) {
      const char *msg = "payload missing after ensure";
      http_reply(cfd, 500, "text/plain", msg, strlen(msg));
      close(cfd);
      return;
    }
    int rc = send_to_elfldr(buf, blen);
    free(buf);
    if (rc == 0) {
      char ok[700];
      snprintf(ok, sizeof(ok), "ok %s %s (%zu bytes)", source, found, blen);
      http_reply(cfd, 200, "text/plain", ok, strlen(ok));
    } else if (rc == -2) {
      const char *msg = "elfldr not listening on 127.0.0.1:9021";
      http_reply(cfd, 502, "text/plain", msg, strlen(msg));
    } else {
      const char *msg = "send to elfldr failed";
      http_reply(cfd, 500, "text/plain", msg, strlen(msg));
    }
    close(cfd);
    return;
  }

  const char *msg = "not found";
  http_reply(cfd, 404, "text/plain", msg, strlen(msg));
  close(cfd);
}

static void *http_thread(void *arg) {
  (void)arg;
  if (http_global_init()) {
    printf("warning: HTTP init failed - downloads disabled\n");
  }
  int s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) {
    perror("socket");
    return NULL;
  }
  int yes = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    close(s);
    return NULL;
  }
  if (listen(s, 8) < 0) {
    perror("listen");
    close(s);
    return NULL;
  }
  printf("ps5-elfs-host listening on %d\n", PORT);
  for (;;) {
    int c = accept(s, NULL, NULL);
    if (c < 0) {
      if (errno == EINTR)
        continue;
      break;
    }
    handle_client(c);
  }
  close(s);
  return NULL;
}

__attribute__((constructor)) static void ctor(void) {
  sceUserServiceInitialize(0);
}

__attribute__((destructor)) static void dtor(void) {
  sceUserServiceTerminate();
}

int main(void) {
  pthread_t th;
  if (pthread_create(&th, NULL, http_thread, NULL)) {
    perror("pthread_create");
    return 1;
  }
  sleep(1);
  const char *uri = "http://127.0.0.1:18182/";
  printf("launching browser %s\n", uri);
  if (sceSystemServiceLaunchWebBrowser(uri, 0)) {
    perror("sceSystemServiceLaunchWebBrowser");
  }
  pthread_join(th, NULL);
  return 0;
}
