#ifndef TC_UTIL_H
#define TC_UTIL_H

#include "pch.h"
#include "shared/util/util.h"

namespace tc::util {

    static void copyToClipboard(const std::string& text, const std::wstring& title = L"") {
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.length() + 1);
        if (!hMem) {
            tc::util::displayLastError(L"copyToClipboard failed, could not allocate memory");
            return;
        }

        LPVOID locked = GlobalLock(hMem);
        if (!locked) {
            tc::util::displayLastError(L"Could not lock memory in copyToClipboard");
            GlobalFree(hMem);
            return;
        }

        memcpy(locked, text.data(), text.length() + 1);
        GlobalUnlock(hMem);
        OpenClipboard(0);
        EmptyClipboard();
        SetClipboardData(CF_TEXT, hMem);
        CloseClipboard();

        GlobalFree(hMem);

        if (!title.empty()) {
            tc::util::displayMessage(MB_OK, title);
        }
    }

}

#endif // TC_UTIL_H