#include "pch.h"
#include "BurnInDataReport.h"

static bidr::spreadsheet spreadsheet;

bool WINAPI
load_files( _In_ const LPSAFEARRAY* ppsa,
            _In_ LPVARIANT          raw_config_loc_name,
            _In_ const uinteger&    max_header_sz,
            _In_ const double&      max_off_time_minutes,
            _In_ const bool&        do_trimming ) {
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
                              do_trimming);

        std::string msg("Available columns:");
        for ( const auto& col : spreadsheet.get_available_cols() ) {
            msg += std::format("\n - {}", col);
        }
        msg += "\nFiles loaded.";
        write_log(msg);

        return true;
    }
    catch ( const std::exception& err ) {
        write_err_log(err, "DLL: <load_files>");
        return false;
    }
}

bool WINAPI
clear_spreadsheet() {
    try {
        const bool result{ spreadsheet.clear_spreadsheet() };
        if ( !result ) {
            throw std::runtime_error("Failed to clear spreadsheet.");
        }
        write_log( "Clearing spreadsheet\n" );
        //clear_log();
        return result;
    }
    catch ( const std::exception& err ) {
        write_err_log(err, "DLL: <clear_spreadsheet>");
        return false;
    }
}

bool WINAPI
init_spreadsheet( _In_ const LPSAFEARRAY* ppsa,
                  _In_ LPVARIANT          raw_config_loc_name,
                  _In_ const uinteger&    max_header_sz,
                  _In_ const double&      max_off_time_minutes,
                  _In_ const bool&        do_trimming ) {
    try {
        write_log("Initializing spreadsheet...");
        return clear_spreadsheet()
               && load_files(ppsa, raw_config_loc_name, max_header_sz,
                             max_off_time_minutes, do_trimming);
    }
    catch ( const std::exception& err ) {
        write_err_log(err, "DLL: <init_spreadsheet>");
        return false;
    }
}

bool WINAPI
add_file( LPVARIANT v_filename ) {
    try {
        const auto file { bidr::bstr_string_convert(_bstr_t(v_filename)) };
        write_log(std::format("Adding file: {}", file));
        return spreadsheet.add_file(std::filesystem::directory_entry(file));
    }
    catch ( const std::exception& err ) {
        write_err_log(
                      err,
                      std::format("DLL: <add_file> {}",
                                  bidr::bstr_string_convert(*v_filename)
                                 )
                     );
        return false;
    }
}

bool WINAPI
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

        return spreadsheet.add_files(files);
    }
    catch ( const std::exception& err ) {
        write_err_log(err, "DLL: <add_files> Unable to add files.");
        return false;
    }
}

bool WINAPI
remove_file( const uinteger& idx ) {
    try {
        write_log(std::format("Removing file {}...", idx + 1));
        spreadsheet.remove_file(idx);
        return true;
    }
    catch ( const std::exception& err ) {
        write_err_log(err, "DLL: <remove_file>");
        return false;
    }
}

bool WINAPI
remove_files( LPSAFEARRAY* ppsa ) {
    try {
        CComSafeArray<uinteger> csa(*ppsa);
        write_log(std::format("Removing {} files...", csa.GetCount()));
        constexpr auto conversion_op = [](const uinteger& x) { return x; };
        const auto arr {
                bidr::array_convert<uinteger, uinteger>( csa, conversion_op )
            };
        *ppsa = csa.Detach();
        return spreadsheet.remove_files(arr);
    }
    catch ( const std::exception& err ) {
        write_err_log(err, "DLL: <remove_files>");
        return false;
    }
}

