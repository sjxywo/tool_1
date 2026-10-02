#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:wWinMainCRTStartup")
#pragma execution_character_set("utf-8")
#include <windows.h>
#include <string>
#include <vector>
#include <commctrl.h>
#include <shlobj.h>
#include <uxtheme.h>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "msimg32.lib")

// ==================== 常量与前置声明 ====================
#define MAX_ABOUT_ITEMS 20 // 关于页面的最大行数，以后随便加

void UpdatePreview();
void UpdateName();
LRESULT CALLBACK ItemButtonProc(HWND, UINT, WPARAM, LPARAM);
void LayoutControls(HWND hWnd);
void DrawModernButton(DRAWITEMSTRUCT* dis);
void ShowSettingsView(HWND hWnd);
void ShowAboutView(HWND hWnd);

// ==================== 颜色定义 ====================
#define CLR_BG_TOP      RGB(40, 44, 52)
#define CLR_BG_BOTTOM   RGB(15, 15, 15)
#define CLR_BTN_BG      RGB(65, 70, 80)
#define CLR_BTN_HOVER   RGB(80, 130, 210)
#define CLR_BTN_TEXT    RGB(240, 240, 240)
#define CLR_EDIT_BG     RGB(45, 48, 55)
#define CLR_EDIT_TEXT   RGB(220, 220, 220)

// ==================== 全局变量 ====================
const int MAX_ITEMS = 16;
struct ItemData {
    int type; // 0 = 模块, 1 = 分隔符
    std::wstring name;
    std::wstring value;
};
ItemData g_items[MAX_ITEMS];
int g_itemCount = 0;
int g_selectedIndex = -1;
std::wstring g_history[10];
int g_historyCount = 0;

int g_conflictMode = 0; // 0=自动序号, 1=日期后缀, 2=覆盖, 3=跳过, 4=弹窗询问

HWND hEditName, hEditValue, hStaticValue, hPreview, hStaticName;
HWND hItemButtons[MAX_ITEMS];
HWND hInsertionLine = NULL;
HWND hListView = NULL;
HWND hDropZone = NULL;
HWND hRadioButtons[5] = { NULL, NULL, NULL, NULL, NULL };
HWND hAboutTexts[MAX_ABOUT_ITEMS] = { 0 };          // 扩大到20行
std::wstring g_aboutUrls[MAX_ABOUT_ITEMS] = { L"" }; // 存储对应行的链接，如果为空则只是文本
HFONT hFont = NULL;

int g_dragIndex = -1;
int g_dropIndex = -1;
WNDPROC g_oldButtonProc[MAX_ITEMS];
RECT g_btnRects[MAX_ITEMS];

HBRUSH hEditBrush = NULL;

// ==================== 注册表读写 ====================
void SaveConfig() {
    HKEY hKey;
    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\HomeworkRenamer", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL);
    RegSetValueExW(hKey, L"ItemCount", 0, REG_DWORD, (const BYTE*)&g_itemCount, sizeof(g_itemCount));
    RegSetValueExW(hKey, L"ConflictMode", 0, REG_DWORD, (const BYTE*)&g_conflictMode, sizeof(g_conflictMode));
    for (int i = 0; i < g_itemCount; ++i) {
        std::wstring idx = std::to_wstring(i);
        RegSetValueExW(hKey, (L"Item" + idx + L"_Type").c_str(), 0, REG_DWORD, (const BYTE*)&g_items[i].type, sizeof(int));
        RegSetValueExW(hKey, (L"Item" + idx + L"_Name").c_str(), 0, REG_SZ, (const BYTE*)g_items[i].name.c_str(), (g_items[i].name.size() + 1) * sizeof(wchar_t));
        RegSetValueExW(hKey, (L"Item" + idx + L"_Value").c_str(), 0, REG_SZ, (const BYTE*)g_items[i].value.c_str(), (g_items[i].value.size() + 1) * sizeof(wchar_t));
    }
    RegCloseKey(hKey);
}

void LoadConfig() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\HomeworkRenamer", 0, KEY_READ, &hKey) != ERROR_SUCCESS) return;
    DWORD size = sizeof(g_itemCount);
    RegQueryValueExW(hKey, L"ItemCount", NULL, NULL, (LPBYTE)&g_itemCount, &size);
    size = sizeof(g_conflictMode);
    RegQueryValueExW(hKey, L"ConflictMode", NULL, NULL, (LPBYTE)&g_conflictMode, &size);
    for (int i = 0; i < g_itemCount; ++i) {
        std::wstring idx = std::to_wstring(i);
        DWORD typeSize = sizeof(int);
        RegQueryValueExW(hKey, (L"Item" + idx + L"_Type").c_str(), NULL, NULL, (LPBYTE)&g_items[i].type, &typeSize);
        wchar_t bufName[256] = { 0 }, bufVal[256] = { 0 };
        DWORD sz = sizeof(bufName);
        RegQueryValueExW(hKey, (L"Item" + idx + L"_Name").c_str(), NULL, NULL, (LPBYTE)bufName, &sz);
        sz = sizeof(bufVal);
        RegQueryValueExW(hKey, (L"Item" + idx + L"_Value").c_str(), NULL, NULL, (LPBYTE)bufVal, &sz);
        g_items[i].name = bufName; g_items[i].value = bufVal;
    }
    RegCloseKey(hKey);
}

