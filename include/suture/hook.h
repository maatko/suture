#ifndef SUTURE_HOOK_H
#define SUTURE_HOOK_H

enum su_hook_type {
  SU_HOOK_DETOUR
};

struct su_hook {
  enum su_hook_type type;

  char *name;
  char *signature;
  char *class_name;

  void *function;
};

#endif // SUTURE_HOOK_H