#include "pch.h"
#include "BurnInDataReport.h"

static bidr::spreadsheet spreadsheet;

BOOL WINAPI
load_files( _In_ const LPSAFEARRAY* ppsa,
            _In_ LPVARIANT raw_config_loc_name,
            _In_ const uinteger& max_header_sz,
            _In_ const double& max_off_time_minutes,
            _In_ const BOOL& do_trimming ) {
    try {
        write_log("Adding files...");
        CComSafeArray<VARIANT> variant_filenames(*ppsa);

        std::vector<std::string> filenames;
        filenames.reserve(variant_filenames.GetCount());

        write_log("Files to load:");
        for ( ULONG i { 0 }; i < variant_filenames.GetCount(); ++i ) {
            const std::string filename = bidr::bstr_string_convert(variant_filenames.GetAt(i));
            filenames.emplace_back(filename);
            write_log(std::format(" - {}", filename));
        }

        const auto config_loc_name = bidr::bstr_string_convert(*raw_config_loc_name);
        write_log(std::format("Config location: {}", config_loc_name));

        spreadsheet =
            bidr::spreadsheet(filenames,
                              config_loc_name,
                              max_header_sz,
                              max_off_time_minutes,
                              do_trimming == 1
                                  ? true
                                  : false);

        std::string msg("Available columns:");
        for ( const auto& col : spreadsheet.get_available_cols() ) { msg += std::format("\n - {}", col); }
        msg += "\nFiles loaded.";
        write_log(msg);

        return TRUE;
    }
    catch ( const std::exception& err ) {
        write_err_log(err, "DLL: <load_files>");
        return FALSE;
    }
}

BOOL WINAPI
clear_spreadsheet() {
    try {
        const BOOL result = (spreadsheet.clear_spreadsheet())
                                ? TRUE
                                : FALSE;
        if ( !result ) { throw std::runtime_error("Failed to clear spreadsheet."); }
        clear_log();
        return result;
    }
    catch ( const std::exception& err ) {
        write_err_log(err, "DLL: <clear_spreadsheet>");
        return FALSE;
    }
}

BOOL WINAPI
init_spreadsheet( _In_ const LPSAFEARRAY* ppsa,
                  _In_ LPVARIANT raw_config_loc_name,
                  _In_ const uinteger& max_header_sz,
                  _In_ const double& max_off_time_minutes,
                  _In_ const BOOL& do_trimming ) {
    try {
        write_log("Initializing spreadsheet...");
        BOOL result = clear_spreadsheet();
        result &= load_files(ppsa, raw_config_loc_name, max_header_sz,
                             max_off_time_minutes, do_trimming);
        return result;
    }
    catch ( const std::exception& err ) {
        write_err_log(err, "DLL: <init_spreadsheet>");
        return FALSE;
    }
}

BOOL WINAPI
add_file( LPVARIANT v_filename ) {
    try {
        const auto file { bidr::bstr_string_convert(_bstr_t(v_filename)) };
        write_log(std::format("Adding file: {}", file));
        const BOOL result = spreadsheet.add_file(std::filesystem::directory_entry(file))
                                ? TRUE
                                : FALSE;
        return result;
    }
    catch ( const std::exception& err ) {
        write_err_log(
                      err,
                      std::format("DLL: <add_file> {}",
                                  bidr::bstr_string_convert(*v_filename)
                                 )
                     );
        return FALSE;
    }
}

BOOL WINAPI
add_files( LPSAFEARRAY* ppsa ) {
    CComSafeArray<VARIANT> var_csa;
    try {
        write_log("Adding files:");

        var_csa.Attach(*ppsa);

        std::vector<std::filesystem::directory_entry> files;
        files.reserve(var_csa.GetCount());

        for ( ULONG i { 0 }; i < var_csa.GetCount(); ++i ) {
            const auto file = bidr::bstr_string_convert(var_csa.GetAt(i));
            files.emplace_back(std::filesystem::directory_entry { file });
            write_log(std::format(" - {}", file));
        }

        *ppsa = var_csa.Detach();

        return spreadsheet.add_files(files)
                   ? TRUE
                   : FALSE;
    }
    catch ( const std::exception& err ) {
        write_err_log(err, "DLL: <add_files> Unable to add files.");
        return FALSE;
    }
}

BOOL WINAPI
remove_file( const uinteger& idx ) {
    try {
        write_log(std::format("Removing file {}...", idx + 1));
        return spreadsheet.remove_file(idx)
                   ? TRUE
                   : FALSE;
    }
    catch ( const std::exception& err ) {
        write_err_log(err, "DLL: <remove_file>");
        return FALSE;
    }
}

