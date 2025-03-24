#ifndef TC_HOOKS_H
#define TC_HOOKS_H

#include "pch.h"

namespace tc::mem::hooks {

    // Structures
    //------------------------------------------------------------------------
    struct hook {
        hook(std::string_view _name, void* _target, bool _enabled) : name(_name), target(_target), enabled(_enabled) {}

        std::string name;
        void* target;
        bool enabled;
    };

    // Functions
    //------------------------------------------------------------------------
    bool addHook(const std::string& name, void* target, void* detour, void** orig, bool enabled = true);
    bool removeHook(const std::string& name);

    template<typename T, typename D, typename O>
    inline bool addHook(const std::string& name, T target, D detour, O* orig, bool enabled = true) {
        return addHook(name, (void*)target, (void*)detour, (void**)orig);
    }
}

#endif // TC_HOOKS_H