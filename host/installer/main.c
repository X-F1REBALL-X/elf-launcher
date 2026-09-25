/* ps5-elfs-host-installer: permanent homescreen browser launcher (PSFE00001)
 *
 * elfldr:9021 cannot start payloads that NEEDED libSceAppInstUtil.sprx
 * (bisect confirmed). Also cannot dlopen it without privilege.
 *
 * Working approach:
 *   1) write param.json + icon0 (same layout as live SLKT00001 / WK Autoloader)
 *   2) elevate authid + caps
 *   3) sceKernelLoadStartModule(libSceAppInstUtil) via NID
 *   4) InstallTitleDir via NID Wudg3Xe3heE
 */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <ps5/kernel.h>

#define TITLE_ID "PSFE00001"

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

INCASSET(param_json, "assets/param.json");
INCASSET(icon0_png, "assets/icon0.png");

#define NID_LoadStart "wzvqT4UqKX8"
#define NID_Initialize "540lotO7oHE"
#define NID_Terminate "kLLazhNh6d4"
#define NID_InstallAll "+scQA5stvjs"
#define NID_InstallTitleDir "Wudg3Xe3heE"

static void marker(const char *msg) {
  FILE *f = fopen("/user/app/" TITLE_ID "/sce_sys/installer_log.txt", "a");
  if (!f)
    return;
  fprintf(f, "%s\n", msg);
  fclose(f);
}

static int mkdir_p(const char *path, mode_t mode) {
  char tmp[256];
  snprintf(tmp, sizeof(tmp), "%s", path);
  size_t len = strlen(tmp);
  if (len == 0)
    return 0;
  if (tmp[len - 1] == '/')
    tmp[len - 1] = '\0';
  for (char *p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      if (mkdir(tmp, mode) != 0 && errno != EEXIST)
        return -1;
      *p = '/';
    }
  }
  return (mkdir(tmp, mode) != 0 && errno != EEXIST) ? -1 : 0;
}

static int install_file(const char *path, const uint8_t *data, size_t size) {
  FILE *f = fopen(path, "wb");
  if (!f)
    return -1;
  if (fwrite(data, size, 1, f) != 1) {
    fclose(f);
    return -1;
  }
  fclose(f);
  return 0;
}

static int kernel_handle(uint32_t *out) {
  if (!kernel_dynlib_handle(-1, "libkernel_web.sprx", out) ||
      !kernel_dynlib_handle(-1, "libkernel.sprx", out) ||
      !kernel_dynlib_handle(-1, "libkernel_sys.sprx", out))
    return 0;
  return -1;
}

int main(void) {
  char sce_sys[128], param_path[160], icon_path[160], buf[160];
  uint32_t kh = 0, ah = 0;
  int res = 0, rv;
  uint8_t privcaps[16] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                          0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  int (*LoadStart)(const char *, size_t, const void *, uint32_t, void *,
                   int *) = 0;
  int (*Init)(void) = 0;
  int (*Term)(void) = 0;
  int (*InstallAll)(void *) = 0;
  int (*InstallDir)(const char *, const char *, void *) = 0;
  static const char *mod_names[] = {
      "libSceAppInstUtil.sprx",
      "/system/common/lib/libSceAppInstUtil.sprx",
      "/system/priv/lib/libSceAppInstUtil.sprx",
      "/system_ex/common_ex/lib/libSceAppInstUtil.sprx",
      "/system_ex/priv_ex/lib/libSceAppInstUtil.sprx",
      0};

  snprintf(sce_sys, sizeof(sce_sys), "/user/app/%s/sce_sys", TITLE_ID);
  snprintf(param_path, sizeof(param_path), "/user/app/%s/sce_sys/param.json",
           TITLE_ID);
  snprintf(icon_path, sizeof(icon_path), "/user/app/%s/sce_sys/icon0.png",
           TITLE_ID);

  if (mkdir_p(sce_sys, 0755))
    return 1;
  /* fresh log */
  fclose(fopen(param_path, "ab")); /* touch dir writable */
  {
    FILE *lf = fopen("/user/app/" TITLE_ID "/sce_sys/installer_log.txt", "w");
    if (lf)
      fclose(lf);
  }
  marker("start");

  if (install_file(param_path, param_json, param_json_size) ||
      install_file(icon_path, icon0_png, icon0_png_size)) {
    marker("asset write fail");
    return 1;
  }
  marker("assets ok");

  /* Elevate like websrv elfldr / pt */
  kernel_set_ucred_authid(-1, 0x4801000000000013L);
  kernel_set_ucred_caps(-1, privcaps);
  marker("elevated");

  if (kernel_handle(&kh)) {
    marker("no libkernel");
    return 1;
  }
  LoadStart = (void *)kernel_dynlib_resolve(-1, kh, NID_LoadStart);
  snprintf(buf, sizeof(buf), "LoadStart=%p", (void *)LoadStart);
  marker(buf);
  if (!LoadStart)
    return 1;

  if (kernel_dynlib_handle(-1, "libSceAppInstUtil.sprx", &ah)) {
    for (int i = 0; mod_names[i]; i++) {
      res = -1;
      rv = LoadStart(mod_names[i], 0, 0, 0, 0, &res);
      snprintf(buf, sizeof(buf), "load %s rv=%d res=%d", mod_names[i], rv, res);
      marker(buf);
      if (!kernel_dynlib_handle(-1, "libSceAppInstUtil.sprx", &ah))
        break;
    }
  }

  if (kernel_dynlib_handle(-1, "libSceAppInstUtil.sprx", &ah)) {
    marker("AppInstUtil missing — assets on disk; refresh home may still work");
    return 2;
  }
  snprintf(buf, sizeof(buf), "ah=0x%x", ah);
  marker(buf);

  Init = (void *)kernel_dynlib_resolve(-1, ah, NID_Initialize);
  Term = (void *)kernel_dynlib_resolve(-1, ah, NID_Terminate);
  InstallAll = (void *)kernel_dynlib_resolve(-1, ah, NID_InstallAll);
  InstallDir = (void *)kernel_dynlib_resolve(-1, ah, NID_InstallTitleDir);

  if (!Init) {
    marker("no Init");
    return 1;
  }
  rv = Init();
  snprintf(buf, sizeof(buf), "Init=0x%08X", rv);
  marker(buf);
  if (rv)
    return 1;

  if (InstallDir)
    rv = InstallDir(TITLE_ID, "/user/app/", 0);
  else if (InstallAll)
    rv = InstallAll(0);
  else
    rv = -1;
  snprintf(buf, sizeof(buf), "install=0x%08X", rv);
  marker(buf);
  if (Term)
    Term();

  marker(rv ? "FAIL" : "OK");
  return rv ? 1 : 0;
}
