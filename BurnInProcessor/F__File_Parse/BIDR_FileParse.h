#pragma once

#include <string>
#include <exception>
#include <sstream>
#include <fstream>
#include <filesystem>

#include "../S__Datastructures/BIDR_Settings.h"
#include "BIDR_Regex.h"

namespace burn_in_data_report
{
    inline bool
    check_json( const std::string& _path ) {
        try {
            std::string file_ext = _path.substr(_path.rfind('.'), _path.length());
            if ( file_ext == std::string(".json") )
                return true;
            return false;
        }
        catch ( const std::exception& err ) {
            write_err_log( err, "DLL: <check_json>" );
            return false;
        }
    }

    inline bool
    check_json( const std::filesystem::path& _path ) {
        try { return check_json(_path.string()); }
        catch ( const std::exception& err ) {
            write_err_log( err, "DLL: <check_json>" );
            return false;
        }
    }

    inline std::vector<std::string_view>
    split_str( const std::string_view _sv, const std::string_view _delims ) {
        /*
        * Declare & pre-reserve result storage
        * Reserving _sv.size() / 3 for space
        * efficiency. Reallocation will only
        * occur if there are less than 3 chars
        * on avg per value in the data.
        */

        std::vector<std::string_view> substrings;
        substrings.reserve(_sv.size() / 3);

        // Init ptr to string data & var to track
        // size of current substring.
        const char* ptr { _sv.data() };
        integer    size { 0 };

        /* Iterate through _sv, compare against all _delims */
        for ( const char& c : _sv ) {
            for ( const char& d : _delims ) {
                if ( c == d ) { // Substring found
                    substrings.emplace_back(ptr, size);
                    ptr += size + 1;
                    size = 0;
                    goto next;
                }
            }
            ++size;
        next: continue;
        }

        if ( size ) {
            /*
            * Add final substring between last delim
            * and end of _sv.
            */
            substrings.emplace_back(ptr, size);
        }

        // get rid of excess storage
        substrings.shrink_to_fit();

        return substrings;
    }

    inline std::vector<std::string_view>
    split_str( std::string_view _sv, const std::string& delims ) {
        const std::string_view         _delims { delims };
        std::vector<std::string_view> res;
        res.reserve(_sv.length() / 2);

        const char* ptr { _sv.data() };
        integer    size { 0 };

        for ( const char& c : _sv ) {
            for ( const char& d : _delims ) {
                if ( c == d ) {
                    res.emplace_back(ptr, size);
                    ptr += size + 1;
                    size = 0;
                    goto next;
                }
            }
            ++size;
        next: continue;
        }

        if ( size )
            res.emplace_back(ptr, size);

        res.shrink_to_fit();
        return res;
    }

    inline
    bool
    is_whitespace( const char& c ) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v'; }

    inline std::pair<integer, integer>
    remove_whitespace( const std::string_view sv ) {
        if ( sv.empty() ) {
            throw std::out_of_range("Empty string encountered in remove_whitespace.");
        }
        const char* ptr1{ sv.data() },
                  * ptr2{ sv.data() + sv.size() - 1 };
        while ( ptr1 != ptr2 && is_whitespace(*ptr1) )
            ptr1++;
        while ( ptr1 != ptr2 && is_whitespace(*ptr2) )
            ptr2--;
        return std::pair{ ptr1 - sv.data(), ptr2 - sv.data() };
    }

    // Remove leading & ending whitespace characters
    inline std::string
    trim( const std::string_view sv ) {
        if ( sv.empty() ) {
            throw std::out_of_range("Empty string encountered in remove_whitespace.");
        }
        auto view{
            sv
            | std::views::drop_while(isspace)
            | std::views::reverse
            | std::views::drop_while(isspace)
            | std::views::reverse
        };
        return std::string{ view.begin(), view.end() };
    }

