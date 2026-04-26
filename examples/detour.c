#include <Windows.h>
#include <suture.h>
#include <suture/tracker.h>

static jmethodID original_click_mouse;

static void JNICALL click_mouse_detour(JNIEnv *env, jobject instance) {
  printf("click_mouse_detour: called\n");

  (*env)->CallVoidMethod(env, instance, original_click_mouse);
}

static jmethodID original_render_game_overlay;

static void JNICALL render_game_overlay_detour(JNIEnv *env, jobject instance, jfloat partial_ticks) {
  (*env)->CallVoidMethod(env, instance, original_render_game_overlay, partial_ticks);

  if (GetAsyncKeyState(VK_ESCAPE) & 0x1) {
    printf("Escape key pressed\n");
  }
}

static DWORD WINAPI ThreadMain(LPVOID lpParams) {
  struct su_env env = { 0 };
  if (su_init(&env) != SU_OK) {
    fprintf(stderr, "Failed to initialize the suture library");
    goto exit;
  }

  if (su_detour(&env, "ave", "ax", "()V", &original_click_mouse, click_mouse_detour) != SU_OK) {
    fprintf(stderr, "Failed to register method detour hook");
    goto exit;
  }

  if (su_detour(&env, "avo", "a", "(F)V", &original_render_game_overlay, render_game_overlay_detour) != SU_OK) {
    fprintf(stderr, "Failed to register method detour hook");
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

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
  static FILE *in;
  static FILE *out;
  static FILE *err;

  switch (fdwReason) {
    case DLL_PROCESS_ATTACH: {
      AllocConsole();
      SetConsoleCtrlHandler(NULL, TRUE);

      freopen_s(&in, "conin$", "r", stdin);
      freopen_s(&out, "conout$", "w", stdout);
      freopen_s(&err, "conout$", "w", stderr);

      HANDLE handle = CreateThread(0, 0, ThreadMain, hinstDLL, 0, 0);
      if (handle)
        CloseHandle(handle);
    } break;
    case DLL_PROCESS_DETACH: {
      if (in)
        fclose(in);
      if (out)
        fclose(out);
      if (err)
        fclose(err);

      FreeConsole();
    } break;
    default:;
  }
  return TRUE;
}