#include "pch.h"
#include "teardown/containers/td_containers.h"
#include "memory/memory.h"
#include "teardown/types.h"
#include "offsets_generated.h"

using namespace td;
using namespace tc;

void td_string::init(td_string* str) {
    str->_stack[0] = 0;
    str->_stack[31] = 0;
}

td_string* td_string::fromCStr(td_string* dest, const char* src) {
    init(dest);

    if (!src) src = "";
    size_t len = strlen(src);

    ensure_capacity(dest, len);

    char* dest_ptr = dest->_stack[31] ? dest->_heap : dest->_stack;
    memcpy(dest_ptr, src, len);
    dest_ptr[len] = 0;

    return dest;
}

void* td_string::copy(td_string* dest, const td_string* src) {
    const char* src_ptr = src->_stack[31] ? src->_heap : src->_stack;

    size_t len = strlen(src_ptr);
    ensure_capacity(dest, len);

    char* dest_ptr = dest->_stack[31] ? dest->_heap : dest->_stack;
    memcpy(dest_ptr, src_ptr, len);
    dest_ptr[len] = 0;

    return dest;
}

td_string* td_string::concat(const td_string* appendStr, td_string* dest, td_string* source) {
    init(dest);

    const char* src_ptr = source->_stack[31] ? source->_heap : source->_stack;
    const char* append_ptr = appendStr->_stack[31] ? appendStr->_heap : appendStr->_stack;

    size_t src_len = strlen(src_ptr);
    size_t append_len = strlen(append_ptr);

    ensure_capacity(dest, src_len + append_len);

    char* dest_ptr = dest->_stack[31] ? dest->_heap : dest->_stack;

    memcpy(dest_ptr, src_ptr, src_len);
    memcpy(dest_ptr + src_len, append_ptr, append_len + 1);  // +1 for null terminator

    return dest;
}

void td_string::ensure_capacity(td_string* str, size_t required_len) {
    if (required_len < 32) {
        // String fits in stack buffer
        return;
    }

    if (!str->_stack[31]) {
        // Currently on stack, move to heap
        char* new_heap = (char*)funcs::mem::alloc(required_len + 1);
        memcpy(new_heap, str->_stack, 32);
        str->_heap = new_heap;
        str->_stack[31] = 1;
    } else {
        // Already on heap, resize if needed
        char* new_heap = (char*)funcs::mem::alloc(required_len + 1);
        memcpy(new_heap, str->_heap, strlen(str->_heap) + 1);
        funcs::mem::free(str->_heap);
        str->_heap = new_heap;
    }
}

void td_string::free(td_string* str) {
    if (str->_stack[31])
        funcs::mem::free(str->_heap);
}

td_string::td_string() {
    init(this);
}

td_string::td_string(const char* str) {
    fromCStr(this, str);
}

td_string::td_string(const td_string& other) {
    init(this);
    copy(this, &other);
}

td_string::td_string(td_string&& other) noexcept {
    init(this);
    if (other._stack[31]) {
        _heap = other._heap;
        _stack[31] = 1;
        other._heap = nullptr;
        other._stack[31] = 0;
    } else {
        memcpy(_stack, other._stack, 32);
    }
}

td_string& td_string::operator=(const td_string& other) {
    if (this != &other) {
        free(this);
        copy(this, &other);
    }
    return *this;
}

td_string& td_string::operator=(td_string&& other) noexcept {
    if (this != &other) {
        free(this);
        if (other._stack[31]) {
            _heap = other._heap;
            _stack[31] = 1;
            other._heap = nullptr;
            other._stack[31] = 0;
        } else {
            memcpy(_stack, other._stack, 32);
        }
    }
    return *this;
}

td_string::~td_string() {
    free(this);
}

td_string& td_string::operator+=(const td_string& other) {
    td_string temp;
    concat(&other, &temp, this);
    *this = std::move(temp);
    return *this;
}

char& td_string::operator[](size_t index) {
    return _stack[31] ? _heap[index] : _stack[index];
}

const char& td_string::operator[](size_t index) const {
    return _stack[31] ? _heap[index] : _stack[index];
}

const char* td_string::c_str() const {
    return _stack[31] ? _heap : _stack;
}

size_t td_string::length() const {
    return strlen(c_str());
}

void td_string::substr(td_string& result, size_t pos, size_t len) const {
    const size_t str_len = length();
    if (pos > str_len)
        throw std::out_of_range("Position out of range");

    const size_t actual_len = (len == static_cast<size_t>(-1) || pos + len > str_len)
        ? (str_len - pos)
        : len;

    if (actual_len == 0) {
        init(&result);
        return;
    }

    const char* src = c_str() + pos;
    char* buffer = (char*)funcs::mem::alloc(actual_len + 1);
    memcpy(buffer, src, actual_len);
    buffer[actual_len] = '\0';

    fromCStr(&result, buffer);
    funcs::mem::free(buffer);
}

td_string td_string::fromInt(int value) {
    char buffer[13];
    sprintf(buffer, "%d", value);
    return td_string(buffer);
}
