#ifndef TC_HOOK_PRESENT_H
#define TC_HOOK_PRESENT_H

namespace td::renderer {
    void onRender();

    enum class type : uint8_t {
        opengl = 0,
        d3d12,
    };

    void hookPresent();
}

#endif // TC_HOOK_PRESENT_H