BOOL WINAPI
remove_files( LPSAFEARRAY* ppsa ) {
    try {
        CComSafeArray<uinteger> csa(*ppsa);
        write_log(std::format("Removing {} files...", csa.GetCount()));
        constexpr auto conversion_op = []( const uinteger& x ) { return x; };
        const auto arr {
                bidr::array_convert<uinteger, uinteger>(csa, conversion_op)
            };
        *ppsa = csa.Detach();
        return spreadsheet.remove_files(arr)
                   ? TRUE
                   : FALSE;
    }
    catch ( const std::exception& err ) {
        write_err_log(err, "DLL: <remove_files>");
        return FALSE;
    }
}

LPSAFEARRAY WINAPI
get_current_cols() {
    try {
        write_log("Retrieving loaded columns...");
        CComSafeArray<VARIANT> csa(spreadsheet.get_current_cols().size());
        for ( const auto& [i, col] : bidr::enumerate(spreadsheet.get_current_cols()) ) {
            csa.SetAt(i, _variant_t(bidr::string_bstr_convert(col)));
        }
        return csa.Detach();
    }
    catch ( const std::exception& err ) {
        write_err_log(err, "DLL: <get_current_cols>");
        return SafeArrayCreateVector(VT_VARIANT, 0, 0);
    }
}

LPSAFEARRAY WINAPI
get_available_cols() {
    try {
        write_log("Retrieving available columns...");
        CComSafeArray<VARIANT> csa(spreadsheet.get_available_cols().size());
        for ( const auto& [i, col] : bidr::enumerate(spreadsheet.get_available_cols()) ) {
            write_log(std::format("  - {}", col));
            csa.SetAt(i, _variant_t(bidr::string_bstr_convert(col)));
        }
        return csa.Detach();
    }
    catch ( const std::exception& err ) {
        write_err_log(err, "DLL: <get_available_cols>");
        return SafeArrayCreateVector(VT_VARIANT, 0, 0);
    }
}

BOOL WINAPI
load_column( LPVARIANT v_col_title ) {
    try {
        const auto col_title { bidr::bstr_string_convert(*v_col_title) };
        write_log(std::format("Loading column: {}", col_title));
        const bool result = spreadsheet.load_column(col_title);
        write_log((result
                       ? "Succeeded"
                       : "Failed"));
        return result
                   ? TRUE
                   : FALSE;
    }
    catch ( const std::exception& err ) {
        const auto col_title { bidr::bstr_string_convert(*v_col_title) };
        write_err_log(
                      err,
                      std::format("DLL: <load_column> Failed to load {}", col_title)
                     );
        return false;
    }
}

BOOL WINAPI
load_columns( LPSAFEARRAY* ppsa ) {
    try {
        CComSafeArray<VARIANT> csa(*ppsa);

        const auto arr {
                bidr::array_convert<VARIANT, std::string>(
                                                          csa, []( const VARIANT& v ) {
                                                              return bidr::bstr_string_convert(v);
                                                          }
                                                         )
            };
        *ppsa = csa.Detach();
        write_log(std::format("Loading {} columns...", arr.size()));
        std::string msg;
        for ( const auto& col : arr ) { msg += col + "  "; }
        write_log(msg);
        const bool result = spreadsheet.load_columns(arr);
        write_log((result
                       ? "Succeeded"
                       : "Failed"));
        return result
                   ? TRUE
                   : FALSE;
    }
    catch ( const std::exception& err ) {
        write_err_log(err, "DLL: <load_columns>");
        return FALSE;
    }
}

BOOL WINAPI
unload_column( LPVARIANT v_col_title ) {
    try {
        const auto col_title { bidr::bstr_string_convert(*v_col_title) };
        write_log(std::format("Loading {}...", col_title));
        const bool result = spreadsheet.unload_column(col_title);
        write_log((result
                       ? "Succeeded"
                       : "Failed"));
        return result
                   ? TRUE
                   : FALSE;
    }
    catch ( const std::exception& err ) {
        const auto col_title { bidr::bstr_string_convert(*v_col_title) };
        write_err_log(err, std::format("DLL: <unload_column> {}", col_title));
        return FALSE;
    }
}

BOOL WINAPI
unload_columns( LPSAFEARRAY* ppsa ) {
    try {
        CComSafeArray<VARIANT> csa(*ppsa);
        const auto arr {
                bidr::array_convert<VARIANT, std::string>(
                                                          csa,
                                                          []( const VARIANT& v ) {
                                                              return bidr::bstr_string_convert(v);
                                                          }
                                                         )
            };
        write_log(std::format("Unloading {} columns...", arr.size()));
        *ppsa = csa.Detach();
        std::string msg;
        for ( const auto& col : arr ) { msg += col + "  "; }
        write_log(msg);
        const bool result = spreadsheet.unload_columns(arr);
        write_log((result
                       ? "Succeeded"
                       : "Failed"));
        return result
                   ? TRUE
                   : FALSE;
    }
    catch ( const std::exception& err ) {
        write_err_log(err, "DLL: <unload_columns>");
        return FALSE;
    }
}

