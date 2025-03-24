#ifndef TC_TEARDOWN_H
#define TC_TEARDOWN_H

#include "pch.h"
#include "memory/memory.h"
#include "teardown/types.h"

namespace tc::teardown {
    void initialize();

    static types::game_t* game = nullptr;
}

#endif // TC_TEARDOWN_H