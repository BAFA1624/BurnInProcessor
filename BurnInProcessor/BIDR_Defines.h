#pragma once

// C++ std library files
#include <bitset>
#include <chrono>
#include <concepts>
#include <functional>
#include <limits>
#include <numeric>
#include <string>
#include <tuple>
#include <typeinfo>
#include <type_traits>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>
#include <execution>
#include <filesystem>
#include <fstream>
#include <format>
#include <iostream>
#include <ranges>
#include <regex>
#include <locale>
#include <codecvt>

#include "nlohmann/json.hpp"

#ifdef _DEBUG
#define BIDR_DEBUG
#endif

#ifdef _WIN64
using integer = int64_t;
using uinteger = uint64_t;
constexpr auto fmt "%lld"
constexpr auto wfmt L"%lld"
#else
using integer = int32_t;
using uinteger = uint32_t;
constexpr auto fmt = "%d";
constexpr auto wfmt = L"%d";
#endif

// Ensure 64-bit double
constexpr size_t double_bits = sizeof(double) * CHAR_BIT;
static_assert( double_bits == 64 );
static_assert( std::is_signed_v<time_t> );

#ifdef DEBUG
inline std::string log_location {
    R"(C:\Users\AndrewsBe\Documents\BurnInProcessor\Debug\)"
};
#else
inline std::string log_location {
    R"(C:\Users\AndrewsBe\Documents\BurnInProcessor\Release\)"
};
#endif

inline std::filesystem::path log_path {
    log_location + "BurnInLog.txt"
};

inline std::filesystem::path err_log_path { log_location };

inline void WINAPI
clear_err_log() {
    std::fstream err_file(err_log_path, std::ios_base::out | std::ios_base::trunc);
    err_file.close();
}
inline void WINAPI
write_err_log( const std::exception& err,
               const std::string_view msg="" ) {
    std::fstream err_file(err_log_path,
                    std::ios_base::out | std::ios_base::app);
    const auto extra_msg = (msg == "") ? "" : std::format("{}\n", msg);
    err_file << std::format("{}INTERNAL ERROR:\n{}\n",
                             extra_msg, err.what() );
    err_file.close();
}

inline void WINAPI
clear_log() {
    std::fstream log_file( log_path,
                     std::ios_base::out | std::ios_base::trunc );
    log_file.close();
}

inline void WINAPI
write_log( const std::string_view str ) {
    std::fstream log_file( log_path,
                            std::ios_base::out | std::ios_base::app );
    log_file << str << "\n";
    log_file.close();
}
inline void WINAPI
write_log( const std::wstring_view w_str ) {
    std::wfstream w_log_file( log_path,
                             std::ios_base::out | std::ios_base::app );
    w_log_file << w_str << L"\n";
    w_log_file.close();
}

#undef max
#undef min

namespace burn_in_data_report
{
    using namespace std::chrono_literals;

    using FileFormat = std::pair<std::string, nlohmann::json>;

    constexpr std::size_t SIZE_T_MAX      = std::numeric_limits<std::size_t>::max();
    constexpr std::size_t MAX_COLS        = 16384;       // Defined in Excel specifications
    constexpr std::size_t MAX_ROWS        = 1048576;     // Defined in Excel specifications
    constexpr std::size_t MAX_LINE_LENGTH = 1024; // Max line length in file

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

    template <typename T1, typename T2>
    using mul = std::ratio_multiply<T1, T2>;
    using nano = std::chrono::duration<std::int64_t, std::nano>;
    using nanocentury = std::chrono::duration<std::int64_t, mul<std::hecto, std::nano>>;
    using micro = std::chrono::duration<std::int64_t, std::micro>;
    using second = std::chrono::duration<std::int64_t>;
    using milli = std::chrono::duration<std::int64_t, std::milli>;
    using kilo = std::chrono::duration<std::int64_t, std::kilo>;

    using dnano = std::chrono::duration<long double, std::nano>;
    using dnanocentury = std::chrono::duration<long double, mul<std::hecto, std::nano>>;
    using dmicro = std::chrono::duration<long double, std::micro>;

    // Vector of { column title : data vector } pairs
    template <typename T>
    using TMap = std::unordered_map<std::string, std::vector<T>>;
    using IMap = std::unordered_map<std::string, std::vector<integer>>;
    using DMap = std::unordered_map<std::string, std::vector<double>>;
    using SMap = std::unordered_map<std::string, std::vector<std::string>>;

    using range_t = std::pair<uinteger, uinteger>;
    using indices_t = std::vector<range_t>;

    template <typename T>
    std::vector<T>
    VSlice( const std::vector<T>& _v, const uinteger& _a,
            const uinteger&       _b ) { return std::vector<T> { _v.begin() + _a, _v.begin() + _b }; }


    enum class DataType
    {
        INTEGER,  // int64_t
        DOUBLE,   // double
        STRING,   // std::string
        /*DATETIME, // double <- Excel DateTime has an underlying type of 64-bit
        // floating point*/
        NONE      // Mostly for errors, not sure if it's needed
    };          // Enum mapping descriptive type name to 0, 1, 2, 3
    const std::map<DataType, std::string> type_string {
            { DataType::INTEGER, "INT" },
            { DataType::DOUBLE, "DOUBLE" },
            { DataType::STRING, "STRING" },
            //{ DataType::DATETIME, "DATETIME" },
            { DataType::NONE, "NONE" }
        };
    using TypeMap = std::unordered_map<std::string, DataType>;
    // Vector of { column title : DataType } pairs <-- In correct column order
    using TVec = std::vector<std::pair<std::string, DataType>>;

    inline void
    memcpy_checked( void* dst, const void* src, const uinteger& sz ) {
        switch ( (sz < SIZE_T_MAX) ? 0 : 1 ) {
        case 0:
            std::memcpy(dst, src, sz);
            break;
        case 1:
            std::runtime_error err("DLL: <memcpy_checked> sz > SIZE_T_MAX");
            write_err_log( err );
            break;
        }
    }

    inline uinteger
    access_checked( const uinteger& idx ) {
        return idx;
    }

    inline std::size_t
    access_range_checked( const uinteger& idx,
                          const uinteger& min,
                          const uinteger& max ) {
        const std::size_t s_idx = access_checked(idx);
        const std::size_t s_min = access_checked(min);
        const std::size_t s_max = access_checked(max);
        if ( s_min <= s_idx && s_max >= s_idx ) {
            return s_idx;
        }
        else {
            std::string msg =
                std::format("DLL: <access_range_checked> Invalid index. min: {}, max: {}, index: {}",
                            s_min, s_max, s_idx );
            write_err_log( std::runtime_error(msg) );
            return s_min;
        }
    }

    template <typename T>
    concept ArithmeticType = std::integral<T> || std::floating_point<T>;

