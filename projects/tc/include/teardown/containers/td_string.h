#ifndef TC_TEARDOWN_CONTAINERS_STRING_H
#define TC_TEARDOWN_CONTAINERS_STRING_H

#include <cstddef>
#include <stdexcept>

namespace td {

    class td_string {
    public:
        td_string();
        td_string(const char* str);
        td_string(const td_string& other);
        td_string(td_string&& other) noexcept;
        td_string& operator=(const td_string& other);
        td_string& operator=(td_string&& other) noexcept;
        ~td_string();

        td_string& operator+=(const td_string& other);
        char& operator[](size_t index);
        const char& operator[](size_t index) const;
        const char* c_str() const;
        size_t length() const;
        void substr(td_string& result, size_t pos, size_t len = static_cast<size_t>(-1)) const;

        static td_string fromInt(int value);

    private:
        union {
            char* _heap;
            char _stack[32] = { 0 };
        };

    private:
        static void init(td_string* str);
        static td_string* fromCStr(td_string* dest, const char* src);
        static void* copy(td_string* dest, const td_string* src);
        static td_string* concat(const td_string* appendStr, td_string* dest, td_string* source);
        static void ensure_capacity(td_string* str, size_t required_len);
        static void free(td_string* str);
    };

} // namespace td

#endif // TC_TEARDOWN_CONTAINERS_STRING_H