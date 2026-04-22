<p align="center">
 <img width="200px" src="logo.png" alt="Logo" />
</p>

# suture
c powered hooking engine for the jvm

[Documentation](https://maatko.github.io/suture/)

### usage

```c
 static jmethodID original_click_mouse;
 
 static void JNICALL click_mouse_detour(JNIEnv *env, jobject instance) {
   printf("click_mouse_detour: called\n");
 
   (*env)->CallVoidMethod(env, instance, original_click_mouse);
 }

  ....

  struct su_env env = { 0 };
  if (su_init(&env) != SU_OK) {
    fprintf(stderr, "Failed to initialize the suture library");
    return 1;
  }

  if (su_detour(&env, "some/random/Class", "clickMouse", "()V", &original_click_mouse, click_mouse_detour) != SU_OK) {
    fprintf(stderr, "Failed to register method detour hook");
    return 1;
  }

  if (su_transform(&env) != SU_OK) {
    fprintf(stderr, "Failed to apply the class transforms");
    return 1;
  }

  while (!(GetAsyncKeyState(VK_DELETE) & 0x1))
    Sleep(100);

  su_dispose(&env);
```