void SaveHistory() {
    HKEY hKey;
    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\HomeworkRenamer", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL);
    RegSetValueExW(hKey, L"HistoryCount", 0, REG_DWORD, (const BYTE*)&g_historyCount, sizeof(g_historyCount));
    for (int i = 0; i < g_historyCount; ++i) {
        RegSetValueExW(hKey, (L"History" + std::to_wstring(i)).c_str(), 0, REG_SZ, (const BYTE*)g_history[i].c_str(), (g_history[i].size() + 1) * sizeof(wchar_t));
    }
    RegCloseKey(hKey);
}

void LoadHistory() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\HomeworkRenamer", 0, KEY_READ, &hKey) != ERROR_SUCCESS) return;
    DWORD size = sizeof(g_historyCount);
    RegQueryValueExW(hKey, L"HistoryCount", NULL, NULL, (LPBYTE)&g_historyCount, &size);
    for (int i = 0; i < g_historyCount; ++i) {
        wchar_t buf[512] = { 0 };
        DWORD sz = sizeof(buf);
        RegQueryValueExW(hKey, (L"History" + std::to_wstring(i)).c_str(), NULL, NULL, (LPBYTE)buf, &sz);
        g_history[i] = buf;
    }
    RegCloseKey(hKey);
}

void AddHistory(const std::wstring& oldName, const std::wstring& newName) {
    std::wstring record = oldName + L" -> " + newName;
    for (int i = 9; i > 0; --i) g_history[i] = g_history[i - 1];
    g_history[0] = record;
    if (g_historyCount < 10) g_historyCount++;
    SaveHistory();
}

// ==================== 自绘按钮 ====================
void DrawModernButton(DRAWITEMSTRUCT* dis) {
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    int state = dis->itemState;

    HBRUSH hBgBrush = CreateSolidBrush(CLR_BG_TOP);
    FillRect(hdc, &rc, hBgBrush);
    DeleteObject(hBgBrush);

    COLORREF bgColor = CLR_BTN_BG;
    if (state & ODS_SELECTED) bgColor = RGB(50, 100, 180);
    else if (state & ODS_FOCUS || state & ODS_HOTLIGHT) bgColor = CLR_BTN_HOVER;

    HRGN hRgn = CreateRoundRectRgn(rc.left, rc.top, rc.right + 1, rc.bottom + 1, 20, 20);
    SelectClipRgn(hdc, hRgn);

    HBRUSH hBrush = CreateSolidBrush(bgColor);
    FillRect(hdc, &rc, hBrush);
    DeleteObject(hBrush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, CLR_BTN_TEXT);
    wchar_t text[256];
    GetWindowTextW(dis->hwndItem, text, 256);
    DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectClipRgn(hdc, NULL);
    DeleteObject(hRgn);
}

