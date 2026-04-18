#include <Windows.h>
#include <assert.h>

#include <suture.h>

static FILE *in;
static FILE *out;
static FILE *err;

static void OpenConsole();
static void CloseConsole();

static jmethodID original_click_mouse;

static void JNICALL click_mouse_detour(JNIEnv *env, jobject instance) {
  printf("click_mouse_detour: called\n");

  (*env)->CallVoidMethod(env, instance, original_click_mouse);
}

static DWORD WINAPI ThreadMain(LPVOID lpParams) {
  OpenConsole();

  struct su_env env = {0};
  if (su_init(&env) != SU_OK) {
    fprintf(stderr, "Failed to initialize the suture library");
    return 1;
  }

  if (su_detour(&env, "ave", "ax", "()V", &original_click_mouse, click_mouse_detour) != SU_OK) {
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

  CloseConsole();
  FreeLibraryAndExitThread((HINSTANCE)lpParams, 0);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
  if (fdwReason != DLL_PROCESS_ATTACH)
    return TRUE;

  const HANDLE handle = CreateThread(0, 0, ThreadMain, hinstDLL, 0, 0);
  if (handle)
    CloseHandle(handle);

  return TRUE;
}

static void OpenConsole() {
  AllocConsole();
  SetConsoleCtrlHandler(NULL, TRUE);

  freopen_s(&in, "conin$", "r", stdin);
  freopen_s(&out, "conout$", "w", stdout);
  freopen_s(&err, "conout$", "w", stderr);
}

static void CloseConsole() {
  if (in)
    fclose(in);
  if (out)
    fclose(out);
  if (err)
    fclose(err);

  FreeConsole();
}