    inline const auto
    int64_t_hash = typeid(integer).hash_code();
    inline const auto
    double_hash  = typeid(double).hash_code();
    inline const auto
    string_hash  = typeid(std::string).hash_code();

    const std::unordered_map<integer, DataType> hash_to_DataType {
            { int64_t_hash, DataType::INTEGER },
            { double_hash, DataType::DOUBLE },
            { string_hash, DataType::STRING }
        };
    const std::unordered_map<DataType, integer> DataType_to_hash {
            { DataType::INTEGER, int64_t_hash },
            { DataType::DOUBLE, double_hash },
            { DataType::STRING, string_hash }
        };


    class median_of_empty_list_exception final : public std::exception
    {
        [[nodiscard]] const char*
        what() const throw() override { return "Attempt to take the median of an empty list --> Undefined."; }
    };


    class median_invalid_iterators final : public std::exception
    {
        const char*
        what() const throw() override {
            return "Attempt to take the median using invalid iterators. E.g _start > "
                "_end, etc.";
        }
    };


/*
    static double
    median( const std::vector<integer>& _v ) {
        if ( _v.empty() )
            throw median_of_empty_list_exception();

        auto tmp = _v;

        const auto n = _v.size() / 2;
        std::ranges::nth_element(tmp.begin(), tmp.begin() + n, tmp.end());
        double med = static_cast<double>(tmp[n]);

        if ( !(_v.size() & 1) ) { // _v.size() == even number
            const auto max_it = std::max_element(tmp.begin(), tmp.begin() + n);
            med = (*max_it + med) / 2.0;
        }

        return med;
    }
    static double
    median( const std::vector<double>& _v ) {
        if ( _v.empty() )
            throw median_of_empty_list_exception();

        auto tmp = _v;

        const auto n = _v.size() / 2;
        std::ranges::nth_element(tmp.begin(), tmp.begin() + n, tmp.end());
        double med = tmp[n];

        if ( !(_v.size() & 1) ) { // _v.size() == even number
            const auto max_it = std::max_element(tmp.begin(), tmp.begin() + n);
            med = (*max_it + med) / 2.0;
        }

        return med;
    }

    static double
    median( const std::vector<integer>&  _data, const uinteger& _first,
            const uinteger& _end ) {
        if ( _data.empty() || (_end - _first + 1 == 0) )
            throw median_of_empty_list_exception();

        auto tmp = _data;

        const auto n = (_end - _first) / 2;
        std::ranges::nth_element(tmp.begin() + _first, tmp.begin() + _first + n,
                         tmp.begin() + _end);
        double med = tmp[n];

        if ( !((_end - _first + 1) & 1) ) {
            const auto max_it =
                std::max_element(tmp.begin() + _first, tmp.begin() + _first + n);
            med = (*max_it + med) / 2.0;
        }

        return med;
    }
    static double
    median( const std::vector<double>&  _data, const uinteger& _first,
            const uinteger& _end ) {
        if ( _data.empty() || (_end - _first + 1 == 0) )
            throw median_of_empty_list_exception();

        auto tmp = _data;

        const auto n = (_end - _first) / 2;
        std::ranges::nth_element( tmp.begin() + _first, tmp.begin() + _first + n,
                             tmp.begin() + _end);
        double med = tmp[n];

        if ( !((_end - _first + 1) & 1) ) {
            const auto max_it =
                std::max_element(tmp.begin() + _first, tmp.begin() + _first + n);
            med = (*max_it + med) / 2.0;
        }

        return med;
    }
*/

    template <ArithmeticType T>
    static double
    median( const std::vector<T>&  _data,
            const uinteger& _first,
            const uinteger& _end,
            const std::vector<double>& stdevs) {
        if ( _data.empty() || _end - _first + 1 == 0 ) { throw median_of_empty_list_exception(); }


        auto tmp = _data;

        const auto n = (_end - _first) / 2;
        std::ranges::nth_element(tmp.begin() + _first, tmp.begin() + _first + n,
                         tmp.begin() + _end);
        double med = tmp[n];

        if ( !(_end - _first + 1 & 1) ) {
            const auto max_it =
                std::max_element(tmp.begin() + _first, tmp.begin() + _first + n);
            med = (*max_it + med) / 2.0;
        }

        write_log(std::format("{}", med));
        return med;
    }

    /*
    static double
    median( const std::vector<double>&  _data,
            const uinteger& _first,
            const uinteger& _end,
            const std::vector<double>& stdevs) {
        if ( _data.empty() || (_end - _first + 1 == 0) )
            throw median_of_empty_list_exception();

        auto tmp = _data;

        const auto n = (_end - _first) / 2;
        std::ranges::nth_element(tmp.begin() + _first, tmp.begin() + _first + n,
                         tmp.begin() + _end);
        double med = tmp[n];

        if ( !((_end - _first + 1) & 1) ) {
            const auto max_it =
                std::max_element(tmp.begin() + _first, tmp.begin() + _first + n);
            med = (*max_it + med) / 2.0;
        }

        return med;
    }*/

    template <typename T>
    const std::function<void( const T& )>
    default_print =
        []( const T& arg ) { std::cout << arg; };

    template <typename T>
    void
    print_vec( const std::vector<T>&                 _v,
               const std::function<void( const T& )> print_func =
                   default_print<T>,
               const std::string_view& delimiter = " " ) {
        for ( auto& val : _v ) {
            print_func(val);
            std::cout << delimiter;
        }
        std::cout << std::endl;
    }

/*
    static double
    mean( const std::vector<integer>& _v ) {
        integer sum = std::accumulate(_v.cbegin(), _v.cend(), static_cast<integer>( 0 ));
        return ( !_v.empty() )
                   ? static_cast<double>( sum / _v.size() )
                   : 0.0;
    }
    static double
    mean( const std::vector<double>& _v ) {
        double sum = std::accumulate(_v.cbegin(), _v.cend(), static_cast<double>( 0 ));
        return ( !_v.empty() )
                   ? sum / _v.size()
                   : 0.0;
    }

    static double
    mean( const std::vector<integer>::const_iterator _first,
          const std::vector<integer>::const_iterator _last ) {
        if ( _first == _last ) {
            const auto msg =
                std::format("DLL: <mean> Invalid iterators received, _first == _last.");
            write_err_log( std::runtime_error(msg) );
            return 0;
        }

        const integer size = _last - _first;
        const integer sum  = std::accumulate(_first, _last, static_cast<integer>( 0 ));
        return (size > 0)
                   ? static_cast<double>( sum ) / static_cast<double>( size )
                   : 0.0;
    }
    static double
    mean( const std::vector<double>::const_iterator _first,
          const std::vector<double>::const_iterator _last ) {
        if ( _first == _last ) {
            const auto msg =
                std::format("DLL: <mean> Invalid iterators received, _first == _last.");;
            write_err_log( std::runtime_error(msg) );
            return 0;
        }

        const integer size = _last - _first;
        const double sum  = std::accumulate(_first, _last, static_cast<double>( 0 ));
        return (size > 0)
                   ? sum / static_cast<double>( size )
                   : 0.0;
    }

    static double
    mean( const std::vector<integer>& _data, const uinteger& _start,
          const uinteger&       _end ) {
#ifdef DEBUG
        if ( !(_start <= _end && _data.size() >= _end - _start) ) {
            print_vec( VSlice<integer>( _data, _start, _end ), default_print<integer> );
        }
#else
        assert( _start <= _end && _data.size() >= _end - _start );
#endif
        integer sum = std::accumulate(_data.begin() + _start, _data.begin() + _end, static_cast<integer>( 0 ));
        return (_end - _start > 0)
                   ?  static_cast<double>(sum) / static_cast<double>( _end - _start )
                   : 0.0;
    }
    static double
    mean( const std::vector<double>& _data, const uinteger& _start,
          const uinteger&       _end ) {
#ifdef DEBUG
        if ( !(_start <= _end && _data.size() >= _end - _start) ) {
            print_vec( VSlice<double>( _data, _start, _end ), default_print<double> );
        }
#else
        assert( _start <= _end && _data.size() >= _end - _start );
#endif
        double sum = std::accumulate(_data.begin() + _start, _data.begin() + _end, 0. );
        return (_end - _start > 0)
                   ?  sum / static_cast<double>( _end - _start )
                   : 0.0;
    }
*/

