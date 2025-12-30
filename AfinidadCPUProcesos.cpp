#define STRICT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <dwmapi.h>
#include <vector>
#include <string>

// Vincular librerías
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// IDs de Controles
#define ID_LISTVIEW 1001
#define ID_BTN_REFRESH 1002
#define ID_BTN_APPLY 1003
#define ID_BTN_SELECTALL 1004
#define ID_CHKS_CPU_BASE 2000

// Colores Dark Theme
#define COLOR_BG      RGB(30, 30, 30)
#define COLOR_TEXT    RGB(240, 240, 240)
#define COLOR_ACCENT  RGB(0, 120, 215)
#define COLOR_LIST_BG RGB(45, 45, 45)

// Variables Globales
HWND hListView;
std::vector<HWND> cpuCheckboxes;
int numProcessors = 0;
HBRUSH hBrushBack;
HFONT hFontTitle, hFontNormal;

// Función para activar el modo oscuro en la barra de título
void EnableDarkTitleBar(HWND hwnd) {
    BOOL value = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &value, sizeof(value)); // DWMWA_USE_IMMERSIVE_DARK_MODE
}

// Función para listar procesos
void RefreshProcessList() {
    ListView_DeleteAllItems(hListView);
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(hSnapshot, &pe)) {
        int index = 0;
        do {
            LVITEMW lvItem = { 0 };
            lvItem.mask = LVIF_TEXT | LVIF_PARAM;
            lvItem.iItem = index;
            lvItem.pszText = pe.szExeFile;
            lvItem.lParam = (LPARAM)pe.th32ProcessID;
            ListView_InsertItem(hListView, &lvItem);

            std::wstring pidStr = std::to_wstring(pe.th32ProcessID);
            ListView_SetItemText(hListView, index, 1, (LPWSTR)pidStr.c_str());
            index++;
        } while (Process32NextW(hSnapshot, &pe));
    }
    CloseHandle(hSnapshot);
}

