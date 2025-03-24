#ifndef TC_HOOK_PRESENT_H
#define TC_HOOK_PRESENT_H

namespace td::renderer {
    using tRenderCallback = std::function<void()>;

    enum class type : uint8_t {
        opengl = 0,
        d3d12,
    };

    void hookPresent(renderer::type type);
    void addRenderCallback(const tRenderCallback& callback);
}

#endif // TC_HOOK_PRESENT_H