    template <ArithmeticType T>
    static double
    mean( const std::vector<T>& _data, const uinteger&           _start,
          const uinteger&       _end, const std::vector<double>& _stdevs ) {
        if ( _start > _end || _end > _data.size() ) {
            write_err_log(
                std::runtime_error("<mean> Invalid start/end parameters received.")
            );
            return std::numeric_limits<double>::signaling_NaN();
        }
        if ( _stdevs.empty() ) {
            const auto x =
                static_cast<double>(
                    std::accumulate(
                        _data.cbegin() + _start, _data.cbegin() + _end,
                        static_cast<T>(0),
                        [](const T lhs, const T& rhs)
                        { return lhs + rhs; }
                    )
                ) / (_end - _start);
            return x;
        }
        assert(_data.size() == _stdevs.size());

        const double sum_weights =
            std::accumulate(_stdevs.begin() + _start, _stdevs.begin() + _end, 0. );
        double sum_weights_means { 0. };
        for ( uinteger i { _start }; i < _end; ++i )
            sum_weights_means += (_stdevs[i] * static_cast<double>(_data[i]));
        return (_end - _start > 0)
                   ? sum_weights_means / sum_weights
                   : 0.;
    }

    /*
    static double
    mean( const std::vector<double>& _data, const uinteger& _start,
          const uinteger& _end, const std::vector<double>& _stdevs ) {
        double result { 0. };
        if ( _stdevs.empty() )
            return mean(_data, _start, _end);
        assert(_data.size() == _stdevs.size());

        const double sum_weights =
            std::accumulate(_stdevs.begin() + _start, _stdevs.begin() + _end, 0.);
        double sum_weights_means { 0. };
        for ( uinteger i { _start }; i < _end; ++i )
            sum_weights_means += (_stdevs[i] * _data[i]);
        return (_end - _start > 0)
                   ? sum_weights_means / sum_weights
                   : 0.;
    }*/

/*
    static double
    mean_debug( const std::vector<integer>& _v ) {
        std::cout << "mean_debug: _v.size() = " << _v.size() << std::endl;
        std::cout << "            typeid(T).name() = int64_t" << std::endl;
        integer sum = std::accumulate(_v.cbegin(), _v.cend(), static_cast<integer>( 0 ));
        std::cout << "            sum = " << sum << std::endl;
        std::cout << "            result = " << static_cast<double>( sum / _v.size() ) << std::endl;
        return static_cast<double>(sum) / static_cast<double>(_v.size());
    }
    static double
    mean_debug( const std::vector<double>& _v ) {
        std::cout << "mean_debug: _v.size() = " << _v.size() << std::endl;
        std::cout << "            typeid(T).name() = double" << std::endl;
        double sum = std::accumulate(_v.cbegin(), _v.cend(), 0. );
        std::cout << "            sum = " << sum << std::endl;
        std::cout << "            result = " << sum / _v.size() << std::endl;
        return sum / static_cast<double>(_v.size());
    }

    static double
    mean_debug( const std::vector<integer>::const_iterator _first,
                const std::vector<integer>::const_iterator _last ) {
        if ( _first == _last ) {
            const auto msg =
                std::format("DLL: <mean_debug> Invalid iterators received, _first == _last.");
            write_err_log( std::runtime_error(msg) );
            return 0;
        }

        const uinteger size = _last - _first;
        const integer sum  = std::accumulate(_first, _last, static_cast<integer>( 0 ));
        std::cout << "mean_debug: size = " << size << std::endl;
        std::cout << "            typeid(T).name() = int64_t" << std::endl;
        std::cout << "            sum = " << sum << std::endl;
        std::cout << "            result = " << static_cast<double>( sum ) / static_cast<double>( size ) << std::endl;
        return  static_cast<double>(sum) / static_cast<double>( size );
    }
    static double
    mean_debug( const std::vector<double>::const_iterator _first,
                const std::vector<double>::const_iterator _last ) {
        if ( _first == _last ) {
            write_err_log(
                std::runtime_error("DLL: <mean_debug> Invalid iterators received, _first == _last.")
            );
            return 0;
        }

        const uinteger size = _last - _first;
        const double sum  = std::accumulate(_first, _last, static_cast<double>( 0 ));
        std::cout << "mean_debug: size = " << size << std::endl;
        std::cout << "            typeid(T).name() = int64_t" << std::endl;
        std::cout << "            sum = " << sum << std::endl;
        std::cout << "            result = " << sum / static_cast<double>( size ) << std::endl;
        return sum / static_cast<double>(size);
    }

    static double
    mean_debug( const std::vector<integer>& _data, const uinteger _start,
                const uinteger&       _end ) {
        assert(_start <= _end && _data.size() >= _end - _start);
        const integer sum = std::accumulate(_data.begin() + _start, _data.begin() + _end,
                                static_cast<integer>( 0 ));
        std::cout << "mean_debug: size = " << _end - _start << std::endl;
        std::cout << "            typeid(T).name() = int64_t" << std::endl;
        std::cout << "            sum = " << sum << std::endl;
        std::cout << "            result = " << static_cast<double>( sum ) / static_cast<double>( _end - _start ) <<
            std::endl;
        return static_cast<double>( sum ) / static_cast<double>( _end - _start );
    }
    static double
    mean_debug( const std::vector<double>& _data, const uinteger _start,
                const uinteger&       _end ) {
        assert(_start <= _end && _data.size() >= _end - _start);
        const double sum = std::accumulate(_data.begin() + _start, _data.begin() + _end, 0. );
        std::cout << "mean_debug: size = " << _end - _start << std::endl;
        std::cout << "            typeid(T).name() = double" << std::endl;
        std::cout << "            sum = " << sum << std::endl;
        std::cout << "            result = " <<
            sum / static_cast<double>( _end - _start ) << std::endl;
        return sum / static_cast<double>( _end - _start );
    }

    static double
    mean_debug( const std::vector<integer>& _data, const uinteger& _start,
                const uinteger& _end, const std::vector<double>& _stdevs ) {
        double result { 0. };
        if ( _stdevs.empty() )
            return mean_debug(_data, _start, _end);
        assert(_start < _end&& _start >= 0 && _end <= _data.size());
        assert((_data.size() == _stdevs.size()) ||
               (_stdevs.size() >= _end));
        std::cout << "mean_debug: size = " << _end - _start << std::endl;
        std::cout << "            typeid(T).name() = int64_t" << std::endl;
        double sum_weights =
            std::accumulate(_stdevs.begin() + _start, _stdevs.begin() + _end, 
                             0. );
        std::cout << "            sum_weights = " << sum_weights << std::endl;
        double sum_weights_means { 0. };
        for ( uinteger i { _start }; i < _end; ++i )
            sum_weights_means += (_stdevs[i] * static_cast<double>(_data[i]));
        std::cout << "            sum_weights_means = " << sum_weights_means << std::endl;
        std::cout << "            result = " << sum_weights_means / sum_weights << std::endl;
        return sum_weights_means / sum_weights;
    }
    static double
    mean_debug( const std::vector<double>& _data, const uinteger& _start,
                const uinteger& _end, const std::vector<double>& _stdevs ) {
        double result { 0. };
        if ( _stdevs.empty() )
            return mean_debug(_data, _start, _end);
        assert(_start < _end&& _start >= 0 && _end <= _data.size());
        assert((_data.size() == _stdevs.size()) ||
               (_stdevs.size() >= _end));
        std::cout << "mean_debug: size = " << _end - _start << std::endl;
        std::cout << "            typeid(T).name() = double" << std::endl;
        double sum_weights =
            std::accumulate(_stdevs.begin() + _start, _stdevs.begin() + _end,
                            0. );
        std::cout << "            sum_weights = " << sum_weights << std::endl;
        double sum_weights_means { 0. };
        for ( uinteger i { _start }; i < _end; ++i )
            sum_weights_means += (_stdevs[i] * _data[i]);
        std::cout << "            sum_weights_means = " << sum_weights_means << std::endl;
        std::cout << "            result = " << sum_weights_means / sum_weights << std::endl;
        return sum_weights_means / sum_weights;
    }
*/

