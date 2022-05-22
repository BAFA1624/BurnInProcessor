#pragma once

#include <chrono> // for std::chrono functions

class Timer {
private:
    // Type aliases to make accessing nested type easier
    using clock_type = std::chrono::steady_clock;
    using second_type = std::chrono::duration<double, std::ratio<1> >;

    std::chrono::time_point<clock_type> m_beg;

public:
    Timer() : m_beg{ clock_type::now() } {}

    void reset() {
        m_beg = clock_type::now();
    }

    double elapsed() const {
        return std::chrono::duration_cast< second_type >(clock_type::now() - m_beg).count();
    }
};