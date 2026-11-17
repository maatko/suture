#include <Windows.h>
#include <suture.h>
#include <suture/tracker.h>

static jmethodID original_click_mouse;

static void JNICALL click_mouse_detour(JNIEnv *env, jobject instance) {
  printf("click_mouse_detour: called\n");

  (*env)->CallVoidMethod(env, instance, original_click_mouse);
}

static DWORD WINAPI ThreadMain(LPVOID lpParams) {
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