    static range_t
    stable_period_convert( const uinteger _start, const uinteger _last ) {
        try {
            const uinteger midpoint            = std::midpoint(_start, _last);
            const uinteger quarter_point       = std::midpoint(_start, midpoint);
            const uinteger three_quarter_point = std::midpoint(midpoint, _last);
            return (quarter_point != three_quarter_point)
                       ? std::pair { quarter_point, three_quarter_point }
                       : std::pair { _start, _last };
        }
        catch ( const std::exception& err ) {
            write_err_log( err, "<stable_period_convert>" );
            return { _start, _last };
        }
    }

/*
    static range_t
    stable_period_convert_debug( const uinteger _start, const uinteger _last ) {
        std::cout << "stable_period_convert: _start = " << _start
            << ", _last = " << _last << std::endl;
        bool     test                = _start <= _last;
        uinteger midpoint            = std::midpoint(_start, _last);
        uinteger quarter_point       = std::midpoint(_start, midpoint);
        uinteger three_quarter_point = std::midpoint(midpoint, _last);
        std::cout << "                       midpoint = " << midpoint << std::endl;
        std::cout << "                       three_quarter_point  = " << three_quarter_point << std::endl;
        range_t result { quarter_point, three_quarter_point };
        range_t fallback_result { _start, _last };
        std::cout << "                       result = (" << result.first << ", " << result.second << ")" << std::endl;
        std::cout << "                       fallback_result = (" << fallback_result.first << ", " << fallback_result.
            second << ")" << std::endl;
        return (quarter_point != three_quarter_point)
                   ? result
                   : fallback_result;
    }
*/

/*
    static double
    stdev( const std::vector<integer>& _v, const double& _mean,
           const int& _ddof = 0 ) {
        double sum = 0;
        for ( const auto& val : _v )
            sum += (static_cast<double>( val ) - _mean)
                     * (static_cast<double>(val) - _mean);
        sum /= _v.size() - _ddof;
        return sqrt(sum);
    }
    static double
    stdev( const std::vector<double>& _v, const double& _mean,
           const int& _ddof = 0 ) {
        double sum = 0;
        for ( const auto& val : _v )
            sum += (val - _mean) * (val - _mean);
        sum /= _v.size() - _ddof;
        return sqrt(sum);
    }

    static double
    stdev( const std::vector<integer>::const_iterator _first,
           const std::vector<integer>::const_iterator _last,
           const double& _mean, const int& _ddof = 0 ) {
        assert( _first <= _last );
        double sum = 0;
        for ( auto i = _first; i != _last; ++i ) {
            sum += (*i - _mean)
                * (*i - _mean);
        }
        sum /= (_last - _first) - _ddof; // sum / ((size) - ddof)
        return sqrt(sum);
    }
    static double
    stdev( const std::vector<double>::const_iterator _first,
           const std::vector<double>::const_iterator _last,
           const double& _mean, const int& _ddof = 0 ) {
        assert( _first <= _last );
        double sum = 0;
        for ( auto i = _first; i != _last; ++i ) {
            sum += (*i - _mean)
                * (*i - _mean);
        }
        sum /= (_last - _first) - _ddof; // sum / ((size) - ddof)
        return sqrt(sum);
    }
    template <ArithmeticType T>
    static double
    stdev( const std::vector<T>& _data, const uinteger& _start,
           const uinteger& _end, const double& _mean,
           const int& _ddof = 0 ) {
        if ( _start > _end || _end >= _data.size() ) {
            const auto msg =
                std::format("DLL: <stdev> _start={}, _end={}, _data.size()={}",
                            _start, _end, _data.size());
            write_err_log( std::runtime_error(msg) );
            return std::numeric_limits<double>::signaling_NaN();
        }
        double sum = 0;
        for ( auto i = _data.begin() + _start;
              i != _data.begin() + _end; ++i ) {
            sum += (*i - _mean) * (*i - _mean);
        }
        sum /= (_end - _start) - _ddof;
        return sqrt(sum);
    }*/
/*    static double
    stdev( const std::vector<double>& _data, const uinteger& _start,
           const uinteger& _end, const double& _mean,
           const int& _ddof = 0 ) {
        if ( _start > _end || _end >= _data.size() ) {
            const auto msg =
                std::format("DLL: <stdev> _start={}, _end={}, _data.size()={}",
                            _start, _end, _data.size());
            write_err_log( std::runtime_error(msg) );
            return std::numeric_limits<double>::signaling_NaN();
        }
        double sum = 0;
        for ( auto i = _data.begin() + _start;
              i != _data.begin() + _end; ++i ) {
            sum += (*i - _mean) * (*i - _mean);
        }
        sum /= (_end - _start) - _ddof;
        return sqrt(sum);
    }

    static double
    stdev( const std::vector<integer>& _v, const int& _ddof = 0 ) {
        double _mean = mean(_v);
        return stdev(_v, _mean, _ddof);
    }
    static double
    stdev( const std::vector<double>& _v, const int& _ddof = 0 ) {
        double _mean = mean(_v);
        return stdev(_v, _mean, _ddof);
    }

    static double
    stdev( const std::vector<integer>::const_iterator _first,
           const std::vector<integer>::const_iterator _last,
           const int& _ddof = 0 ) {
        double _mean = mean(_first, _last);
        return stdev(_first, _last, _mean, _ddof);
    }
    static double
    stdev( const std::vector<double>::const_iterator _first,
           const std::vector<double>::const_iterator _last,
           const int& _ddof = 0 ) {
        double _mean = mean( _first, _last );
        return stdev( _first, _last, _mean, _ddof );
    }

    static double
    stdev( const std::vector<integer>& _data, const uinteger& _first,
           const uinteger&       _last, const int&      _ddof = 0 ) {
        double _mean = mean(_data, _first, _last);
        return stdev(_data, _first, _last, _mean, _ddof);
    }
    static double
    stdev( const std::vector<double>& _data, const uinteger& _first,
           const uinteger& _last, const int& _ddof = 0 ) {
        double _mean = mean(_data, _first, _last);
        return stdev(_data, _first, _last, _mean, _ddof);
    }
*/

