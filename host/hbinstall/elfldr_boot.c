#include <ps5/kernel.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "elfldr.h"
#include "notify.h"
#include "pt.h"

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

INCASSET(bootstrap_elf, "assets/bootstrap.elf");

/* Same bootstrap elfldr uses, run inside this payload so port 9020 is not required. */
int start_real_elfldr(void) {
  pid_t mypid = getpid();
  uint8_t qa_flags[16];
  uint8_t caps[16];
  uint64_t authid;
  intptr_t vnode;
  pid_t vpid;
  int ret = 0;

  if (elfldr_sanity_check(bootstrap_elf, bootstrap_elf_size))
    return -1;
  if (kernel_get_qaflags(qa_flags))
    return -1;
  if (!(qa_flags[1] & 0x03)) {
    qa_flags[1] |= 0x03;
    if (kernel_set_qaflags(qa_flags))
      return -1;
  }
  if (!(vnode = kernel_get_proc_rootdir(mypid)))
    return -1;
  if (kernel_get_ucred_caps(mypid, caps))
    return -1;
  if (!(authid = kernel_get_ucred_authid(mypid)))
    return -1;
  if ((vpid = elfldr_find_pid("SceRedisServer")) < 0)
    return -1;
  if (elfldr_raise_privileges(mypid))
    ret = -1;
  else if (pt_attach(vpid))
    ret = -1;
  else
    ret = elfldr_exec(vpid, -1, bootstrap_elf);

  if (kernel_set_proc_jaildir(mypid, vnode))
    ret = -1;
  if (kernel_set_proc_rootdir(mypid, vnode))
    ret = -1;
  if (kernel_set_ucred_caps(mypid, caps))
    ret = -1;
  if (kernel_set_ucred_authid(mypid, authid))
    ret = -1;
  return ret;
}