// Función para aplicar afinidad
void ApplyAffinity() {
    DWORD_PTR mask = 0;
    for (int i = 0; i < numProcessors; i++) {
        if (SendMessage(cpuCheckboxes[i], BM_GETCHECK, 0, 0) == BST_CHECKED) {
            mask |= ((DWORD_PTR)1 << i);
        }
    }

    if (mask == 0) {
        MessageBox(NULL, L"Selecciona al menos un núcleo de CPU.", L"Error", MB_ICONERROR);
        return;
    }

    int itemCount = ListView_GetItemCount(hListView);
    int successCount = 0;
    int failCount = 0;

    for (int i = 0; i < itemCount; i++) {
        if (ListView_GetCheckState(hListView, i)) {
            LVITEM lvItem = { 0 };
            lvItem.mask = LVIF_PARAM;
            lvItem.iItem = i;
            ListView_GetItem(hListView, &lvItem);
            DWORD pid = (DWORD)lvItem.lParam;

            HANDLE hProcess = OpenProcess(PROCESS_SET_INFORMATION, FALSE, pid);
            if (hProcess) {
                if (SetProcessAffinityMask(hProcess, mask)) successCount++;
                else failCount++;
                CloseHandle(hProcess);
            }
            else failCount++;
        }
    }
    std::wstring msg = L"Finalizado.\nÉxitos: " + std::to_wstring(successCount) + L"\nFallidos: " + std::to_wstring(failCount);
    MessageBox(NULL, msg.c_str(), L"Información", MB_ICONINFORMATION);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        EnableDarkTitleBar(hwnd);
        hBrushBack = CreateSolidBrush(COLOR_BG);

        // Fuentes
        hFontTitle = CreateFont(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
        hFontNormal = CreateFont(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");

        // Encabezado
        HWND hHeader = CreateWindow(L"STATIC", L"Aplicación C++ escritorio de Windows", WS_CHILD | WS_VISIBLE, 20, 10, 500, 40, hwnd, NULL, NULL, NULL);
        SendMessage(hHeader, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        numProcessors = sysInfo.dwNumberOfProcessors;

        // ListView
        hListView = CreateWindowEx(0, WC_LISTVIEW, L"", WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SHOWSELALWAYS,
            20, 60, 400, 380, hwnd, (HMENU)ID_LISTVIEW, NULL, NULL);
        ListView_SetExtendedListViewStyle(hListView, LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES | LVS_EX_DOUBLEBUFFER);

        // Colores del ListView
        ListView_SetBkColor(hListView, COLOR_LIST_BG);
        ListView_SetTextBkColor(hListView, COLOR_LIST_BG);
        ListView_SetTextColor(hListView, COLOR_TEXT);

        LVCOLUMNW lvc = { 0 };
        lvc.mask = LVCF_TEXT | LVCF_WIDTH;
        lvc.pszText = (LPWSTR)L"Proceso"; lvc.cx = 280;
        ListView_InsertColumn(hListView, 0, &lvc);
        lvc.pszText = (LPWSTR)L"PID"; lvc.cx = 80;
        ListView_InsertColumn(hListView, 1, &lvc);

        // Checkboxes CPU
        int startX = 440, startY = 80;
        HWND hCpuLabel = CreateWindow(L"STATIC", L"Hilos de CPU:", WS_CHILD | WS_VISIBLE, startX, 60, 150, 20, hwnd, NULL, NULL, NULL);
        SendMessage(hCpuLabel, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        for (int i = 0; i < numProcessors; i++) {
            std::wstring cpuName = L"CPU " + std::to_wstring(i);
            HWND hChk = CreateWindow(L"BUTTON", cpuName.c_str(), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                startX + (i / 16) * 90, startY + (i % 16) * 22, 80, 20, hwnd, (HMENU)(ID_CHKS_CPU_BASE + i), NULL, NULL);
            SendMessage(hChk, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
            cpuCheckboxes.push_back(hChk);
        }

        // Botones
        HWND btn1 = CreateWindow(L"BUTTON", L"Seleccionar Todo", WS_CHILD | WS_VISIBLE, 20, 450, 150, 35, hwnd, (HMENU)ID_BTN_SELECTALL, NULL, NULL);
        HWND btn2 = CreateWindow(L"BUTTON", L"Actualizar Lista", WS_CHILD | WS_VISIBLE, 180, 450, 150, 35, hwnd, (HMENU)ID_BTN_REFRESH, NULL, NULL);
        HWND btn3 = CreateWindow(L"BUTTON", L"Aplicar Afinidad", WS_CHILD | WS_VISIBLE, 440, 450, 180, 35, hwnd, (HMENU)ID_BTN_APPLY, NULL, NULL);

        SendMessage(btn1, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        SendMessage(btn2, WM_SETFONT, (WPARAM)hFontNormal, TRUE);
        SendMessage(btn3, WM_SETFONT, (WPARAM)hFontNormal, TRUE);

        RefreshProcessList();
        break;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = (HDC)wParam;
        SetTextColor(hdcStatic, COLOR_TEXT);
        SetBkColor(hdcStatic, COLOR_BG);
        return (INT_PTR)hBrushBack;
    }

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        if (wmId == ID_BTN_REFRESH) RefreshProcessList();
        else if (wmId == ID_BTN_SELECTALL) {
            int count = ListView_GetItemCount(hListView);
            for (int i = 0; i < count; i++) ListView_SetCheckState(hListView, i, TRUE);
        }
        else if (wmId == ID_BTN_APPLY) ApplyAffinity();
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        FillRect(hdc, &ps.rcPaint, hBrushBack);
        EndPaint(hwnd, &ps);
        break;
    }

    case WM_DESTROY:
        DeleteObject(hBrushBack);
        DeleteObject(hFontTitle);
        DeleteObject(hFontNormal);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    InitCommonControls();
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = NULL; // Manejado en WM_PAINT
    wc.lpszClassName = L"DarkAffinityManager";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, L"DarkAffinityManager", L"Gestor de Afinidad de CPU",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 660, 550, NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}