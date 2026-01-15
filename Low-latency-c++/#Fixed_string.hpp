#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
using namespace std;

template<size_t CAP>
class FixedString {
private:
    char buffer[CAP];
    size_t len;
public:
    FixedString() : buffer{0}, len(0) {
        buffer[0] = '\0';
    }

    inline void clear(){
        len = 0;
        buffer[0] = '\0';
    }

    inline size_t size(){ 
        return len; 
    }

    inline const char* view_string() const{
        return buffer;
    }

    inline char* edit_string(){
        return buffer;
    }

    inline bool append_char(char ch){
        if(len == CAP) return false;
        buffer[len++] = ch;
        buffer[len] = '\0';
        return true;
    }

    inline bool append(const char* s){
        if(!s) return false;
        size_t n = strlen(s);
        if(n + len >= CAP) return false;
        memcpy(buffer + len , s , n);
        len += n;
        buffer[len] = '\0';
        return true;
    }

    inline bool append_u64(uint64_t x){
        char temp[32];
        int p = 0;
        do{
            temp[p++] = '0' + (x % 10);
            x /= 10;
        }while(x);
        if(len + p >= CAP) return false;
        while(p--) buffer[len++] = temp[p];
        buffer[len] = '\0';
        return true;
    }

    inline bool append_i64(int64_t x) {
        if (x < 0) {
            if (!append_char('-')) return false;
            uint64_t ux = (uint64_t)(-(x + 1)) + 1;
            return append_u64(ux);
        }
        return append_u64(x);
    }

    inline bool append_int(int x) {
        return append_u64(x);
    }
};