// ==================== 排版布局 ====================
void LayoutControls(HWND hWnd) {
    RECT rc;
    GetClientRect(hWnd, &rc);
    int w = rc.right, h = rc.bottom;

    int leftX = 15, leftW = 130;
    SetWindowPos(GetDlgItem(hWnd, 101), NULL, leftX, 20, leftW, 40, SWP_NOZORDER);  // 预览
    SetWindowPos(GetDlgItem(hWnd, 103), NULL, leftX, 70, leftW, 40, SWP_NOZORDER);  // 历史
    SetWindowPos(GetDlgItem(hWnd, 102), NULL, leftX, 120, leftW, 40, SWP_NOZORDER); // 设置
    SetWindowPos(GetDlgItem(hWnd, 104), NULL, leftX, 170, leftW, 40, SWP_NOZORDER); // 关于

    int rightX = 165;
    int rightW = w - rightX - 15;

    // 功能按钮区
    SetWindowPos(GetDlgItem(hWnd, 200), NULL, rightX, 20, 120, 40, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hWnd, 201), NULL, rightX + 130, 20, 120, 40, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hWnd, 202), NULL, rightX + 260, 20, 100, 40, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hWnd, 203), NULL, rightX + 370, 20, 100, 40, SWP_NOZORDER);

    // 模块条
    int btnW = 100, btnH = 40, spacing = 10;
    int btnPerRow = rightW / (btnW + spacing);
    if (btnPerRow < 1) btnPerRow = 1;
    for (int i = 0; i < g_itemCount; ++i) {
        int row = i / btnPerRow, col = i % btnPerRow;
        int btnX = rightX + col * (btnW + spacing);
        int btnY = 70 + row * (btnH + spacing);
        SetWindowPos(hItemButtons[i], NULL, btnX, btnY, btnW, btnH, SWP_NOZORDER);
        GetWindowRect(hItemButtons[i], &g_btnRects[i]);
        MapWindowPoints(HWND_DESKTOP, hWnd, (LPPOINT)&g_btnRects[i], 2);
    }
    int moduleRows = (g_itemCount + btnPerRow - 1) / btnPerRow;
    if (moduleRows == 0) moduleRows = 1;
    int moduleBarHeight = 70 + moduleRows * (btnH + spacing);

    // 编辑区
    int editY = moduleBarHeight + 10;
    SetWindowPos(hStaticName, NULL, rightX, editY, 50, 25, SWP_NOZORDER);
    SetWindowPos(hEditName, NULL, rightX + 50, editY, 120, 28, SWP_NOZORDER);
    SetWindowPos(hStaticValue, NULL, rightX + 180, editY, 40, 25, SWP_NOZORDER);
    SetWindowPos(hEditValue, NULL, rightX + 220, editY, rightW - 220, 28, SWP_NOZORDER);
    SetWindowPos(hPreview, NULL, rightX, editY + 38, rightW, 25, SWP_NOZORDER);

    // 拖放区
    int dropY = editY + 70;
    int dropH = h - dropY - 20;
    if (dropH < 50) dropH = 50;
    SetWindowPos(hDropZone, NULL, rightX, dropY, rightW, dropH, SWP_NOZORDER);

    // 历史记录列表
    if (hListView) SetWindowPos(hListView, NULL, rightX, 70, rightW, h - 90, SWP_NOZORDER);

    // 设置页面的单选按钮
    for (int i = 0; i < 5; ++i) {
        if (hRadioButtons[i]) SetWindowPos(hRadioButtons[i], NULL, rightX, 70 + i * 45, 400, 35, SWP_NOZORDER);
    }

    // 关于页面的占位文本（循环到20）
    for (int i = 0; i < MAX_ABOUT_ITEMS; ++i) {
        if (hAboutTexts[i]) {
            SetWindowPos(hAboutTexts[i], NULL, rightX + 20, 70 + i * 50, rightW - 40, 40, SWP_NOZORDER);
        }
    }
}

// ==================== 界面切换 ====================
void ShowSettingsView(HWND hWnd) {
    ShowWindow(hStaticName, SW_HIDE); ShowWindow(hEditName, SW_HIDE);
    ShowWindow(hStaticValue, SW_HIDE); ShowWindow(hEditValue, SW_HIDE); ShowWindow(hPreview, SW_HIDE);
    if (hListView) ShowWindow(hListView, SW_HIDE);
    ShowWindow(hDropZone, SW_HIDE);
    for (int i = 0; i < MAX_ABOUT_ITEMS; ++i) if (hAboutTexts[i]) ShowWindow(hAboutTexts[i], SW_HIDE);

    const wchar_t* radioTexts[5] = {
        L"自动加序号（如：作业_1.docx）", L"自动加日期（如：作业_20231026.docx）",
        L"直接覆盖原文件", L"跳过不处理", L"弹窗询问（像 Windows 一样）"
    };
    for (int i = 0; i < 5; ++i) {
        if (!hRadioButtons[i]) {
            hRadioButtons[i] = CreateWindowW(L"BUTTON", radioTexts[i], WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | (i == 0 ? WS_GROUP : 0), 0, 0, 0, 0, hWnd, (HMENU)(700 + i), GetModuleHandleW(NULL), NULL);
            SendMessageW(hRadioButtons[i], WM_SETFONT, (WPARAM)hFont, TRUE);
        }
        else {
            ShowWindow(hRadioButtons[i], SW_SHOW);
        }
    }
    CheckRadioButton(hWnd, 700, 704, 700 + g_conflictMode);
    LayoutControls(hWnd);
}

void ShowHistoryView(HWND hWnd) {
    ShowWindow(hStaticName, SW_HIDE); ShowWindow(hEditName, SW_HIDE);
    ShowWindow(hStaticValue, SW_HIDE); ShowWindow(hEditValue, SW_HIDE); ShowWindow(hPreview, SW_HIDE);
    for (int i = 0; i < 5; ++i) if (hRadioButtons[i]) ShowWindow(hRadioButtons[i], SW_HIDE);
    for (int i = 0; i < MAX_ABOUT_ITEMS; ++i) if (hAboutTexts[i]) ShowWindow(hAboutTexts[i], SW_HIDE);
    ShowWindow(hDropZone, SW_HIDE);

    if (!hListView) {
        hListView = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL, 0, 0, 0, 0, hWnd, (HMENU)600, GetModuleHandleW(NULL), NULL);
        ListView_SetExtendedListViewStyle(hListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        SendMessageW(hListView, WM_SETFONT, (WPARAM)hFont, TRUE);
        ListView_SetBkColor(hListView, CLR_EDIT_BG);
        ListView_SetTextColor(hListView, CLR_EDIT_TEXT);
        ListView_SetTextBkColor(hListView, CLR_EDIT_BG);

        LVCOLUMNW lvc = { 0 };
        lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        lvc.pszText = (LPWSTR)L"原文件名"; lvc.cx = 280;
        ListView_InsertColumn(hListView, 0, &lvc);
        lvc.pszText = (LPWSTR)L"新文件名"; lvc.cx = 280;
        ListView_InsertColumn(hListView, 1, &lvc);
    }
    else {
        ShowWindow(hListView, SW_SHOW);
    }
    LayoutControls(hWnd);

    ListView_DeleteAllItems(hListView);
    for (int i = 0; i < g_historyCount; ++i) {
        size_t pos = g_history[i].find(L" -> ");
        if (pos == std::wstring::npos) continue;
        std::wstring oldName = g_history[i].substr(0, pos);
        std::wstring newName = g_history[i].substr(pos + 4);
        LVITEMW lvi = { 0 };
        lvi.mask = LVIF_TEXT; lvi.iItem = i; lvi.iSubItem = 0;
        lvi.pszText = (LPWSTR)oldName.c_str();
        ListView_InsertItem(hListView, &lvi);
        ListView_SetItemText(hListView, i, 1, (LPWSTR)newName.c_str());
    }
}

