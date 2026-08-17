// Dpi.h : per-monitor DPI helpers
//
// The application manifest declares PerMonitorV2, so every hardcoded pixel
// constant in the UI has to be scaled against the DPI of the monitor the window
// is currently on, not against a fixed 96.
//
// GetDeviceCaps(LOGPIXELSX) on a *window* DC returns that window's DPI in a
// per-monitor aware process, which is the same answer GetDpiForWindow gives
// without needing a Windows 10 import. Keeping it this way lets the binary
// still load on the older systems the manifest claims support for.
/////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

namespace netwatch::util {

inline constexpr int kReferenceDpi = 96;

// DPI of the monitor hWnd is on. Falls back to the primary display when hWnd is
// not yet created.
inline int DpiForWindow(HWND hWnd) {
    HDC hdc = ::GetDC(hWnd);
    if (hdc == nullptr) {
        return kReferenceDpi;
    }
    const int dpi = ::GetDeviceCaps(hdc, LOGPIXELSX);
    ::ReleaseDC(hWnd, hdc);
    return (dpi > 0) ? dpi : kReferenceDpi;
}

// Scale a constant authored at 96 DPI.
inline int ScaleForDpi(int pixels96, int dpi) {
    return ::MulDiv(pixels96, dpi, kReferenceDpi);
}

inline int ScaleForWindow(int pixels96, HWND hWnd) {
    return ScaleForDpi(pixels96, DpiForWindow(hWnd));
}

} // namespace netwatch::util