    template <ArithmeticType T>
    static double
    stdev( const std::vector<T>&      _data, const uinteger& _first,
           const uinteger&            _last, const double&   _weighted_mean,
           const std::vector<double>& _stdevs, const int&    _ddof = 0 ) {
        if ( _stdevs.empty() ) {
            double sum = 0;
            for ( auto i = _data.begin() + _first;
                  i != _data.begin() + _last; ++i ) {
                sum += (*i - _weighted_mean) * (*i - _weighted_mean);
            }
            sum /= (_last - _first) - _ddof;
            return sqrt(sum);
        }

        if ( _data.size() != _stdevs.size() ) {
            throw std::runtime_error("<stdev> Size mismatch between data & standard deviations provided.");
        }

        uinteger non_zero_weights { 0 };
        double   sum_weights { 0. };
        double   sum_weights_values { 0. };

        for ( uinteger i { 0 }; i < _data.size(); ++i ) {
            non_zero_weights += ((_stdevs[i] != 0.0)
                                     ? 1
                                     : 0);
            sum_weights += _stdevs[i];
            sum_weights_values += (_stdevs[i] * (static_cast<double>(_data[i]) - _weighted_mean) *
                                   (static_cast<double>(_data[i]) - _weighted_mean));
        }

        return sqrt(sum_weights_values /
                     (static_cast<double>(non_zero_weights - 1) * sum_weights / static_cast<double>(non_zero_weights)));
    }

    /*
    static double
    stdev( const std::vector<double>&      _data, const uinteger& _first,
           const uinteger&            _last, const double&   _weighted_mean,
           const std::vector<double>& _stdevs, const int&    _ddof = 0 ) {
        if ( _stdevs.empty() ) {
            return stdev(_data, _first, _last, _weighted_mean, _ddof);
        }

        assert(_data.size() == _stdevs.size());

        uinteger non_zero_weights { 0 };
        double   sum_weights { 0. };
        double   sum_weights_values { 0. };

        for ( uinteger i { 0 }; i < _data.size(); ++i ) {
            non_zero_weights += (_stdevs[i] != 0.0)
                                     ? 1
                                     : 0;
            sum_weights += _stdevs[i];
            sum_weights_values += _stdevs[i] * (static_cast<double>(_data[i]) - _weighted_mean) *
                                   (_data[i] - _weighted_mean);
        }

        return sqrt(sum_weights_values /
                     (static_cast<double>(non_zero_weights - 1) * sum_weights / static_cast<double>(non_zero_weights)));
    }*/

    template <ArithmeticType T>
    static double
    stable_mean( const std::vector<T>& data,
                 const uinteger& first,
                 const uinteger& last,
                 const std::vector<double>& std_deviations ) {
        auto [n_first, n_last] =
            stable_period_convert( first, last );
        return mean<T>( data, n_first, n_last, std_deviations );
    }
    /*
    static double
    stable_mean( const std::vector<double>& data,
                 const uinteger& first,
                 const uinteger& last,
                 const std::vector<double>& std_deviations ) {
        auto [n_first, n_last] =
            stable_period_convert( first, last );
        return mean( data, n_first, n_last, std_deviations );
    }*/

    template <ArithmeticType T>
    static double
    stable_median( const std::vector<T>& data,
                   const uinteger& first,
                   const uinteger& last,
                   const std::vector<double>& stdevs ) {
        const auto [n_first, n_last] =
            stable_period_convert( first, last );
        return median<T>( data, first, last, stdevs );
    }
 /*
    static double
    stable_median( const std::vector<double>& data,
                   const uinteger& first,
                   const uinteger& last,
                   const std::vector<double>& stdevs ) {
        const auto [n_first, n_last] =
            stable_period_convert( first, last );
        return median( data, first, last, stdevs );
    }
*/

    template <ArithmeticType T>
    static double
    stable_stdev( const std::vector<T>& data,
                  const uinteger& first,
                  const uinteger& last,
                  const double& mean,
                  const std::vector<double>& stdevs,
                  const int& ddof = 0 ) {
        const auto& [n_first, n_last] =
            stable_period_convert( first, last );
        return stdev<T>( data, n_first, n_last, mean, stdevs, ddof );
    }

    /*
    static double
    stable_stdev( const std::vector<double>& data,
                  const uinteger& first,
                  const uinteger& last,
                  const double& mean,
                  const std::vector<double>& stdevs,
                  const int& ddof = 0 ) {
        const auto& [n_first, n_last] =
            stable_period_convert( first, last );
        return stdev( data, n_first, n_last, mean, stdevs, ddof );
    }*/