void ShowAboutView(HWND hWnd) {
    if (hListView) ShowWindow(hListView, SW_HIDE);
    for (int i = 0; i < 5; ++i) if (hRadioButtons[i]) ShowWindow(hRadioButtons[i], SW_HIDE);
    ShowWindow(hStaticName, SW_HIDE); ShowWindow(hEditName, SW_HIDE);
    ShowWindow(hStaticValue, SW_HIDE); ShowWindow(hEditValue, SW_HIDE); ShowWindow(hPreview, SW_HIDE);
    ShowWindow(hDropZone, SW_HIDE);

    for (int i = 0; i < MAX_ABOUT_ITEMS; ++i) {
        if (hAboutTexts[i]) ShowWindow(hAboutTexts[i], SW_SHOW);
    }
    LayoutControls(hWnd);
}

void ShowEditView(HWND hWnd) {
    if (hListView) ShowWindow(hListView, SW_HIDE);
    for (int i = 0; i < 5; ++i) if (hRadioButtons[i]) ShowWindow(hRadioButtons[i], SW_HIDE);
    for (int i = 0; i < MAX_ABOUT_ITEMS; ++i) if (hAboutTexts[i]) ShowWindow(hAboutTexts[i], SW_HIDE);

    ShowWindow(hStaticName, SW_SHOW); ShowWindow(hEditName, SW_SHOW);
    ShowWindow(hStaticValue, SW_SHOW); ShowWindow(hEditValue, SW_SHOW); ShowWindow(hPreview, SW_SHOW);
    ShowWindow(hDropZone, SW_SHOW);

    if (g_selectedIndex >= 0 && g_selectedIndex < g_itemCount) {
        SetWindowTextW(hStaticName, L"名称:");
        SetWindowTextW(hEditName, g_items[g_selectedIndex].name.c_str());
        SetWindowTextW(hEditValue, g_items[g_selectedIndex].value.c_str());
        UpdatePreview();
    }
    LayoutControls(hWnd);
}

// ==================== 界面刷新 ====================
void UpdateName() {
    if (g_selectedIndex < 0 || g_selectedIndex >= g_itemCount) return;
    wchar_t buffer[256];
    GetWindowTextW(hEditName, buffer, 256);
    g_items[g_selectedIndex].name = buffer;
    SaveConfig();
    SetWindowTextW(hItemButtons[g_selectedIndex], buffer);
}

void UpdatePreview() {
    if (g_selectedIndex < 0 || g_selectedIndex >= g_itemCount) return;
    wchar_t buffer[256];
    GetWindowTextW(hEditValue, buffer, 256);
    g_items[g_selectedIndex].value = buffer;
    SaveConfig();
    std::wstring previewText = L"预览: ";
    for (int i = 0; i < g_itemCount; ++i) {
        if (g_items[i].type == 0) previewText += g_items[i].name + L"[" + g_items[i].value + L"] ";
        else previewText += g_items[i].name + L" ";
    }
    SetWindowTextW(hPreview, previewText.c_str());
}

void RebuildItemButtons(HWND hWnd) {
    for (int i = 0; i < MAX_ITEMS; ++i) {
        if (hItemButtons[i]) { DestroyWindow(hItemButtons[i]); hItemButtons[i] = NULL; }
    }
    for (int i = 0; i < g_itemCount; ++i) {
        hItemButtons[i] = CreateWindowW(L"BUTTON", g_items[i].name.c_str(), WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hWnd, (HMENU)(300 + i), GetModuleHandleW(NULL), NULL);
        SendMessageW(hItemButtons[i], WM_SETFONT, (WPARAM)hFont, TRUE);
        SetWindowLongPtrW(hItemButtons[i], GWLP_USERDATA, i);
        g_oldButtonProc[i] = (WNDPROC)SetWindowLongPtrW(hItemButtons[i], GWLP_WNDPROC, (LONG_PTR)ItemButtonProc);
    }
    LayoutControls(hWnd);
}

