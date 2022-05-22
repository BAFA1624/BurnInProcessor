#pragma once

#ifdef PRINT_DEBUG
#include <iostream>
#include <string>
inline void
print(const std::string& msg, const int &v_level) {
    bool test = (v_level <= PRINT_DEBUG || PRINT_DEBUG < 0);
    switch ( test ) {
    case 0:
        break;
    case 1:
        std::cout << std::string(v_level, '\t') << msg << std::endl;
        break;
    }
    return;
}
#else
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include <string>
inline void
print(const std::string& msg, const int &v_level) {
    return;
}
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
#endif