#include <Windows.h>
#include <suture.h>
#include <suture/tracker.h>

static void JNICALL click_mouse_trampoline(JNIEnv *env, jobject instance) {
  printf("click_mouse_trampoline: called\n");
}

static DWORD WINAPI ThreadMain(LPVOID lpParams) {
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
