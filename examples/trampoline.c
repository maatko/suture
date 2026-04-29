#include <Windows.h>
#include <suture.h>

static void JNICALL click_mouse_detour(JNIEnv *env, jobject instance) {
  printf("click_mouse_detour: called\n");
}

static DWORD WINAPI ThreadMain(LPVOID lpParams) {
  struct su_env env = { 0 };
  if (su_init(&env) != SU_OK) {
    fprintf(stderr, "Failed to initialize the suture library\n");
    goto exit;
  }

  if (su_trampoline(&env, "ave", "ax", "()V", click_mouse_detour) != SU_OK) {
    fprintf(stderr, "Failed to register method detour hook\n");
    goto exit;
  }

  if (su_transform(&env) != SU_OK) {
    fprintf(stderr, "Failed to apply the class transforms\n");
    goto exit;
  }

  printf("Transform ok\n");
  goto success;

exit:
  printf("Transform failed\n");

success:
  while (!(GetAsyncKeyState(VK_DELETE) & 0x1))
    Sleep(100);

  su_dispose(&env);
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