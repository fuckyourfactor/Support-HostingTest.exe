#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <commctrl.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Comctl32.lib")

namespace {
constexpr int kLogId = 1003;
constexpr int kResultsId = 1001;
constexpr int kAddressId = 1002;
constexpr UINT kAppendMessage = WM_APP + 10;
constexpr UINT kFinishMessage = WM_APP + 11;
constexpr int kOriginalTestPort = 34384;
constexpr int kOriginalListenPort = 34447;
HINSTANCE gInstance = nullptr;
HBITMAP gLogo = nullptr;
HICON gIcon = nullptr;

void append(HWND dialog, int controlId, const wchar_t* message) {
    const LRESULT index = SendDlgItemMessageW(dialog, controlId, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(message));
    if (index != LB_ERR && index != LB_ERRSPACE) {
        SendDlgItemMessageW(dialog, controlId, LB_SETTOPINDEX, static_cast<WPARAM>(index), 0);
    }
}

void report(HWND dialog, int controlId, const wchar_t* message) {
    const SIZE_T bytes = static_cast<SIZE_T>(lstrlenW(message) + 1) * sizeof(wchar_t);
    wchar_t* copy = static_cast<wchar_t*>(HeapAlloc(GetProcessHeap(), 0, bytes));
    if (!copy) return;
    lstrcpyW(copy, message);
    if (!PostMessageW(dialog, kAppendMessage, static_cast<WPARAM>(controlId), reinterpret_cast<LPARAM>(copy))) {
        HeapFree(GetProcessHeap(), 0, copy);
    }
}

void reportError(HWND dialog, const wchar_t* prefix, int code) {
    wchar_t message[128];
    wsprintfW(message, L"%sWinsock error %d", prefix, code);
    report(dialog, kLogId, message);
}

bool testLocalTcpListener(HWND dialog) {
    const SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) {
        reportError(dialog, L"TCP: unable to open socket: ", WSAGetLastError());
        return false;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(kOriginalListenPort);
    bool ok = bind(listener, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0;
    if (ok) ok = listen(listener, SOMAXCONN) == 0;
    if (ok) {
        report(dialog, kLogId, L"TCP: local port 34447 available for incoming connections.");
    } else {
        reportError(dialog, L"TCP: unable to listen on port 34447: ", WSAGetLastError());
    }
    closesocket(listener);
    return ok;
}

bool testLocalUdpSocket(HWND dialog) {
    const SOCKET listener = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (listener == INVALID_SOCKET) {
        reportError(dialog, L"UDP: unable to open socket: ", WSAGetLastError());
        return false;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(kOriginalListenPort);
    const bool ok = bind(listener, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0;
    if (ok) {
        report(dialog, kLogId, L"UDP: local port 34447 available.");
    } else {
        reportError(dialog, L"UDP: unable to bind port 34447: ", WSAGetLastError());
    }
    closesocket(listener);
    return ok;
}

void testRemoteTcp(HWND dialog, DWORD ipAddress) {
    const SOCKET connection = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connection == INVALID_SOCKET) {
        report(dialog, kLogId, L"Remote TCP: socket initialization failed.");
        return;
    }
    u_long nonblocking = 1;
    ioctlsocket(connection, FIONBIO, &nonblocking);
    sockaddr_in remote{};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(kOriginalTestPort);
    remote.sin_addr.s_addr = htonl(ipAddress);
    const int result = connect(connection, reinterpret_cast<sockaddr*>(&remote), sizeof(remote));
    const int initialError = WSAGetLastError();
    bool ok = result == 0;
    if (!ok && initialError == WSAEWOULDBLOCK) {
        fd_set writable;
        FD_ZERO(&writable);
        FD_SET(connection, &writable);
        timeval timeout{3, 0};
        if (select(0, nullptr, &writable, nullptr, &timeout) > 0) {
            int error = 0;
            int length = sizeof(error);
            ok = getsockopt(connection, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &length) == 0 && error == 0;
        }
    }
    report(dialog, kLogId, ok ? L"Remote TCP: port 34384 accepted a connection." : L"Remote TCP: connection not established.");
    closesocket(connection);
}

struct TestArguments {
    HWND dialog;
    DWORD ipAddress;
};

DWORD WINAPI runTest(LPVOID context) {
    TestArguments* args = static_cast<TestArguments*>(context);
    const HWND dialog = args->dialog;
    const DWORD ipAddress = args->ipAddress;
    HeapFree(GetProcessHeap(), 0, args);
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        report(dialog, kLogId, L"Unable to initialize Winsock.");
    } else {
        report(dialog, kLogId, L"Starting Winsock");
        const bool tcp = testLocalTcpListener(dialog);
        const bool udp = testLocalUdpSocket(dialog);
        testRemoteTcp(dialog, ipAddress);
        report(dialog, kResultsId, tcp && udp ? L"Local TCP and UDP port checks succeeded." : L"Local port checks reported a problem.");
        report(dialog, kResultsId, L"The original remote-server handshake is not reconstructed.");
        WSACleanup();
    }
    PostMessageW(dialog, kFinishMessage, 0, 0);
    return 0;
}

void configureLogo(HWND dialog) {
    gLogo = static_cast<HBITMAP>(LoadImageW(gInstance, MAKEINTRESOURCEW(129), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR));
    for (HWND child = FindWindowExW(dialog, nullptr, L"Static", nullptr); child; child = FindWindowExW(dialog, child, L"Static", nullptr)) {
        const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(child, GWL_STYLE));
        if ((style & SS_TYPEMASK) == SS_BITMAP && gLogo) {
            SendMessageW(child, STM_SETIMAGE, IMAGE_BITMAP, reinterpret_cast<LPARAM>(gLogo));
            break;
        }
    }
    gIcon = static_cast<HICON>(LoadImageW(gInstance, MAKEINTRESOURCEW(128), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE));
    if (gIcon) {
        SendMessageW(dialog, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(gIcon));
        SendMessageW(dialog, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(gIcon));
    }
}

INT_PTR CALLBACK dialogProc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_INITDIALOG: {
        RECT windowRect{};
        MONITORINFO monitorInfo{sizeof(MONITORINFO)};
        if (GetWindowRect(dialog, &windowRect) &&
            GetMonitorInfoW(MonitorFromWindow(dialog, MONITOR_DEFAULTTONEAREST), &monitorInfo)) {
            const RECT& work = monitorInfo.rcWork;
            const int width = windowRect.right - windowRect.left;
            const int height = windowRect.bottom - windowRect.top;
            const int x = work.left + ((work.right - work.left) - width) / 2;
            const int y = work.top + ((work.bottom - work.top) - height) / 2;
            SetWindowPos(dialog, nullptr, x, y, 0, 0,
                         SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
        configureLogo(dialog);
        SendDlgItemMessageW(dialog, kAddressId, IPM_SETADDRESS, 0, MAKEIPADDRESS(127, 0, 0, 1));
        append(dialog, kLogId, L"Native C++ reconstruction: original dialog assets loaded.");
        append(dialog, kLogId, L"TCP/UDP checks are illustrative, not the original protocol.");
        return TRUE;
    }
    case kAppendMessage: {
        wchar_t* message = reinterpret_cast<wchar_t*>(lparam);
        if (message) {
            append(dialog, static_cast<int>(wparam), message);
            HeapFree(GetProcessHeap(), 0, message);
        }
        return TRUE;
    }
    case kFinishMessage:
        EnableWindow(GetDlgItem(dialog, 2), TRUE);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case 1:
            EndDialog(dialog, 0);
            return TRUE;
        case 2: {
            DWORD ipAddress = 0;
            if (SendDlgItemMessageW(dialog, kAddressId, IPM_GETADDRESS, 0, reinterpret_cast<LPARAM>(&ipAddress)) != 4) {
                MessageBoxW(dialog, L"Enter a complete IPv4 address.", L"Hosting Tester", MB_OK | MB_ICONWARNING);
                return TRUE;
            }
            TestArguments* args = static_cast<TestArguments*>(HeapAlloc(GetProcessHeap(), 0, sizeof(TestArguments)));
            if (!args) return TRUE;
            args->dialog = dialog;
            args->ipAddress = ipAddress;
            const HANDLE thread = CreateThread(nullptr, 0, runTest, args, 0, nullptr);
            if (!thread) {
                HeapFree(GetProcessHeap(), 0, args);
                MessageBoxW(dialog, L"Unable to start connection diagnostics.", L"Hosting Tester", MB_OK | MB_ICONERROR);
                return TRUE;
            }
            CloseHandle(thread);
            EnableWindow(GetDlgItem(dialog, 2), FALSE);
            append(dialog, kLogId, L"Starting connection diagnostics...");
            return TRUE;
        }
        }
        break;
    case WM_CLOSE:
        EndDialog(dialog, 0);
        return TRUE;
    case WM_DESTROY:
        if (gLogo) { DeleteObject(gLogo); gLogo = nullptr; }
        if (gIcon) { DestroyIcon(gIcon); gIcon = nullptr; }
        return TRUE;
    }
    return FALSE;
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    gInstance = instance;
    INITCOMMONCONTROLSEX controls{sizeof(INITCOMMONCONTROLSEX), ICC_WIN95_CLASSES | ICC_INTERNET_CLASSES};
    InitCommonControlsEx(&controls);
    HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(102), RT_RCDATA);
    if (!resource) return 1;
    HGLOBAL handle = LoadResource(instance, resource);
    if (!handle) return 1;
    auto* templateData = static_cast<LPCDLGTEMPLATEW>(LockResource(handle));
    if (!templateData) return 1;
    const INT_PTR result = DialogBoxIndirectParamW(instance, templateData, nullptr, dialogProc, 0);
    return result < 0 ? 1 : static_cast<int>(result);
}
