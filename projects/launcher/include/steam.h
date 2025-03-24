#ifndef TC_STEAM_H
#define TC_STEAM_H

#include "pch.h"

namespace tc::steam {

    bool getPath(std::wstring& outPath);
    bool getGamePath(fs::path& outPath, std::wstring_view folderName, std::wstring_view executableName);

}

#endif // TC_STEAM_H