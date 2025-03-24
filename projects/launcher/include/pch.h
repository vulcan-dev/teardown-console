#ifndef TC_PCH_H
#define TC_PCH_H

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>

#include <filesystem>
#include <fstream>
#include <format>

#include <cstdio>
#include <stdexcept>

#include <string_view>
#include <string>

#include <numeric>
#include <utility>
#include <vector>
#include <map>

#include "argparse/argparse.hpp"

namespace fs = std::filesystem;

#endif // TC_PCH_H