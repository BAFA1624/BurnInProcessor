#pragma once

//#include <memory>
#include <new>
//#include <cstring>
//#include <cmath>
#include <iostream>
#include <filesystem>

#include "../BIDR_Defines.h"

#include "../debug.h"


namespace burn_in_data_report
{
    class memory_handle
    {
    public:
        char*         _data;
        uinteger _sz;

        memory_handle() :
            _data(nullptr),
            _sz(0) {}

        memory_handle( char* _d, const uinteger& _s ) :
            _data(_d),
            _sz(_s) {}

        ~memory_handle() {
            if ( _data )
                delete[] _data;
        }

        memory_handle( const memory_handle& _other ) noexcept :
            _data(nullptr),
            _sz(0) { // Copy constructor
            if ( _other._data ) {
                _sz = _other._sz;
                alloc(_data, _sz);

                memcpy_checked(_data, _other._data, _sz);
            }
        }

        memory_handle& operator=( const memory_handle& _other ) noexcept { // Copy assignment
            if ( this == &_other ) { // Prevent self assignment
                return *this;
            }

            if ( _other._data ) {
                if ( _sz != _other._sz ) { // Can't reuse _data, realloc
                    realloc(_data, _sz, _other._sz);
                }

                memcpy_checked(_data, _other._data, _sz);
            }
            return *this;
        }

        memory_handle( memory_handle&& _other ) noexcept :
            _data(nullptr),
            _sz(0) { // Move constructor
            if ( _other._data ) {
                _data = std::exchange(_other._data, nullptr);
                _sz   = std::exchange(_other._sz, 0);
            }
        }

        memory_handle& operator=( memory_handle&& _other ) noexcept { // Move assignment
            if ( this == &_other ) { // Guard self assignment
                return *this;
            }

            if ( _other._data ) {
                _data = std::exchange(_other._data, nullptr);
                _sz   = std::exchange(_other._sz, 0);
            }

            return *this;
        }

        memory_handle& operator+=( const memory_handle& _rhs ) {
            const uinteger new_sz{ _sz + _rhs._sz }, this_old_sz{ _sz };

            realloc(_data, _sz, new_sz);

            memcpy_checked(_data + this_old_sz, _rhs._data, _rhs._sz);

            return *this;
        }

        char& operator[]( const uinteger& idx ) { return _data[access_range_checked(idx, 0, _sz - 1)]; }

        friend memory_handle operator+( memory_handle _lhs, const memory_handle& _rhs ) {
            _lhs += _rhs;
            return _lhs;
        }

        friend memory_handle
        get_file(
            const std::filesystem::directory_entry& _file
        ) noexcept;

        char* get() const noexcept {
            if ( _data ) {
                auto result = new char[access_checked(_sz)];
                memcpy_checked(result, _data, _sz);
                return result;
            }
            return nullptr;
        }

        void get( char* _dest, const char* _src, const uinteger& _n ) const {
            if ( _data && (_src + _n) >= _data && (_src + _n) <= (_data + _sz) ) { memcpy_checked(_dest, _src, _n); }
            else {
                throw std::out_of_range
                    ("memory_handle::get(char* _dest, const char* _src, const uint64_t& _n): Index out of range. _n = "
                        + std::to_string(_n));
            }
        }

        void get( char* _dest, const uinteger& start, const uinteger& end ) const {
            if ( !_data || _sz == 0 ) {
                std::out_of_range
                    err("memory_handle::get(char*_dest, const uint64_t& start, const uint64_t& end): Invalid request, no data available. (_sz = 0).");
                throw err;
            }

            if ( _data && start < _sz && end <= _sz ) {
                if ( end >= start ) { std::copy((_data + start), (_data + end), _dest); }
                else { std::reverse_copy((_data + end), (_data + start), _dest); }
            }
            else {
                std::out_of_range
                    err("memory_handle::get(char* _dest, const uint64_t& start, const uint64_t& end): Index out of range.");
                throw err;
            }
        }

        static char* alloc( char*& _target, uinteger& sz ) noexcept {
            free(_target);
            try { _target = new char[access_checked(sz)]; }
            catch ( const std::bad_alloc& e ) {
                write_err_log( e, "DLL: <handle::alloc>" );
                if ( _target ) {
                    delete[] _target;
                    _target = nullptr;
                    sz      = 0;
                }
            }
            return _target;
        }

        char* alloc( const uinteger& sz ) noexcept {
            _sz = sz;
            return alloc(_data, _sz);
        }

        static char* realloc( char*& _target, uinteger& _target_sz, const uinteger& sz ) noexcept {
            if ( _target_sz == sz )
                return _target;
            if ( sz == 0 ) {
                free(_target);
                return _target;
            }

            auto     tmp     = new char[access_checked(sz)];
            uinteger copy_sz = (sz > _target_sz)
                                   ? _target_sz
                                   : sz;

            memcpy_checked(tmp, _target, copy_sz);

            free(_target);
            _target = tmp;

            _target_sz = sz;

            return _target;
        }

        char* realloc( const uinteger& sz ) noexcept { return realloc(_data, _sz, sz); }

        static void free( char* _target ) noexcept {
            if ( _target ) {
                delete[] _target;
                _target = nullptr;
            }
        }

        void free() noexcept {
            free(_data); // if (_data) handled in free(char* _target)
            if ( !_data )
                _sz = 0;
        }

        char at( uinteger _idx ) const noexcept { if ( _data && _idx < _sz ) { return _data[_idx]; } }
    };
} // NAMESPACE: burn_in_data_report