LPSAFEARRAY WINAPI
get_current_cols() {
    try {
        write_log("Retrieving loaded columns...");
        CComSafeArray<VARIANT> csa( spreadsheet.get_current_cols().size() );
        for ( const auto& [i, col] : bidr::enumerate(spreadsheet.get_current_cols()) ) {
            csa.SetAt( i, _variant_t( bidr::string_bstr_convert( col ) ) );
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
        CComSafeArray<VARIANT> csa( spreadsheet.get_available_cols().size() );
        for ( const auto& [i, col] : bidr::enumerate(spreadsheet.get_available_cols()) ) {
            write_log( std::format("  - {}", col) );
            csa.SetAt( i, _variant_t( bidr::string_bstr_convert( col ) ) );
        }
        return csa.Detach();
    }
    catch ( const std::exception& err ) {
        write_err_log(err, "DLL: <get_available_cols>");
        return SafeArrayCreateVector(VT_VARIANT, 0, 0);
    }
}

bool WINAPI
load_column( LPVARIANT v_col_title ) {
    try {
        const auto col_title { bidr::bstr_string_convert(*v_col_title) };
        write_log(std::format("Loading column: {}", col_title));
        const bool result{ spreadsheet.load_column(col_title) };
        write_log( (result ? "Successful" : "Failed") );
        return result;
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

bool WINAPI
load_columns( LPSAFEARRAY* ppsa ) {
    try {
        CComSafeArray<VARIANT> csa(*ppsa);
        const auto             arr {
                bidr::array_convert<VARIANT, std::string>(
                                                          csa, []( const VARIANT& v ) {
                                                              return bidr::bstr_string_convert(v);
                                                          }
                                                         )
            };
        *ppsa = csa.Detach();
        write_log(std::format("Loading {} columns...", arr.size()));
        std::string msg;
        for ( const auto& col : arr ) {
            msg += col + "  ";
        }
        write_log(msg);
        const bool result{ spreadsheet.load_columns(arr) };
        write_log( (result ? "Successful" : "Failed") );
        return result;
    }
    catch ( const std::exception& err ) {
        write_err_log(err, "DLL: <load_columns>");
        return false;
    }
}

bool WINAPI
unload_column( LPVARIANT v_col_title ) {
    try {
        const auto col_title { bidr::bstr_string_convert(*v_col_title) };
        write_log(std::format("Loading {}...", col_title));
        return spreadsheet.unload_column(col_title);
    }
    catch ( const std::exception& err ) {
        const auto col_title { bidr::bstr_string_convert(*v_col_title) };
        write_err_log(err, std::format("DLL: <unload_column> {}", col_title));
        return false;
    }
}

bool WINAPI
unload_columns( LPSAFEARRAY* ppsa ) {
    try {
        CComSafeArray<VARIANT> csa(*ppsa);
        const auto             arr {
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
        for ( const auto& col : arr ) {
            msg += col + "  ";
        }
        write_log( msg );
        const bool result = spreadsheet.unload_columns( arr );
        return result;
    }
    catch ( const std::exception& err ) {
        write_err_log(err, "DLL: <unload_columns>");
        return false;
    }
}

bool WINAPI
filter( LPVARIANT       v_col_title,
        const double&   cutoff,
        const uinteger& n_skip,
        const uinteger& max_range_sz ) {
    try {
        write_log("Filtering data...");
        const auto col_title{ bidr::bstr_string_convert(*v_col_title) };
        write_log(std::format("{}", col_title));
        return spreadsheet.filter(col_title, cutoff, n_skip, max_range_sz);
    }
    catch ( const std::exception& err ) {
        const auto col_title{ bidr::bstr_string_convert(*v_col_title) };
        write_err_log(err, std::format("DLL: <filter>", col_title));
        return false;
    }
}

bool WINAPI
apply_reduction( LPVARIANT v_col_title,
                 const uinteger& type,
                 const uinteger& reduction,
                 const uinteger& average,
                 const uinteger& n_group,
                 const uinteger& n_point) {
    try {
        write_log( "Applying reduction..." );
        const auto col_title{ bidr::bstr_string_convert(*v_col_title) };
        write_log( std::format("{}", col_title) );

        const auto d_type = static_cast<bidr::DataType>(type);
        const auto r_type = static_cast<bidr::reduction_type>(reduction);
        const auto a_type = static_cast<bidr::avg_type>(average);

        write_log( std::format("Data type: {}", bidr::type_string.at(d_type)));

        switch ( d_type ) {
        case bidr::DataType::INTEGER:
            return spreadsheet.apply_reduction<integer>(col_title, r_type, a_type, n_group, n_point);
        case bidr::DataType::DOUBLE:
            return spreadsheet.apply_reduction<double>(col_title, r_type, a_type, n_group, n_point);
        case bidr::DataType::STRING:
            return spreadsheet.apply_reduction<std::string>(col_title, r_type, a_type, n_group, n_point);
        case bidr::DataType::NONE:
            write_log( "DLL: <apply_reduction> NONE type received." );
            return false;
        }
        return false;
    }
    catch ( const std::exception& err ) {
        const auto col_title{ bidr::bstr_string_convert(*v_col_title) };
        write_err_log( err, std::format("DLL: <apply_reduction> {}", col_title) );
        return false;
    }
}

bool WINAPI
is_initialized() {
    return spreadsheet.is_initialized();
}