// ==================== 核心重命名 ====================
void PerformRename(HWND hWnd, std::vector<std::wstring> files) {
    if (g_itemCount == 0) { if (hWnd) MessageBoxW(hWnd, L"请先在上方添加模块！", L"提示", MB_OK); return; }
    int fileCount = files.size(), successCount = 0;

    for (int f = 0; f < fileCount; ++f) {
        std::wstring oldPath = files[f];
        size_t lastSlash = oldPath.find_last_of(L"\\/");
        std::wstring dir = (lastSlash != std::wstring::npos) ? oldPath.substr(0, lastSlash + 1) : L"";
        std::wstring fileName = (lastSlash != std::wstring::npos) ? oldPath.substr(lastSlash + 1) : oldPath;
        size_t lastDot = fileName.find_last_of(L'.');
        std::wstring ext = (lastDot != std::wstring::npos && lastDot != 0) ? fileName.substr(lastDot) : L"";
        std::wstring oldNameNoExt = (lastDot != std::wstring::npos && lastDot != 0) ? fileName.substr(0, lastDot) : fileName;

        std::wstring newName = L"";
        for (int i = 0; i < g_itemCount; ++i) {
            if (g_items[i].type == 0) {
                if (g_items[i].name == L"原名") newName += oldNameNoExt;
                else newName += g_items[i].value;
            }
            else newName += g_items[i].value;
        }
        if (fileCount > 1) newName += L" (" + std::to_wstring(f + 1) + L")";
        std::wstring newPath = dir + newName + ext;
        if (oldPath == newPath) continue;

        bool skipThisFile = false;
        if (GetFileAttributesW(newPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            if (g_conflictMode == 0) {
                int conflict = 1; std::wstring tempPath;
                do {
                    tempPath = dir + newName + L"_" + std::to_wstring(conflict) + ext;
                    conflict++;
                } while (GetFileAttributesW(tempPath.c_str()) != INVALID_FILE_ATTRIBUTES);
                newPath = tempPath;
            }
            else if (g_conflictMode == 1) {
                SYSTEMTIME st; GetLocalTime(&st);
                wchar_t dateStr[32]; swprintf_s(dateStr, L"_%04d%02d%02d", st.wYear, st.wMonth, st.wDay);
                newPath = dir + newName + dateStr + ext;
                int conflict = 1;
                while (GetFileAttributesW(newPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    newPath = dir + newName + dateStr + L"_" + std::to_wstring(conflict) + ext;
                    conflict++;
                }
            }
            else if (g_conflictMode == 2) {
                // 覆盖
            }
            else if (g_conflictMode == 3) {
                skipThisFile = true;
            }
            else if (g_conflictMode == 4) {
                std::wstring msg = L"文件已存在：\n" + newPath + L"\n\n是否覆盖？";
                int ret = MessageBoxW(hWnd, msg.c_str(), L"重名冲突", MB_YESNO | MB_ICONWARNING);
                if (ret != IDYES) skipThisFile = true;
            }
        }
        if (skipThisFile) continue;

        DWORD flags = (g_conflictMode == 2) ? MOVEFILE_REPLACE_EXISTING : 0;
        if (MoveFileExW(oldPath.c_str(), newPath.c_str(), flags)) {
            successCount++;
            AddHistory(fileName, newName + ext);
        }
    }
    SHChangeNotify(SHCNE_RENAMEITEM, SHCNF_PATHW, NULL, NULL);
    if (hWnd) {
        std::wstring msg = L"成功重命名 " + std::to_wstring(successCount) + L" / " + std::to_wstring(fileCount) + L" 个文件。";
        MessageBoxW(hWnd, msg.c_str(), L"完成", MB_OK | MB_ICONINFORMATION);
        if (hListView && IsWindowVisible(hListView)) ShowHistoryView(hWnd);
    }
}

// ==================== 按钮子类化 ====================
LRESULT CALLBACK ItemButtonProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    int idx = (int)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (idx < 0 || idx >= g_itemCount) return CallWindowProcW(g_oldButtonProc[idx], hWnd, msg, wParam, lParam);
    static POINT ptDown; static int clickedIdx = -1;

    switch (msg) {
    case WM_LBUTTONDOWN:
        SetFocus(GetParent(hWnd)); GetCursorPos(&ptDown); clickedIdx = idx;
        g_dragIndex = -1; g_dropIndex = -1; SetCapture(hWnd); return 0;
    case WM_MOUSEMOVE:
        if (GetCapture() == hWnd) {
            POINT pt; GetCursorPos(&pt);
            if (g_dragIndex == -1 && (abs(pt.x - ptDown.x) > 5 || abs(pt.y - ptDown.y) > 5)) g_dragIndex = clickedIdx;
            if (g_dragIndex != -1) SendMessageW(GetParent(hWnd), WM_USER + 1, (WPARAM)pt.x, (LPARAM)pt.y);
        }
        break;
    case WM_LBUTTONUP:
        if (GetCapture() == hWnd) {
            ReleaseCapture();
            if (g_dragIndex != -1) SendMessageW(GetParent(hWnd), WM_USER + 2, 0, 0);
            else SendMessageW(GetParent(hWnd), WM_COMMAND, MAKEWPARAM(300 + clickedIdx, BN_CLICKED), (LPARAM)hWnd);
            g_dragIndex = -1; g_dropIndex = -1; clickedIdx = -1; return 0;
        }
        break;
    }
    return CallWindowProcW(g_oldButtonProc[idx], hWnd, msg, wParam, lParam);
}

// ==================== 主窗口消息 ====================
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        HINSTANCE hInst = GetModuleHandleW(NULL);
        hFont = CreateFontW(22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
        hEditBrush = CreateSolidBrush(CLR_EDIT_BG);

        CreateWindowW(L"BUTTON", L"预览", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hWnd, (HMENU)101, hInst, NULL);
        CreateWindowW(L"BUTTON", L"历史", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hWnd, (HMENU)103, hInst, NULL);
        CreateWindowW(L"BUTTON", L"设置", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hWnd, (HMENU)102, hInst, NULL);
        CreateWindowW(L"BUTTON", L"关于", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hWnd, (HMENU)104, hInst, NULL);

        CreateWindowW(L"BUTTON", L"添加模块", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hWnd, (HMENU)200, hInst, NULL);
        CreateWindowW(L"BUTTON", L"添加分隔符", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hWnd, (HMENU)201, hInst, NULL);
        CreateWindowW(L"BUTTON", L"删除选中", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hWnd, (HMENU)202, hInst, NULL);
        CreateWindowW(L"BUTTON", L"重置所有", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hWnd, (HMENU)203, hInst, NULL);

        hStaticName = CreateWindowW(L"STATIC", L"名称:", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, NULL, hInst, NULL);
        hEditName = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 0, 0, 0, 0, hWnd, (HMENU)501, hInst, NULL);
        hStaticValue = CreateWindowW(L"STATIC", L"值:", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, NULL, hInst, NULL);
        hEditValue = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 0, 0, 0, 0, hWnd, (HMENU)500, hInst, NULL);
        hPreview = CreateWindowW(L"STATIC", L"预览: ", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, NULL, hInst, NULL);

        hInsertionLine = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_BORDER | SS_BLACKRECT, 0, 0, 0, 0, hWnd, NULL, hInst, NULL);
        ShowWindow(hInsertionLine, SW_HIDE);

        hDropZone = CreateWindowW(L"STATIC", L"下方区域：把文件拖到这里", WS_CHILD | WS_VISIBLE | SS_CENTER | WS_BORDER, 0, 0, 0, 0, hWnd, NULL, hInst, NULL);

        // =============== 关于页面内容 ===============
        // 格式：hAboutTexts[索引] = CreateWindowW(..., 文本, ...);
        // 如果有链接，就设置 g_aboutUrls[索引] = L"链接地址";

        hAboutTexts[0] = CreateWindowW(L"STATIC", L"作业重命名工具 v1.1", WS_CHILD | WS_VISIBLE | SS_CENTER, 0, 0, 0, 0, hWnd, NULL, hInst, NULL);

        hAboutTexts[1] = CreateWindowW(L"STATIC", L"作者：[水又丰]", WS_CHILD | WS_VISIBLE | SS_CENTER, 0, 0, 0, 0, hWnd, NULL, hInst, NULL);

        hAboutTexts[2] = CreateWindowW(L"STATIC", L"技术栈：C++ / Win32 API", WS_CHILD | WS_VISIBLE | SS_CENTER, 0, 0, 0, 0, hWnd, NULL, hInst, NULL);

        hAboutTexts[3] = CreateWindowW(L"STATIC", L"简介：一个极简的免安装文件重命名小工具。", WS_CHILD | WS_VISIBLE | SS_CENTER, 0, 0, 0, 0, hWnd, NULL, hInst, NULL);

        
        hAboutTexts[4] = CreateWindowW(L"STATIC", L"有问题欢迎反馈 QQ：334015073", WS_CHILD | WS_VISIBLE | SS_CENTER, 0, 0, 0, 0, hWnd, NULL, hInst, NULL);

        // GitHub 项目链接（加了 SS_NOTIFY 和特殊 ID 800+ 才能点击跳转）
        hAboutTexts[5] = CreateWindowW(L"STATIC", L"项目地址：https://github.com/sjxywo/tool_1", WS_CHILD | WS_VISIBLE | SS_CENTER | SS_NOTIFY, 0, 0, 0, 0, hWnd, (HMENU)805, hInst, NULL);
        g_aboutUrls[5] = L"https://github.com/sjxywo/tool_1"; // 点击后跳转的链接

        // 如果你还想加更多，往下继续写 hAboutTexts[6], g_aboutUrls[6] ... 最大到 19

        for (int i = 0; i < MAX_ABOUT_ITEMS; ++i) {
            if (hAboutTexts[i]) ShowWindow(hAboutTexts[i], SW_HIDE);
        }
        // =================================================================

        EnumChildWindows(hWnd, [](HWND child, LPARAM lParam) -> BOOL {
            SendMessageW(child, WM_SETFONT, (WPARAM)lParam, TRUE); return TRUE;
            }, (LPARAM)hFont);

        DragAcceptFiles(hWnd, TRUE);
        LoadConfig(); LoadHistory(); RebuildItemButtons(hWnd);
        if (g_itemCount > 0) { g_selectedIndex = 0; ShowEditView(hWnd); }
        else LayoutControls(hWnd);
        break;
    }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc; GetClientRect(hWnd, &rc);
        TRIVERTEX vert[2];
        vert[0].x = rc.left; vert[0].y = rc.top;
        vert[0].Red = GetRValue(CLR_BG_TOP) << 8; vert[0].Green = GetGValue(CLR_BG_TOP) << 8; vert[0].Blue = GetBValue(CLR_BG_TOP) << 8; vert[0].Alpha = 0;
        vert[1].x = rc.right; vert[1].y = rc.bottom;
        vert[1].Red = GetRValue(CLR_BG_BOTTOM) << 8; vert[1].Green = GetGValue(CLR_BG_BOTTOM) << 8; vert[1].Blue = GetBValue(CLR_BG_BOTTOM) << 8; vert[1].Alpha = 0;
        GRADIENT_RECT gRect = { 0, 1 };
        GradientFill(hdc, vert, 2, &gRect, 1, GRADIENT_FILL_RECT_V);
        EndPaint(hWnd, &ps);
        break;
    }
    case WM_DRAWITEM: DrawModernButton((DRAWITEMSTRUCT*)lParam); return TRUE;
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, CLR_EDIT_TEXT);
        SetBkMode(hdc, TRANSPARENT);
        return (LRESULT)GetStockObject(NULL_BRUSH);
    }
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, CLR_EDIT_TEXT);
        SetBkColor(hdc, CLR_EDIT_BG);
        return (LRESULT)hEditBrush;
    }
    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = 750; mmi->ptMinTrackSize.y = 550; return 0;
    }
    case WM_SIZE: LayoutControls(hWnd); break;
    case WM_DROPFILES: {
        HDROP hDrop = (HDROP)wParam; UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
        std::vector<std::wstring> files;
        for (UINT i = 0; i < count; ++i) { wchar_t path[MAX_PATH]; DragQueryFileW(hDrop, i, path, MAX_PATH); files.push_back(path); }
        DragFinish(hDrop); PerformRename(hWnd, files); break;
    }
    case WM_USER + 1: {
        POINT pt = { (int)wParam, (int)lParam }; ScreenToClient(hWnd, &pt);
        g_dropIndex = g_itemCount;
        for (int i = 0; i < g_itemCount; ++i) {
            if (pt.x < g_btnRects[i].left + (g_btnRects[i].right - g_btnRects[i].left) / 2) { g_dropIndex = i; break; }
        }
        int lineX = 165;
        if (g_dropIndex < g_itemCount) lineX = g_btnRects[g_dropIndex].left - 5;
        else if (g_itemCount > 0) lineX = g_btnRects[g_itemCount - 1].right + 5;
        SetWindowPos(hInsertionLine, HWND_TOP, lineX, 70, 3, 40, SWP_SHOWWINDOW);
        break;
    }
    case WM_USER + 2: {
        ShowWindow(hInsertionLine, SW_HIDE);
        if (g_dragIndex != -1 && g_dropIndex != -1) {
            if (g_dragIndex != g_dropIndex) {
                ItemData temp = g_items[g_dragIndex];
                if (g_dragIndex < g_dropIndex) for (int i = g_dragIndex; i < g_dropIndex; ++i) g_items[i] = g_items[i + 1];
                else for (int i = g_dragIndex; i > g_dropIndex; --i) g_items[i] = g_items[i - 1];
                g_items[g_dropIndex] = temp; SaveConfig(); RebuildItemButtons(hWnd); ShowEditView(hWnd);
            }
            g_dragIndex = -1; g_dropIndex = -1; return 0;
        }
        break;
    }
    case WM_COMMAND: {
        int wmId = LOWORD(wParam), wmEvent = HIWORD(wParam);
        if (wmId == 500 && wmEvent == EN_CHANGE) { UpdatePreview(); break; }
        if (wmId == 501 && wmEvent == EN_CHANGE) { UpdateName(); break; }

        if (wmId >= 700 && wmId <= 704 && wmEvent == BN_CLICKED) {
            g_conflictMode = wmId - 700; SaveConfig(); break;
        }

        if (wmId == 101) { ShowEditView(hWnd); break; }
        if (wmId == 102) { ShowSettingsView(hWnd); break; }
        if (wmId == 103) { ShowHistoryView(hWnd); break; }
        if (wmId == 104) { ShowAboutView(hWnd); break; }

        // 处理关于页面链接的点击 (ID 范围 800 ~ 819)
        if (wmId >= 800 && wmId < 800 + MAX_ABOUT_ITEMS && wmEvent == STN_CLICKED) {
            int urlIndex = wmId - 800;
            if (!g_aboutUrls[urlIndex].empty()) {
                ShellExecuteW(NULL, L"open", g_aboutUrls[urlIndex].c_str(), NULL, NULL, SW_SHOWNORMAL);
            }
            break;
        }

        if (wmId == 200) {
            if (g_itemCount >= MAX_ITEMS) { MessageBoxW(hWnd, L"最多支持 16 个模块！", L"提示", MB_OK); break; }
            g_items[g_itemCount].type = 0; g_items[g_itemCount].name = L"模块 " + std::to_wstring(g_itemCount + 1); g_items[g_itemCount].value = L"值";
            g_itemCount++; SaveConfig(); RebuildItemButtons(hWnd); g_selectedIndex = g_itemCount - 1; ShowEditView(hWnd); break;
        }
        if (wmId == 201) {
            if (g_itemCount >= MAX_ITEMS) { MessageBoxW(hWnd, L"最多支持 16 个模块！", L"提示", MB_OK); break; }
            HMENU hMenu = CreatePopupMenu(); AppendMenuW(hMenu, MF_STRING, 1, L"下划线 (_)"); AppendMenuW(hMenu, MF_STRING, 2, L"空格 ( )"); AppendMenuW(hMenu, MF_STRING, 3, L"分号 (;)");
            POINT pt; GetCursorPos(&pt);
            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, NULL);
            DestroyMenu(hMenu);
            if (cmd > 0) {
                std::wstring sepValue = L"_"; if (cmd == 2) sepValue = L" "; if (cmd == 3) sepValue = L";";
                g_items[g_itemCount].type = 1; g_items[g_itemCount].name = sepValue; g_items[g_itemCount].value = sepValue;
                g_itemCount++; SaveConfig(); RebuildItemButtons(hWnd); g_selectedIndex = g_itemCount - 1; ShowEditView(hWnd);
            }
        }
        if (wmId == 202) {
            if (g_selectedIndex < 0 || g_selectedIndex >= g_itemCount) { MessageBoxW(hWnd, L"请先点击上方的一个模块或分隔符！", L"提示", MB_OK); break; }
            if (MessageBoxW(hWnd, L"确定要删除选中的项吗？", L"删除", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                for (int i = g_selectedIndex; i < g_itemCount - 1; ++i) g_items[i] = g_items[i + 1];
                g_itemCount--; if (g_itemCount == 0) g_selectedIndex = -1; else if (g_selectedIndex >= g_itemCount) g_selectedIndex = g_itemCount - 1;
                SaveConfig(); RebuildItemButtons(hWnd); ShowEditView(hWnd);
            }
            break;
        }
        if (wmId == 203) {
            if (MessageBoxW(hWnd, L"确定要清空所有模块和分隔符吗？此操作不可逆！", L"重置", MB_YESNO | MB_ICONWARNING) == IDYES) {
                g_itemCount = 0; g_selectedIndex = -1; SaveConfig(); RebuildItemButtons(hWnd);
                SetWindowTextW(hEditName, L""); SetWindowTextW(hEditValue, L""); SetWindowTextW(hPreview, L"预览: ");
                ShowEditView(hWnd);
            }
            break;
        }
        if (wmId >= 300 && wmId < 300 + MAX_ITEMS) { int idx = wmId - 300; if (idx < g_itemCount) { g_selectedIndex = idx; ShowEditView(hWnd); } }
        break;
    }
    case WM_DESTROY:
        if (hFont) DeleteObject(hFont); if (hEditBrush) DeleteObject(hEditBrush); PostQuitMessage(0); break;
    default: return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return 0;
}

// ==================== 程序入口 ====================
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    int argc = 0; LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argc > 1) {
        std::vector<std::wstring> files;
        for (int i = 1; i < argc; ++i) files.push_back(argv[i]);
        LocalFree(argv); LoadConfig(); PerformRename(NULL, files); return 0;
    }
    LocalFree(argv);
    INITCOMMONCONTROLSEX icex; icex.dwSize = sizeof(INITCOMMONCONTROLSEX); icex.dwICC = ICC_LISTVIEW_CLASSES; InitCommonControlsEx(&icex);
    WNDCLASSW wc = {}; wc.lpfnWndProc = WndProc; wc.hInstance = hInstance; wc.lpszClassName = L"HomeworkRenamerClass"; wc.hbrBackground = NULL; RegisterClassW(&wc);
    HWND hWnd = CreateWindowExW(0, L"HomeworkRenamerClass", L"作业重命名工具", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, NULL, NULL, hInstance, NULL);
    ShowWindow(hWnd, nCmdShow); UpdateWindow(hWnd);
    MSG msg; while (GetMessageW(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return (int)msg.wParam;
}