    static std::pair<std::vector<double>, std::vector<double>>
    cycle_average( const std::vector<integer>& _data,
                   const indices_t& _filter,
                   const std::vector<double>& _std_deviations,
                   double (*_func) (
                       const std::vector<integer>&,
                       const uinteger&,
                       const uinteger&,
                       const std::vector<double>& ),
                   double (*_std_deviation_func) (
                       const std::vector<integer>&,
                       const uinteger&,
                       const uinteger&,
                       const double&,
                       const std::vector<double>&,
                       const int& ) ) {
        std::vector<double> averages, tmp_std_deviations;
                    averages.reserve(_filter.size());
                    tmp_std_deviations.reserve(_filter.size());

                    for ( const auto& [first, last] : _filter ) {
                        averages.emplace_back( _func( _data, first, last, _std_deviations ) );
                        tmp_std_deviations.emplace_back( 
                            _std_deviation_func( _data,
                                                 first, last,
                                                 averages[averages.size() - 1],
                                                 _std_deviations, 1 ) );
                    }

                    averages.shrink_to_fit();
                    tmp_std_deviations.shrink_to_fit();

                    return { averages, tmp_std_deviations };
    }
    static std::pair<std::vector<double>, std::vector<double>>
    cycle_average( const std::vector<double>& _data,
                   const indices_t& _filter,
                   const std::vector<double>& _std_deviations,
                   double (*_func) (
                       const std::vector<double>&,
                       const uinteger&,
                       const uinteger&,
                       const std::vector<double>& ),
                   double (*_std_deviation_func) (
                       const std::vector<double>&,
                       const uinteger&,
                       const uinteger&,
                       const double&,
                       const std::vector<double>&,
                       const int& ) ) {
        std::vector<double> averages, tmp_std_deviations;
                    averages.reserve(_filter.size());
                    tmp_std_deviations.reserve(_filter.size());

                    for ( const auto& [first, last] : _filter ) {
                        averages.emplace_back( _func( _data, first, last, _std_deviations ) );
                        tmp_std_deviations.emplace_back( 
                            _std_deviation_func( _data,
                                                 first, last,
                                                 averages[averages.size() - 1],
                                                 _std_deviations, 1 ) );
                    }

                    averages.shrink_to_fit();
                    tmp_std_deviations.shrink_to_fit();

                    return { averages, tmp_std_deviations };
    }

    enum class avg_type
    {
        stable_mean,
        overall_mean,
        stable_median,
        overall_median
    };
    const std::map<avg_type, std::string> avg_string{
        {avg_type::stable_mean, "stable_mean"},
        {avg_type::overall_mean, "overall_mean"},
        {avg_type::stable_median, "stable_mean"},
        {avg_type::overall_median, "overall_median"}
    };

    template <ArithmeticType T>
    using a_func_t = std::function<double(const std::vector<T>&, const uinteger&, const uinteger&, const std::vector<double>&)>;
    template <ArithmeticType T>
    using std_func_t = std::function<double(const std::vector<T>&, const uinteger&, const uinteger&, const double&, const std::vector<double>&, const int&)>;
    template <ArithmeticType T>
    using func_map_t = std::map<avg_type, std::pair<a_func_t<T>, std_func_t<T>>>;

    template <ArithmeticType T>
    const func_map_t<T> avg_func_map {
        {avg_type::stable_mean, std::pair<a_func_t<T>, std_func_t<T>>{stable_mean<T>, stable_stdev<T>}},
        {avg_type::overall_mean, std::pair<a_func_t<T>, std_func_t<T>>{mean<T>, stdev<T>}},
        {avg_type::stable_median, std::pair<a_func_t<T>, std_func_t<T>>{stable_median<T>, stable_stdev<T>}},
        {avg_type::overall_median, std::pair<a_func_t<T>, std_func_t<T>>{median<T>, stdev<T>}}
    };

    template <typename T>
    static bool
    is_in( const T& _val, const std::vector<T>& _arr ) {
        try {
            for ( const T& val : _arr ) {
                if ( _val == val )
                    return true;
            }
            return false;
        }
        catch ( const std::exception& err ) {
            std::runtime_error r_err(std::string { "ERROR: <is_in> Runtime error - " } + std::string { err.what() });
            throw r_err;
        }
    }

    template <typename T>
    static uinteger
    is_in_count( const T& _val, const std::vector<T>& _arr ) {
        try {
            const auto count =
                std::accumulate(
                    _arr.cbegin(), _arr.cend(),
                    static_cast<uinteger>(0),
                    [&_val]( int& lhs, const T& rhs )
                    { return lhs += _val == rhs ? 1 : 0; }
                );
            return count;
        }
        catch ( const std::exception& err ) {
            std::runtime_error
                r_err(std::string { "ERROR: <is_in_count> Runtime error - " } + std::string { err.what() });
            throw r_err;
        }
    }

    template <typename KeyType, typename ValueType>
    static bool
    is_in( const KeyType&                                _val,
           const std::unordered_map<KeyType, ValueType>& _map ) {
        try { return _map.contains(_val); }
        catch ( const std::exception& err ) {
            std::runtime_error r_err(std::string { "ERROR: <is_in> Runtime error - " }
                                     + std::string { err.what() });
            throw r_err;
        }
    }

    template <typename KeyType, typename ValueType>
    static int
    is_in_count( const KeyType&                               _val,
                const std::unordered_map<KeyType, ValueType>& _map ) {
        try { return _map.count(_val); }
        catch ( const std::exception& err ) {
            std::runtime_error r_err(std::string { "ERROR: <is_in_count> Runtime error - " }
                                     + std::string { err.what() });
            throw r_err;
        }
    }

    enum class reduction_type
    {
        /*
         * Averaging by cycle is always applied. If no cycles exist the filtering is
         * unlikely to remove any data unless the * noise in the data is
         * noise > cutoff_fraction * max(data)
         */
        DEFAULT,
        npoints,
        ngroup,
        all,
        none
    };
    const std::map<reduction_type, std::string> reduction_string{
        {reduction_type::DEFAULT, "DEFAULT"},
        {reduction_type::npoints, "npoints"},
        {reduction_type::ngroup, "ngroup"},
        {reduction_type::all, "all"},
        {reduction_type::none, "none"}
    };

    template <typename KeyType, typename ValueType>
    std::vector<KeyType>
    get_keys( const std::unordered_map<KeyType, ValueType>& _map ) noexcept {
        std::vector<KeyType> keys;
        keys.reserve(_map.size());
        for ( const auto& key : _map | std::views::keys ) { keys.emplace_back( key ); }
        return keys;
    }

    template <typename KeyType, typename ValueType>
    std::vector<KeyType>
    get_keys( const std::map<KeyType, ValueType>& _map ) noexcept {
        std::vector<KeyType> keys;
        keys.reserve(_map.size());
        for ( const auto& key : _map | std::views::keys ) { keys.emplace_back( key ); }
        return keys;
    }

