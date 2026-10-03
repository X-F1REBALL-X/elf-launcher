/* Compact SHA-256 + hardened Update helpers for elf-launcher :1000 */
/* Included from main.c. Writes ONLY under /data/elf-launcher/. */

#define DATA_ROOT "/data/elf-launcher/"
#define UPDATE_MAX_FILE (64 * 1024 * 1024)

/* ---- SHA-256 (public-domain compact) ---- */
typedef struct {
  uint64_t bitlen;
  uint32_t state[8];
  uint8_t data[64];
  uint32_t datalen;
} sha256_ctx;

static uint32_t sha256_rotr(uint32_t x, uint32_t n) {
  return (x >> n) | (x << (32 - n));
}

static void sha256_transform(sha256_ctx *ctx, const uint8_t data[]) {
  static const uint32_t k[64] = {
      0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
      0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
      0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
      0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
      0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
      0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
      0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
      0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
  uint32_t m[64], a, b, c, d, e, f, g, h, t1, t2, i, j;
  for (i = 0, j = 0; i < 16; ++i, j += 4)
    m[i] = ((uint32_t)data[j] << 24) | ((uint32_t)data[j + 1] << 16) |
           ((uint32_t)data[j + 2] << 8) | ((uint32_t)data[j + 3]);
  for (; i < 64; ++i) {
    uint32_t s0 = sha256_rotr(m[i - 15], 7) ^ sha256_rotr(m[i - 15], 18) ^ (m[i - 15] >> 3);
    uint32_t s1 = sha256_rotr(m[i - 2], 17) ^ sha256_rotr(m[i - 2], 19) ^ (m[i - 2] >> 10);
    m[i] = m[i - 16] + s0 + m[i - 7] + s1;
  }
  a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
  e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];
  for (i = 0; i < 64; ++i) {
    uint32_t S1 = sha256_rotr(e, 6) ^ sha256_rotr(e, 11) ^ sha256_rotr(e, 25);
    uint32_t ch = (e & f) ^ ((~e) & g);
    t1 = h + S1 + ch + k[i] + m[i];
    uint32_t S0 = sha256_rotr(a, 2) ^ sha256_rotr(a, 13) ^ sha256_rotr(a, 22);
    uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    t2 = S0 + maj;
    h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
  }
  ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
  ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void sha256_init(sha256_ctx *ctx) {
  ctx->datalen = 0;
  ctx->bitlen = 0;
  ctx->state[0] = 0x6a09e667; ctx->state[1] = 0xbb67ae85;
  ctx->state[2] = 0x3c6ef372; ctx->state[3] = 0xa54ff53a;
  ctx->state[4] = 0x510e527f; ctx->state[5] = 0x9b05688c;
  ctx->state[6] = 0x1f83d9ab; ctx->state[7] = 0x5be0cd19;
}

static void sha256_update(sha256_ctx *ctx, const uint8_t data[], size_t len) {
  size_t i;
  for (i = 0; i < len; ++i) {
    ctx->data[ctx->datalen++] = data[i];
    if (ctx->datalen == 64) {
      sha256_transform(ctx, ctx->data);
      ctx->bitlen += 512;
      ctx->datalen = 0;
    }
  }
}

static void sha256_final(sha256_ctx *ctx, uint8_t hash[]) {
  uint32_t i = ctx->datalen;
  if (ctx->datalen < 56) {
    ctx->data[i++] = 0x80;
    while (i < 56) ctx->data[i++] = 0x00;
  } else {
    ctx->data[i++] = 0x80;
    while (i < 64) ctx->data[i++] = 0x00;
    sha256_transform(ctx, ctx->data);
    memset(ctx->data, 0, 56);
  }
  ctx->bitlen += (uint64_t)ctx->datalen * 8;
  ctx->data[63] = (uint8_t)ctx->bitlen;
  ctx->data[62] = (uint8_t)(ctx->bitlen >> 8);
  ctx->data[61] = (uint8_t)(ctx->bitlen >> 16);
  ctx->data[60] = (uint8_t)(ctx->bitlen >> 24);
  ctx->data[59] = (uint8_t)(ctx->bitlen >> 32);
  ctx->data[58] = (uint8_t)(ctx->bitlen >> 40);
  ctx->data[57] = (uint8_t)(ctx->bitlen >> 48);
  ctx->data[56] = (uint8_t)(ctx->bitlen >> 56);
  sha256_transform(ctx, ctx->data);
  for (i = 0; i < 4; ++i) {
    hash[i]      = (ctx->state[0] >> (24 - i * 8)) & 0xff;
    hash[i + 4]  = (ctx->state[1] >> (24 - i * 8)) & 0xff;
    hash[i + 8]  = (ctx->state[2] >> (24 - i * 8)) & 0xff;
    hash[i + 12] = (ctx->state[3] >> (24 - i * 8)) & 0xff;
    hash[i + 16] = (ctx->state[4] >> (24 - i * 8)) & 0xff;
    hash[i + 20] = (ctx->state[5] >> (24 - i * 8)) & 0xff;
    hash[i + 24] = (ctx->state[6] >> (24 - i * 8)) & 0xff;
    hash[i + 28] = (ctx->state[7] >> (24 - i * 8)) & 0xff;
  }
}

static void sha256_hex(const uint8_t *data, size_t len, char out[65]) {
  uint8_t hash[32];
  sha256_ctx ctx;
  int i;
  sha256_init(&ctx);
  sha256_update(&ctx, data, len);
  sha256_final(&ctx, hash);
  for (i = 0; i < 32; i++)
    snprintf(out + i * 2, 3, "%02x", hash[i]);
  out[64] = 0;
}

/* Path check for catalog IO is mirror_disk() in main.c:
 * /data/elf-launcher/mirror/<filename> only. */

static int mkdir_p_under_data(const char *filepath) {
  char dir[512];
  char *slash;
  char *p;
  if (strncmp(filepath, DATA_ROOT, sizeof(DATA_ROOT) - 1))
    return -1;
  snprintf(dir, sizeof(dir), "%s", filepath);
  slash = strrchr(dir, '/');
  if (!slash)
    return 0;
  *slash = 0;
  /* Walk from DATA_ROOT onward only. */
  p = dir + (sizeof(DATA_ROOT) - 1);
  if (p > dir && p[-1] == '/') {
    /* ok */
  }
  for (; *p; p++) {
    if (*p == '/') {
      *p = 0;
      if (mkdir(dir, 0755) && errno != EEXIST)
        return -1;
      *p = '/';
    }
  }
  if (mkdir(dir, 0755) && errno != EEXIST)
    return -1;
  return 0;
}

static int write_atomic_under_data(const char *final_path, const uint8_t *data,
                                  size_t len) {
  char tmp[540];
  FILE *f;
  if (strncmp(final_path, DATA_ROOT, sizeof(DATA_ROOT) - 1))
    return -1;
  if (mkdir_p_under_data(final_path))
    return -2;
  if (snprintf(tmp, sizeof(tmp), "%s.tmp", final_path) >= (int)sizeof(tmp))
    return -3;
  if (strncmp(tmp, DATA_ROOT, sizeof(DATA_ROOT) - 1))
    return -1;
  f = fopen(tmp, "wb");
  if (!f)
    return -4;
  if (fwrite(data, 1, len, f) != len) {
    fclose(f);
    unlink(tmp);
    return -5;
  }
  if (fclose(f)) {
    unlink(tmp);
    return -6;
  }
  if (rename(tmp, final_path)) {
    unlink(tmp);
    return -7;
  }
  return 0;
}


static int write_sha256_sidecar(const char *final_path, const char *hex64) {
  char side[540];
  FILE *f;
  if (!final_path || !hex64 || strlen(hex64) != 64)
    return -1;
  if (snprintf(side, sizeof(side), "%s.sha256", final_path) >= (int)sizeof(side))
    return -2;
  if (strncmp(side, DATA_ROOT, sizeof(DATA_ROOT) - 1))
    return -1;
  f = fopen(side, "wb");
  if (!f)
    return -3;
  if (fwrite(hex64, 1, 64, f) != 64) {
    fclose(f);
    unlink(side);
    return -4;
  }
  if (fclose(f)) {
    unlink(side);
    return -5;
  }
  return 0;
}

static int read_sha256_sidecar(const char *final_path, char *out, size_t outsz) {
  char side[540];
  FILE *f;
  size_t n;
  if (!final_path || !out || outsz < 65)
    return -1;
  out[0] = 0;
  if (snprintf(side, sizeof(side), "%s.sha256", final_path) >= (int)sizeof(side))
    return -2;
  f = fopen(side, "rb");
  if (!f)
    return -3;
  n = fread(out, 1, 64, f);
  fclose(f);
  if (n != 64)
    return -4;
  out[64] = 0;
  return 0;
}

/* ---- Query helpers ---- */
static int qget(const char *q, const char *key, char *out, size_t outsz) {
  char pat[64];
  const char *s;
  size_t k = 0;
  if (!q || !key || !out || outsz < 2)
    return -1;
  if (snprintf(pat, sizeof(pat), "%s=", key) >= (int)sizeof(pat))
    return -1;
  s = strstr(q, pat);
  if (!s)
    return -1;
  s += strlen(pat);
  while (*s && *s != '&' && k + 1 < outsz) {
    if (*s == '%' && s[1] && s[2]) {
      unsigned int h = 0;
      if (sscanf(s + 1, "%2x", &h) == 1) {
        out[k++] = (char)h;
        s += 3;
        continue;
      }
    }
    if (*s == '+') {
      out[k++] = ' ';
      s++;
      continue;
    }
    out[k++] = *s++;
  }
  out[k] = 0;
  return 0;
}

static char g_upd_err[160];
static int g_upd_status;

static void upd_set_err(const char *msg) {
  snprintf(g_upd_err, sizeof(g_upd_err), "%s", msg ? msg : "download failed");
}

static int host_ends(const char *host, size_t hlen, const char *suf) {
  size_t sl = strlen(suf);
  if (hlen < sl)
    return 0;
  if (strncasecmp(host + hlen - sl, suf, sl))
    return 0;
  return hlen == sl || host[hlen - sl - 1] == '.';
}

static int url_allowed_for_update(const char *url) {
  const char *host, *slash, *at, *colon;
  size_t hlen;
  /* Catalog payloads live on GitHub releases, GitLab raw, or GitHub Pages.
   * Redirect CDNs (release-assets, objects, blob) are followed after this check. */
  if (!url || strncmp(url, "https://", 8))
    return 0;
  host = url + 8;
  at = strchr(host, '@');
  slash = strchr(host, '/');
  if (at && (!slash || at < slash))
    host = at + 1;
  slash = strchr(host, '/');
  hlen = slash ? (size_t)(slash - host) : strlen(host);
  colon = memchr(host, ':', hlen);
  if (colon)
    hlen = (size_t)(colon - host);
  if (!hlen || hlen > 253)
    return 0;
  if (host_ends(host, hlen, "github.com") ||
      host_ends(host, hlen, "githubusercontent.com") ||
      host_ends(host, hlen, "github.io") ||
      host_ends(host, hlen, "gitlab.com") ||
      host_ends(host, hlen, "gitlab.io") ||
      host_ends(host, hlen, "git.etawen.dev"))
    return 1;
  return 0;
}

/* ---- Minimal sceHttp download (elfldr uri.c pattern) ---- */
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

static int g_upd_net = -1, g_upd_ssl = -1, g_upd_http = -1, g_upd_tmpl = -1;
static pthread_mutex_t g_upd_mu = PTHREAD_MUTEX_INITIALIZER;
static int g_upd_http_inited;

static int upd_ssl_cb(void) { return 0; }

static int update_http_init(void) {
  int err;
  if (g_upd_http_inited)
    return g_upd_tmpl >= 0 ? 0 : -1;
  g_upd_http_inited = 1;
  if (sceNetInit()) {
    upd_set_err("net init failed");
    return -1;
  }
  /* Pool must cover GitHub's huge response headers (CSP is >5 KB). */
  if ((g_upd_net = sceNetPoolCreate("elf-launcher-upd", 1024 * 1024, 0)) < 0) {
    upd_set_err("net pool failed");
    return -1;
  }
  if ((g_upd_ssl = sceSslInit(1024 * 1024)) < 0) {
    upd_set_err("ssl init failed");
    return -1;
  }
  if ((g_upd_http = sceHttpInit(g_upd_net, g_upd_ssl, 4 * 1024 * 1024)) < 0) {
    upd_set_err("http init failed");
    return -1;
  }
  if ((g_upd_tmpl = sceHttpCreateTemplate(g_upd_http, "elf-launcher/1.0", 2, 1)) < 0) {
    upd_set_err("http template failed");
    return -1;
  }
  sceHttpsSetSslCallback(g_upd_tmpl, (void *)upd_ssl_cb, 0);
  /* 0x2000 (8 KB) truncates GitHub release 302 headers, so Location is lost
   * and every catalog ELF fails the same way. 64 KB covers the CSP blob. */
  err = sceHttpSetResponseHeaderMaxSize(g_upd_tmpl, 64 * 1024);
  if (err < 0)
    err = sceHttpSetResponseHeaderMaxSize(g_upd_tmpl, 32 * 1024);
  if (err < 0)
    err = sceHttpSetResponseHeaderMaxSize(g_upd_tmpl, 8192);
  if (err < 0) {
    upd_set_err("header buffer rejected");
    return -1;
  }
  sceHttpSetAutoRedirect(g_upd_tmpl, 0);
  return 0;
}

static int resolve_redirect(const char *base, const char *loc, char *out,
                            size_t outsz) {
  if (!loc || !loc[0] || !out || outsz < 16)
    return -1;
  if (!strncmp(loc, "https://", 8) || !strncmp(loc, "http://", 7)) {
    if (strlen(loc) + 1 > outsz)
      return -1;
    memcpy(out, loc, strlen(loc) + 1);
    return 0;
  }
  if (loc[0] == '/' && loc[1] == '/') {
    const char *scheme = "https:";
    if (base && !strncmp(base, "http://", 7))
      scheme = "http:";
    if (strlen(scheme) + strlen(loc) + 1 > outsz)
      return -1;
    snprintf(out, outsz, "%s%s", scheme, loc);
    return 0;
  }
  if (loc[0] == '/' && base) {
    const char *p = strstr(base, "://");
    const char *slash;
    size_t origin;
    if (!p)
      return -1;
    p += 3;
    slash = strchr(p, '/');
    origin = slash ? (size_t)(slash - base) : strlen(base);
    if (origin + strlen(loc) + 1 > outsz)
      return -1;
    memcpy(out, base, origin);
    memcpy(out + origin, loc, strlen(loc) + 1);
    return 0;
  }
  if (base) {
    const char *slash = strrchr(base, '/');
    size_t prefix;
    if (!slash)
      return -1;
    prefix = (size_t)(slash - base + 1);
    if (prefix + strlen(loc) + 1 > outsz)
      return -1;
    memcpy(out, base, prefix);
    memcpy(out + prefix, loc, strlen(loc) + 1);
    return 0;
  }
  return -1;
}

/* Copy Location out of sceHttp headers. GitHub puts a ~1 KB signed URL
 * after a multi-KB CSP, and ParseResponseHeader often misses it. */
static int extract_location(int req, const char *base, char *out, size_t outsz) {
  char *hdrs = 0;
  size_t hdrsz = 0;
  const char *val = 0;
  size_t vlen = 0;
  char *locbuf;
  int rc = -1;
  if (sceHttpGetAllResponseHeaders(req, &hdrs, &hdrsz) || !hdrs || !hdrsz) {
    upd_set_err("no response headers");
    return -1;
  }
  if (sceHttpParseResponseHeader(hdrs, hdrsz, "Location", &val, &vlen) || !val ||
      !vlen) {
    const char *p = hdrs;
    const char *end = hdrs + hdrsz;
    val = 0;
    vlen = 0;
    while (p + 10 < end) {
      if ((p == hdrs || p[-1] == '\n') && (p[0] == 'L' || p[0] == 'l') &&
          !strncasecmp(p, "Location:", 9)) {
        p += 9;
        while (p < end && (*p == ' ' || *p == '\t'))
          p++;
        val = p;
        while (p < end && *p != '\r' && *p != '\n' && *p != '\0')
          p++;
        vlen = (size_t)(p - val);
        break;
      }
      p++;
    }
  }
  if (!val || !vlen) {
    upd_set_err("redirect without Location");
    return -1;
  }
  locbuf = (char *)malloc(vlen + 1);
  if (!locbuf) {
    upd_set_err("oom reading Location");
    return -1;
  }
  memcpy(locbuf, val, vlen);
  locbuf[vlen] = 0;
  while (vlen && (locbuf[vlen - 1] == '\r' || locbuf[vlen - 1] == '\n' ||
                  locbuf[vlen - 1] == ' ' || locbuf[vlen - 1] == '\t'))
    locbuf[--vlen] = 0;
  if (resolve_redirect(base, locbuf, out, outsz)) {
    upd_set_err("redirect URL too long");
    rc = -1;
  } else
    rc = 0;
  free(locbuf);
  return rc;
}

static int https_download_once(const char *url, char *next_url, size_t next_sz,
                               uint8_t **out, size_t *outlen) {
  int conn = -1, req = -1, status = 0, has_len = 0, rc = -1, errn = 0;
  uint64_t clen = 0;
  uint8_t *buf = 0;
  size_t got = 0, cap = 0;

  *out = 0;
  *outlen = 0;
  if (next_url && next_sz)
    next_url[0] = 0;

  conn = sceHttpCreateConnectionWithURL(g_upd_tmpl, url, 0);
  if (conn < 0) {
    upd_set_err("connect failed");
    return -2;
  }
  req = sceHttpCreateRequestWithURL(conn, 0, url, 0);
  if (req < 0) {
    upd_set_err("request create failed");
    sceHttpDeleteConnection(conn);
    return -3;
  }
  if (sceHttpSendRequest(req, 0, 0) < 0) {
    sceHttpGetLastErrno(req, &errn);
    snprintf(g_upd_err, sizeof(g_upd_err), "send failed errno %d", errn);
    sceHttpDeleteRequest(req);
    sceHttpDeleteConnection(conn);
    return -4;
  }
  if (sceHttpGetStatusCode(req, &status) < 0 || status < 0) {
    upd_set_err("no HTTP status");
    sceHttpDeleteRequest(req);
    sceHttpDeleteConnection(conn);
    return -5;
  }
  g_upd_status = status;
  if (status == 301 || status == 302 || status == 303 || status == 307 ||
      status == 308) {
    if (extract_location(req, url, next_url, next_sz)) {
      if (!g_upd_err[0])
        snprintf(g_upd_err, sizeof(g_upd_err), "redirect without Location HTTP %d",
                 status);
      rc = -6;
    } else
      rc = 1;
    sceHttpDeleteRequest(req);
    sceHttpDeleteConnection(conn);
    return rc;
  }
  if (status != 200) {
    snprintf(g_upd_err, sizeof(g_upd_err), "HTTP %d", status);
    sceHttpDeleteRequest(req);
    sceHttpDeleteConnection(conn);
    return -7;
  }
  sceHttpGetResponseContentLength(req, &has_len, &clen);
  if (has_len && clen > UPDATE_MAX_FILE) {
    upd_set_err("file too large");
    sceHttpDeleteRequest(req);
    sceHttpDeleteConnection(conn);
    return -8;
  }
  cap = (has_len && clen) ? (size_t)clen : (256 * 1024);
  if (cap > UPDATE_MAX_FILE)
    cap = UPDATE_MAX_FILE;
  buf = (uint8_t *)malloc(cap ? cap : 1);
  if (!buf) {
    upd_set_err("out of memory");
    sceHttpDeleteRequest(req);
    sceHttpDeleteConnection(conn);
    return -9;
  }
  for (;;) {
    int n;
    if (!has_len && got + 65536 > cap) {
      uint8_t *nbuf;
      size_t ncap = cap * 2;
      if (ncap > UPDATE_MAX_FILE)
        ncap = UPDATE_MAX_FILE;
      if (got >= ncap) {
        free(buf);
        upd_set_err("file too large");
        sceHttpDeleteRequest(req);
        sceHttpDeleteConnection(conn);
        return -8;
      }
      nbuf = (uint8_t *)realloc(buf, ncap);
      if (!nbuf) {
        free(buf);
        upd_set_err("out of memory");
        sceHttpDeleteRequest(req);
        sceHttpDeleteConnection(conn);
        return -9;
      }
      buf = nbuf;
      cap = ncap;
    }
    if (has_len && got >= (size_t)clen)
      break;
    n = sceHttpReadData(req, buf + got, cap - got);
    if (n < 0) {
      free(buf);
      snprintf(g_upd_err, sizeof(g_upd_err), "read failed (%d)", n);
      sceHttpDeleteRequest(req);
      sceHttpDeleteConnection(conn);
      return -10;
    }
    if (n == 0)
      break;
    got += (size_t)n;
  }
  sceHttpDeleteRequest(req);
  sceHttpDeleteConnection(conn);
  if (!got) {
    free(buf);
    upd_set_err("empty body");
    return -10;
  }
  if (has_len && clen && got != (size_t)clen) {
    free(buf);
    snprintf(g_upd_err, sizeof(g_upd_err), "short read %zu/%llu", got,
             (unsigned long long)clen);
    return -11;
  }
  *out = buf;
  *outlen = got;
  return 0;
}

static int https_download_url(const char *url, uint8_t **out, size_t *outlen) {
  /* GitHub release assets 302 to signed release-assets URLs (~1 KB, growing). */
  enum { URL_CAP = 16384 };
  char *cur = 0;
  char *next = 0;
  int hops = 0, rc;
  g_upd_err[0] = 0;
  g_upd_status = 0;
  if (update_http_init())
    return -1;
  cur = (char *)malloc(URL_CAP);
  next = (char *)malloc(URL_CAP);
  if (!cur || !next) {
    free(cur);
    free(next);
    upd_set_err("out of memory");
    return -9;
  }
  if (strlen(url) >= URL_CAP) {
    free(cur);
    free(next);
    upd_set_err("url too long");
    return -12;
  }
  memcpy(cur, url, strlen(url) + 1);
  pthread_mutex_lock(&g_upd_mu);
  for (;;) {
    rc = https_download_once(cur, next, URL_CAP, out, outlen);
    if (rc == 0) {
      pthread_mutex_unlock(&g_upd_mu);
      free(cur);
      free(next);
      g_upd_err[0] = 0;
      return 0;
    }
    if (rc == 1) {
      hops++;
      if (hops > 6 || !next[0]) {
        pthread_mutex_unlock(&g_upd_mu);
        free(cur);
        free(next);
        if (!g_upd_err[0])
          upd_set_err("too many redirects");
        return -20;
      }
      memcpy(cur, next, strlen(next) + 1);
      continue;
    }
    pthread_mutex_unlock(&g_upd_mu);
    free(cur);
    free(next);
    if (!g_upd_err[0])
      snprintf(g_upd_err, sizeof(g_upd_err), "download failed (%d)", rc);
    return rc;
  }
}
