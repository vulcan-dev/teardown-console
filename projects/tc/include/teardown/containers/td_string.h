#ifndef TC_TEARDOWN_CONTAINERS_STRING_H
#define TC_TEARDOWN_CONTAINERS_STRING_H

//__int64* __fastcall td_string::fromCStr(__int64* a1, const char* Src) {
//    const char* v2; // rsi
//    __int64 v4; // rbx
//    _BYTE* v5; // rbp
//    __int64* result; // rax
//
//    *(_BYTE*)a1 = 0;
//    v2 = Source;
//    *((_BYTE*)a1 + 31) = 0;
//    if (Src)
//        v2 = Src;
//    v4 = -1i64;
//    do
//        ++v4;
//    while (v2[v4]);
//    sub_140467710(a1, v4);
//    v5 = a1;
//    if (*((_BYTE*)a1 + 31))
//        v5 = (_BYTE*)*a1;
//    memcpy(v5, v2, (int)v4);
//    result = a1;
//    v5[(int)v4] = 0;
//    return result;
//}

namespace td {

    class td_string {
    public:
        td_string() {
            _stack[0] = 0;
            _stack[31] = 0;
        }
        td_string(const char* str) {
            size_t len = std::strlen(str);
            if (len < 32) {
                std::strncpy(_stack, str, len);
                _stack[len] = '\0';
            } else {
                _heap = new char[len + 1];
                std::strcpy(_heap, str);
                _heap[len] = '\0';
            }
        }

        ~td_string() {
            if (_stack[31] != '\0') {
                delete[] _heap;
            }
        }

        td_string& operator+=(const td_string& other) {
            size_t thisLen = length();
            size_t otherLen = other.length();
            size_t newLen = thisLen + otherLen;

            if (newLen < 32) {
                std::strcat(_stack, other.c_str());
            } else {
                char* newHeap = new char[newLen + 1];
                std::strcpy(newHeap, c_str());
                std::strcat(newHeap, other.c_str());
            
                if (_stack[31] != '\0') {
                    delete[] _heap;
                }
                _heap = newHeap;
                _stack[31] = '\1';
            }

            return *this;
        }

        const char* c_str() const {
            return _stack[31] != '\0' ? _heap : _stack;
        }

        size_t length() const {
            return std::strlen(c_str());
        }

    private:
        union {
            char* _heap;
            char _stack[32] = { 0 };
        };
    };

}

#endif // TC_TEARDOWN_CONTAINERS_STRING_H