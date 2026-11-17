<p align="center">
 <img width="200px" src="logo.png" alt="Logo" />
</p>

<p align="center">
  <img src="https://img.shields.io/badge/License-MIT-blue.svg" alt="License: MIT">
</p>

# suture
c powered hooking engine for the jvm

### usage

#### detours
```c
 static jmethodID original_click_mouse;
 
 static void JNICALL click_mouse_detour(JNIEnv *env, jobject instance) {
   printf("click_mouse_detour: called\n");
 
   (*env)->CallVoidMethod(env, instance, original_click_mouse);
 }

  ....

  void dll_main(){
      struct su_env env = { 0 };
  if (su_init(&env) != SU_OK) {
    fprintf(stderr, "Failed to initialize the suture library");
    goto exit;
  }

  if (su_detour(&env, "net/minecraft/client/Minecraft", "startAttack", "()V", &original_click_mouse, click_mouse_detour) != SU_OK) {
    fprintf(stderr, "Failed to register method detour hook");
    goto exit;
  }

  enum su_error status = SU_OK;
  if ((status = su_transform(&env)) != SU_OK) {
    fprintf(stderr, "Failed to apply the class transforms (%d)", status);
    goto exit;
  }

  printf("Transform ok\n");

  while (!(GetAsyncKeyState(VK_DELETE) & 0x1))
    Sleep(100);

exit:
  su_dispose(&env);

  if (print_leaks())
    Sleep(2500);

  FreeLibraryAndExitThread((HINSTANCE)lpParams, 0);
  }
```

#### trampolines
```c
static void JNICALL click_mouse_trampoline(JNIEnv *env, jobject instance) {
  printf("click_mouse_trampoline: called\n");
}

....

void dll_main(){
  struct su_env env = { 0 };
  if (su_init(&env) != SU_OK) {
    fprintf(stderr, "Failed to initialize the suture library");
    goto exit;
  }

  if (su_trampoline(&env, "a", "b", "()V", click_mouse_trampoline) != SU_OK) {
    fprintf(stderr, "Failed to register method trampoline hook");
    goto exit;
  }

  if (su_transform(&env) != SU_OK) {
    fprintf(stderr, "Failed to apply the class transforms");
    goto exit;
  }

  printf("Transform ok\n");

  while (!(GetAsyncKeyState(VK_DELETE) & 0x1))
    Sleep(100);

exit:
  su_dispose(&env);

  if (print_leaks())
    Sleep(2500);

  FreeLibraryAndExitThread((HINSTANCE)lpParams, 0);
}
```