BOOL WINAPI
filter( LPVARIANT v_col_title,
        const double& cutoff,
        const uinteger& n_skip,
        const uinteger& max_range_sz ) {
    try {
        write_log("Filtering data...");
        const auto col_title { bidr::bstr_string_convert(*v_col_title) };
        write_log(std::format("{}", col_title));
        return spreadsheet.filter(col_title, cutoff, n_skip, max_range_sz)
                   ? TRUE
                   : FALSE;
    }
    catch ( const std::exception& err ) {
        const auto col_title { bidr::bstr_string_convert(*v_col_title) };
        write_err_log(err, std::format("DLL: <filter>", col_title));
        return FALSE;
    }
}

BOOL WINAPI
reduce_data( const uinteger& reduction_type,
             const uinteger& average_type,
             const uinteger& n_group,
             const uinteger& n_points ) {
    try {
        write_log( "Reducing data..." );
        const auto r_type = static_cast<bidr::reduction_type>( reduction_type );
        const auto a_type = static_cast<bidr::avg_type>( average_type );
        const auto result{ spreadsheet.reduce( r_type, a_type, n_group, n_points ) };
        write_log( result ? "Succeeded." : "Failed" );
        return result;
    }
    catch ( const std::exception& err ) {
        write_err_log( err, "DLL: <reduce_data>" );
        return FALSE;
    }
}

BOOL WINAPI
is_initialized() {
    return spreadsheet.is_initialized()
               ? TRUE
               : FALSE;
}

uinteger WINAPI
n_rows() { return spreadsheet.n_rows(); }

LPSAFEARRAY WINAPI
get( LPVARIANT key ) {
    try {
        using DT = bidr::DataType;

        const auto s_key { bidr::bstr_string_convert(*key) };
        const auto type { spreadsheet.type( s_key ) };

        if (  type == DT::NONE ) {
            throw std::out_of_range( "Key not found." );
        }

        switch ( type ) {
        case DT::INTEGER: {
            return bidr::array_convert<integer, VARIANT>(
                    spreadsheet.get_i(s_key),
                    [](const integer& i)
                    { return _variant_t(i); },
                    false
                );
            }
        case DT::DOUBLE: {
            return bidr::array_convert<double, VARIANT>(
                    spreadsheet.get_d(s_key),
                    [](const double& d)
                    { return _variant_t(d); },
                    false
                );
            }
        case DT::STRING: {
            return bidr::array_convert<std::string, VARIANT>(
                    spreadsheet.get_s(s_key),
                    [](const std::string& s)
                    { return _variant_t(_bstr_t(s.c_str())); }
                );
            }
        case DT::NONE: {
            throw std::runtime_error("Invalid type.");
            }
        }

        return SafeArrayCreateVectorEx(VT_VARIANT, 0, 0, nullptr);
    }
    catch ( const std::exception& err ) {
        write_err_log( err, "DLL: <get>" );
        return SafeArrayCreateVectorEx(VT_VARIANT, 0, 0, nullptr);
    }
}

LPSAFEARRAY WINAPI
get_error( LPVARIANT key ) {
    try {
        using DT = bidr::DataType;

        const auto s_key { bidr::bstr_string_convert(*key) };

        if ( const auto type { spreadsheet.type(s_key) };
             type == DT::NONE ) {
            throw std::out_of_range("Key not found.");
        }

        return bidr::array_convert<double, VARIANT>(
            spreadsheet.get_error(s_key),
            [](const double& d)
            { return _variant_t(d); },
            false
        );
    }
    catch ( const std::exception& err ) {
        write_err_log( err, "DLL: <get_error>" );
        return SafeArrayCreateVectorEx(VT_R8, 0, 0, nullptr);
    }
}

BOOL WINAPI
contains( LPVARIANT key ) {
    try {
        const auto s_key { bidr::bstr_string_convert(*key) };
        const auto exists { spreadsheet.contains(s_key) };
        return exists ? TRUE : FALSE;
    }
    catch ( const std::exception& err ) {
        write_err_log( err, "DLL: <contains>" );
        return FALSE;
    }
}

integer WINAPI
type( LPVARIANT key ) {
    using DT = bidr::DataType;
    try {
        const auto s_key{ bidr::bstr_string_convert(*key) };
        const auto type{ spreadsheet.type(s_key) };
        return static_cast<integer>(type);
    }
    catch ( const std::exception& err ) {
        write_err_log( err, "DLL: <type>" );
        return static_cast<integer>(DT::NONE);
    }
}