    template <typename KeyType, typename ValueType>
    std::vector<ValueType>
    get_values( const std::unordered_map<KeyType, ValueType>& _map ) noexcept {
        std::vector<ValueType> values;
        values.reserve(_map.size());
        for ( const auto& value : _map | std::views::values ) { values.emplace_back( value ); }
        return values;
    }

    template <typename KeyType, typename ValueType>
    std::vector<ValueType>
    get_values( const std::map<KeyType, ValueType>& _map ) noexcept {
        std::vector<ValueType> values;
        values.reserve(_map.size());
        for ( const auto& value : _map | std::views::values ) { values.emplace_back( value ); }
        return values;
    }

    // Concept requires T to be element iterable, i.e. suitable in ForEach loops
    template <typename T>
    concept ElementIterable = requires( std::ranges::range_value_t<T> x )
    {
        x.begin();
        x.end();
    };

    // Provides functionality similar to the python function "enumerate(...)"
    template <typename T, typename TIter = decltype(std::begin(std::declval<T>())),
              typename = decltype(std::end(std::declval<T>()))>
    constexpr auto
    enumerate( const T&& _iterable ) {
        struct iterator
        {
            size_t i;
            TIter  iter;

            bool operator!=( const iterator& other ) const { return iter != other.iter; }

            void operator++() {
                ++i;
                ++iter;
            }

            auto operator*() const { return std::tie(i, *iter); }
        };
        struct iterable_wrapper
        {
            T iterable;

            auto begin() { return iterator { 0, std::begin(iterable) }; }

            auto end() { return iterator { 0, std::end(iterable) }; }
        };
        return iterable_wrapper { std::forward<T>(_iterable) };
    }
    template <typename T, typename TIter = decltype(std::begin(std::declval<T>())),
              typename = decltype(std::end(std::declval<T>()))>
    constexpr auto
    enumerate( T&& _iterable ) {
        struct iterator
        {
            size_t i;
            TIter  iter;

            bool operator!=( const iterator& other ) const { return iter != other.iter; }

            void operator++() {
                ++i;
                ++iter;
            }

            auto operator*() { return std::tie(i, *iter); }
        };
        struct iterable_wrapper
        {
            T iterable;

            auto begin() { return iterator { 0, std::begin(iterable) }; }

            auto end() { return iterator { 0, std::end(iterable) }; }
        };
        return iterable_wrapper { std::forward<T>(_iterable) };
    }

    template <typename T>
    T
    VecMax( const std::vector<T>& arr ) {
        if ( arr.size() == 0 ) {
            std::out_of_range err("Can\'t call VecMax on empty array.");
            throw err;
        }
        return *std::max_element(arr.cbegin(), arr.cend());
    }


    class bad_DataType : public std::runtime_error
    {
    public:
        bad_DataType( const std::string& msg ) :
            std::runtime_error(msg) {}

        bad_DataType( const char* msg ) :
            std::runtime_error(msg) {}
    };


    template <typename T>
    std::vector<std::vector<T>>
    sub_vector_split( const std::vector<T>& vec, const uinteger& subvec_sz ) {
        const uinteger              n_subvec { ((vec.size() - 1) / subvec_sz) + 1 };
        std::vector<std::vector<T>> subvectors;
        subvectors.reserve(n_subvec);
        for ( uinteger n { 0 }; n < n_subvec; ++n ) {
            const auto start_itr = std::next(vec.cbegin(), n * subvec_sz);
            auto       end_itr   = std::next(vec.cbegin(), n * subvec_sz + subvec_sz);
            if ( n * subvec_sz + subvec_sz > vec.size() ) { end_itr = vec.cend(); }
            subvectors[n].insert(subvectors.end(), start_itr, end_itr);
        }
        return subvectors;
    }

    template <typename T>
    std::vector<std::vector<T>>
    sub_vector_split( typename std::vector<T>::const_iterator& start,
                      typename std::vector<T>::const_iterator& end,
                      const uinteger&                          subvec_sz ) {
        const uinteger              n_subvec { (((end - start) - 1) / subvec_sz) + 1 };
        std::vector<std::vector<T>> subvectors;
        subvectors.reserve(n_subvec);
        for ( uinteger n { 0 }; n < n_subvec; ++n ) {
            const auto start_itr = std::next(start, n * subvec_sz);
            auto       end_itr   = std::next(start, n * subvec_sz + subvec_sz);
            if ( start + (n * subvec_sz + subvec_sz) > end ) { end_itr = end; }
            subvectors[n].insert(subvectors.end(), start_itr, end_itr);
        }
        return subvectors;
    }

    template <typename T>
    indices_t
    sub_range_split( const std::vector<T>& vec,
                     const uinteger&       start,
                     const uinteger&       end,
                     const uinteger&       subrange_sz = 0 ) {
        using range = range_t;
        if ( subrange_sz == 0 ) { return { range { start, end } }; }
        const uinteger     n_subranges { (((end - start) - 1) / subrange_sz) + 1 };
        std::vector<range> subranges;
        subranges.reserve(n_subranges);
        for ( uinteger n { 0 }; n < n_subranges; ++n ) {
            const uinteger start_idx { start + (n * subrange_sz) };
            uinteger       end_idx { start + subrange_sz + (n * subrange_sz) };
            if ( end_idx > end ) { end_idx = end; }
            subranges.emplace_back(range { start_idx, end_idx });
        }
        return subranges;
    }

    inline double
    time_point_to_datetime( const std::chrono::system_clock::time_point& t ) {
        std::cout << "<time_point_to_datetime> This doesn't work correctly." << std::endl;
        return 0.0;
    }

    inline std::chrono::system_clock::time_point
    datetime_to_time_point( const double& d ) {
        using sdays = std::chrono::sys_days;
        using ddays = std::chrono::duration<double, std::chrono::days::period>;
        return std::chrono::sys_days { std::chrono::December / 30 / 1899 } +
               std::chrono::round<std::chrono::system_clock::duration>(ddays { d });
    }

    inline std::string
    tolower( const std::string_view& s ) {
        std::string result;
        for ( const char& c : s ) {
            result.push_back(std::tolower(c));
        }
        return result;
    }

    inline std::string& operator<<( std::string& a, const std::string_view b ) { return a += b; }
    inline std::string operator+( const std::string& a, const std::string_view b ) {
        std::string copy{ a };
        return copy += b;
    }

