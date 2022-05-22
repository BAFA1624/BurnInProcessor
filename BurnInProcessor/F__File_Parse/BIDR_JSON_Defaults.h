#pragma once

#include <string>
#include <iostream>

#include "nlohmann/json.hpp"

#include "../BIDR_Defines.h"

#include "../debug.h"

/*
Contains default JSON configurations for mixture of file types & tests.
*/

namespace burn_in_data_report
{
    enum class json_err_vals
    {
        ParseErr,
        IterErr,
        TypeErr,
        RangeErr,
        OtherErr
    };


    json_err_vals
    process_json_error( const nlohmann::json::exception& err ) {
        uint64_t err_id = err.id;
        if ( err_id >= 100 && err_id < 200 ) { // Parse error
            return json_err_vals::ParseErr;
        }
        if ( err_id >= 200 && err_id < 300 ) { // Iterator error
            return json_err_vals::IterErr;
        }
        if ( err_id >= 300 && err_id < 400 ) { // Type error
            return json_err_vals::TypeErr;
        }
        if ( err_id >= 400 && err_id < 500 ) { // Range error
            return json_err_vals::RangeErr;
        }
        // Other error
        return json_err_vals::OtherErr;
    }

    inline void
    tmp_err_handle( const std::string& msg ) {
        std::cerr << msg << std::endl;
        std::abort();
    }

    void
    throw_parse_error( const std::string& msg ) { tmp_err_handle(msg); }

    void
    throw_iter_error( const std::string msg ) { tmp_err_handle(msg); }

    void
    throw_type_error( const std::string msg ) { tmp_err_handle(msg); }

    void
    throw_range_error( const std::string msg ) { tmp_err_handle(msg); }

    void
    throw_other_error( const std::string msg ) { tmp_err_handle(msg); }
} // NAMESPACE burn_in_data_report