    // Parse a json file
    inline bool
    parse_json( const std::filesystem::path& _filePath, nlohmann::json& _dest ) {
        try {
            std::ifstream stream(_filePath);
            stream >> _dest;
            return true;
        }
        catch ( const std::exception& err ) {
            write_err_log( err, "DLL: <parse_json>" );
            return false;
        }
    }

    inline
    bool
    text_to_lines( const std::string_view _text, std::vector<std::string_view>& _storage ) noexcept {
        try {
            std::string_view delim { "\n" };
            _storage = split_str(_text, delim);
        }
        catch ( const std::exception& err ) {
            write_err_log( err, "DLL: <text_to_lines>" );
            _storage.clear();
            return false;
        }
        return true;
    }

    // get last write time of file in microseconds since epoch
    template <typename Duration>
    bool
    get_file_write_time( const std::filesystem::directory_entry& _fileEntry, Duration& _age ) {
        if ( _fileEntry.exists() ) {
            _age = std::chrono::duration_cast<Duration>(_fileEntry.last_write_time().time_since_epoch());
            return true;
        }
        _age = Duration::zero();
        return false;
    }

    inline
    time_t
    FILETIME_to_time_t( const FILETIME& ft ) {
        ULARGE_INTEGER ull;
        ull.LowPart  = ft.dwLowDateTime;
        ull.HighPart = ft.dwHighDateTime;
        // time_t represents seconds, FILETIME uses 100s of nanoseconds --> div. by 10^7
        // time_t & FILETIME use different epochs (1 Jan 1970 & 1 Jan 1601 respectively)
        // Subtract no. secs between 1 Jan 1970 & 1 Jan 1601
        return (ull.QuadPart / 10000000ULL) - 11644473600ULL;
    }

    template <typename Duration>
    Duration
    FILETIME_to_DURATION( const FILETIME& ft ) {
        ULARGE_INTEGER ull;
        ull.LowPart  = ft.dwLowDateTime;
        ull.HighPart = ft.dwHighDateTime;
        // FILETIME represents time since epoch in 100s of nanoseconds
        const auto tmp = nanocentury{ ull.QuadPart - 116444736000000000 };
        return std::chrono::duration_cast<Duration>(tmp);
    }

    template <typename Duration>
    Duration
    get_file_write_time( const std::filesystem::directory_entry& _fileEntry ) {
        HANDLE   hFile;
        FILETIME fileCreationTime;
        FILETIME fileLastAccessTime;
        FILETIME fileLastWriteTime;

        hFile = CreateFile(
                           _fileEntry.path().wstring().c_str(),
                           GENERIC_READ,
                           FILE_SHARE_READ,
                           nullptr,
                           OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL,
                           nullptr
                          );
        if ( hFile == INVALID_HANDLE_VALUE ) { // Error, couldn't open file properly
            // Throw error? Log error?
            return Duration::zero();
        }
        if ( GetFileTime(hFile, &fileCreationTime, &fileLastAccessTime, &fileLastWriteTime) ) {
            FILETIME localFileLastWrite;

            if ( FileTimeToLocalFileTime(&fileLastWriteTime, &localFileLastWrite) ) {
                return FILETIME_to_DURATION<Duration>(localFileLastWrite);
            }
            return FILETIME_to_DURATION<Duration>(fileLastWriteTime);
        }
        // Error, failed to retrieve file times for some reason
        // Throw error? Log error?
        return Duration::zero();
    }

    template <typename TimeType>
    TimeType
    time_format( const std::string& _time_str, const std::string& _parse_pattern, const TimeType& _default ) {
        TimeType          time_stamp{ TimeType::duration::zero() }; // Stores result
        std::stringstream ss { _time_str }; // Construct stream to parse data from

        std::chrono::from_stream(ss, _parse_pattern.c_str(), time_stamp);

        if ( !ss.fail() ) { return time_stamp; }
        return _default;
    }

    template <typename TimeType>
    time_t
    time_to_time_t( const TimeType& _time ) {
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(_time);
        return seconds.time_since_epoch().count();
    }
} // NAMESPACE: burn_in_data_report