    // const std::input_iterator auto
    inline std::string
    concatenate( const std::vector<std::string>::const_iterator start,
                 const std::vector<std::string>::const_iterator end,
                 const std::string_view delimiter ) {
        assert( start <= end );
        if ( start == end ) { return ""; }

        auto result =
            std::accumulate(start, end, std::string{""},
                [&delimiter]( const std::string& lhs, const std::string& rhs)
                { return lhs + rhs + delimiter; });

        for ( uinteger i{0}; i < delimiter.size(); ++i ) { result.pop_back(); }

        return result;
    }
    std::string
    concatenate( const std::input_iterator auto start,
                 const std::input_iterator auto end,
                 const std::string_view delimiter ) {
        assert( start <= end );
        if ( start == end ) { return ""; }

        auto result =
            std::accumulate(start, end, std::string{""},
                            [&delimiter]( const std::string& lhs, const std::string& rhs)
                            { return lhs + rhs + delimiter; });

        for ( uinteger i{0}; i < delimiter.size(); ++i ) { result.pop_back(); }

        return result;
    }

    inline std::string
    concatenate( const std::vector<std::string>& v,
                 const std::string_view delimiter ) {
        return concatenate(v.cbegin(), v.cend(), delimiter);
    }

    inline std::wstring
    string_to_wstring( const std::string_view str ) {
        const auto buffer = new wchar_t[str.size() + 1]();
        size_t num_chars_converted{ 0 };
        errno_t err =
            mbstowcs_s( &num_chars_converted,
                        buffer,
                        str.size() + 1,
                        str.data(),
                        str.size() );
        buffer[str.size()] = '\0';
        return std::wstring{ buffer };
    }
    inline std::string
    wstring_to_string( const std::wstring_view wstr ) {
        const auto buffer = new char[wstr.size() + 1];
        size_t num_chars_converted{ 0 };
        errno_t err =
            wcstombs_s( &num_chars_converted,
                        buffer, wstr.size() + 1,
                        wstr.data(), wstr.size() );
        buffer[wstr.size()] = '\0';
        return std::string{ buffer };
    }

    inline std::wstring
    bstr_wstring_convert( const BSTR bstr ) {
        return (bstr == nullptr)
               ? L""
               : std::wstring{ bstr, SysStringLen(bstr) };
    }
    inline std::wstring
    bstr_wstring_convert( const VARIANT var ) {
        return bstr_wstring_convert( _bstr_t( var ) );
    }
    inline BSTR
    wstring_bstr_convert( const std::wstring_view wstr ) {
        return SysAllocStringLen( wstr.data(), wstr.size() );
    }
    inline BSTR
    string_bstr_convert( const std::string_view str ) {
        return wstring_bstr_convert(string_to_wstring(str));
    }
    inline std::string
    bstr_string_convert( const BSTR bstr ) {
        return wstring_to_string(bstr_wstring_convert(bstr));
    }
    inline std::string
    bstr_string_convert( const VARIANT var ) {
        return bstr_string_convert( _bstr_t( var ) );
    }

    template <typename From, typename To=From>
    std::vector<To>
    array_convert( const CComSafeArray<From>& csa,
                   const std::function<To(const From&)>& conversion_op ) {
        std::vector<To> result;
        result.reserve(csa.GetCount());

        for ( LONG i{ csa.GetLowerBound() }; i < csa.GetUpperBound() + 1; ++i ) {
            result.emplace_back(conversion_op(csa.GetAt(i)));
        }

        return result;
    }
    template <typename From, typename To=From>
    LPSAFEARRAY
    array_convert( const std::vector<From>& data,
                   const std::function<To(const From&)>& conversion_op,
                   const bool& copy=true) {
        // Create a column-wise SAFEARRAY
        SAFEARRAYBOUND bounds[2];
        bounds[1].cElements = 1;
        bounds[1].lLbound = 0;
        bounds[0].cElements = data.size();
        bounds[0].lLbound = 0;
        CComSafeArray<To> sa_result( bounds, 2 );

        LONG indexes[2];
        indexes[1] = 0;

        for ( const auto& [i, x] : enumerate(data) ) {
            indexes[0] = i;
            sa_result.MultiDimSetAt( indexes, conversion_op(x) );
        }

        return sa_result.Detach();
    }
    /*inline LPSAFEARRAY
    array_convert( const std::vector<std::string>& data,
                   const std::function<VARIANT(const std::string&)>& conversion_op,
                   const bool& copy=true ) {
        SAFEARRAYBOUND bounds[2];
        bounds[1].cElements = 1;
        bounds[1].lLbound = 0;
        bounds[0].cElements = data.size();
        bounds[0].lLbound = 0;
        CComSafeArray<VARIANT> sa_result( bounds, 2 );

        LONG indexes[2];
        indexes[1] = 0;

        for ( const auto& [i, x] : enumerate(data) ) {
            indexes[0] = i;
            write_log(std::format("{}: {}", i, SysStringLen(_bstr_t(x.c_str()))));
            sa_result.MultiDimSetAt( indexes, conversion_op(x) );
        }

        return sa_result.Detach();
    }*/

    inline CComSafeArray<BSTR>
    VSA_to_BSA( const CComSafeArray<VARIANT> vsa ) {
        CComSafeArray<BSTR> bsa;

        for ( LONG i{ vsa.GetLowerBound() }; i < vsa.GetUpperBound() + 1; ++i ) {
            bsa.Add(_bstr_t(vsa.GetAt(i)));
        }

        return bsa;
    }

    inline bool
    copy_file( const std::filesystem::path& source_path,
               const std::filesystem::path& destination_path ) {
        const auto source_dir = std::filesystem::directory_entry{ source_path };
        const auto destination_dir = std::filesystem::directory_entry{ destination_path };

        // Check source exists
        if ( source_dir.exists() ) {
            using copy_options=std::filesystem::copy_options;
            return std::filesystem::copy_file(source_path,
                                              destination_path,
                                              copy_options::overwrite_existing);
        }
        else {
            write_err_log(std::runtime_error("<copy_file> Source file does not exist."));
            return false;
        }
    }

    template <ArithmeticType T> auto
    check_max(const std::vector<T>& data) {
        if ( data.empty() ) {
            write_err_log(std::runtime_error("<check_max> Empty vector received."));
            return std::numeric_limits<T>::lowest();
        }
        return *std::max_element(data.cbegin(), data.cend());
    }

    template <ArithmeticType T> auto
    check_min(const std::vector<T>& data) {
        if ( data.empty() ) {
            write_err_log(std::runtime_error("<check_min> Empty vector received."));
            return std::numeric_limits<T>::max();
        }
        return *std::min_element(data.begin(), data.end());
    }

    template <typename R, typename... T>
    using gen_func = std::function<R(T...)>;

    template <typename R, typename... Args>
    R
    type_switch(const gen_func<R, Args...>& f, const DataType& dtype, Args... args) {
        R result;
        switch ( dtype ) {
        case DataType::INTEGER: {
            result = f(args...);
        }
        case DataType::DOUBLE: {
            result = f(args...);
        }
        case DataType::STRING: {
            result = f(args...);
        }
        case DataType::NONE: {
            throw
                std::runtime_error(
                    "<type_switch> DataType::NONE encountered."
                );
        }
        }
        return result;
    }

} // namespace burn_in_data_report
