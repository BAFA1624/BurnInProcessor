#pragma once

#include "../BIDR_Defines.h"


namespace burn_in_data_report
{
    // -> bool: Returns if _pattern is found within _line
    inline bool
    line_check( const std::string_view& _line, const std::regex& _pattern ) noexcept {
        try { return std::regex_match(std::string { _line }, _pattern); }
        catch ( const std::exception& err ) {
            write_err_log( err, "DLL: <line_check>" );
            return false;
        }
    }

    // -> bool: Returns _pattern is found in any line in _data. Sets _lim to index of that line if found.
    inline bool
    line_check( const std::vector<std::string_view>& _data,
               const std::regex&                    _pattern,
               uinteger&                            _lim ) noexcept {
        try {
            uinteger count = 0;
            while ( count < _lim && count < _data.size() ) {
                if ( line_check(_data[count], _pattern) ) {
                    _lim = count;
                    return true;
                }
                count++;
            }

            return false;
        }
        catch ( const std::exception& err ) {
            write_err_log( err, "DLL: <line_check>" );
            return false;
        }
    }

    // -> bool: Returns if _pattern_str is found on any line in _data. Sets _lim to index of that line if found.
    inline bool
    line_check( const std::vector<std::string_view>& _data,
               const std::string_view&              _pattern_str,
               uinteger&                            _lim ) noexcept {
        try {
            const auto pattern = std::regex(std::string { _pattern_str });
            return line_check(_data, pattern, _lim);
        }
        catch ( const std::exception& err ) {
            write_err_log( err, "DLL: <line_check>" );
            return false;
        }
    }

    inline bool
    line_capture(
        const std::string_view&   _line,
        std::vector<std::string>& _matches,
        const std::regex&         _pattern
    ) noexcept {
        try {
            std::smatch sm;
            std::string text { _line };
            if ( std::regex_match(text, sm, _pattern) ) {
                for ( auto& match : sm )
                    _matches.push_back(match.str());
                return true;
            }
            return false;
        }
        catch ( const std::exception& err ) {
            write_err_log( err, "DLL: <line_capture>" );
            return false;
        }
    }

    inline bool
    line_capture(
        const std::vector<std::string_view>& _data,
        std::vector<std::string>&            _matches,
        const std::regex&                    _pattern,
        const uinteger&                      _lim
    ) noexcept {
        try {
            uinteger count = 0;
            while ( count < _lim && count < _data.size() ) {
                if ( line_capture(_data[count], _matches, _pattern) ) { return true; }
                count++;
            }
            return false;
        }
        catch ( const std::exception& err ) {
            write_err_log( err, "DLL: <line_capture>" );
            return false;
        }
    }

    inline bool
    line_capture(
        const std::vector<std::string_view>& _data,
        std::vector<std::string>&            _matches,
        const std::string_view&              _pattern_str,
        const uinteger&                      _lim
    ) noexcept {
        try {
            auto pattern = std::regex(_pattern_str.data());
            return line_capture(_data, _matches, pattern, _lim);
        }
        catch ( const std::exception& err ) {
            write_err_log( err, "DLL: <line_capture>" );
            return false;
        }
    }
} // NAMESPACE: burn_in_data_report
