#pragma once

#ifdef BURNINPROCESSOR_EXPORTS
#define BIDR_API __declspec(dllexport)
#else
#define BIDR_API __declspec(dllimport)
#endif

namespace bidr = burn_in_data_report;

/*
 * This header declares two types of function:
 * - i_*: If a function is prefixed with i_..., it is an
 *        internal function which interacts directly with
 *        burn_in_data_report functions and objects. All
 */

/*
 * Receiving arrays:
 * - Take LPSAFEARRAY* as parameter
 * - Use to construct CComSafeArray
 * - Remember to call .Detach() if you
 *   need to send the SAFEARRAY back.
 * - Return as LPSAFEARRAY
 * Receiving strings:
 * - Use VARIANTs.
 * - In arrays, use variants.
 * - Can convert to BSTR using _bstr_t
 *   constructor.
 */

extern "C"
{
    
    /*
     * TODO:
     * Wrappers for spreadsheet class:
     *   - init_spreadsheet: DONE
     *   - load_files: DONE
     *   - add_file: DONE
     *   - add_files: DONE
     *   - remove_file: DONE
     *   - remove_files: DONE
     *   - get_current_cols: DONE
     *   - get_available_cols: DONE
     *   - load_column: DONE
     *   - load_columns: DONE
     *   - unload_column: DONE
     *   - unload_columns: DONE
     *   - filter: DONE
     *   - apply_reduction: DONE
     *   - clear_spreadsheet: DONE
     *   - n_rows: DONE
     *   - get: DONE (wraps get_i, get_d, get_s)
     *   - get_error: DONE
     *   - contains: DONE
    */

    // Load selected files
    // Not sure I want to export this
    /*BIDR_API bool WINAPI
    load_files( _In_ const LPSAFEARRAY* ppsa,
                _In_ LPVARIANT          raw_config_loc_name,
                _In_ const uinteger&    max_header_sz,
                _In_ const double&      max_off_time_minutes,
                _In_ const bool&        do_trimming );*/

    // Clear spreadsheet, then load new files
    BIDR_API BOOL WINAPI
    clear_spreadsheet();

    // Initialise global spreadsheet
    BIDR_API BOOL WINAPI
    init_spreadsheet( _In_ const LPSAFEARRAY* ppsa,
                      _In_ LPVARIANT          raw_config_loc_name,
                      _In_ const uinteger&    max_header_sz,
                      _In_ const double&      max_off_time_minutes,
                      _In_ const BOOL&        do_trimming,
                      _In_ LPVARIANT log_file_path );

    BIDR_API BOOL WINAPI
    add_file( LPVARIANT v_filename );

    BIDR_API BOOL WINAPI
    add_files( LPSAFEARRAY* ppsa );

    BIDR_API BOOL WINAPI
    remove_file( const uinteger& idx );

    BIDR_API BOOL WINAPI
    remove_files( LPSAFEARRAY* ppsa );

    BIDR_API LPSAFEARRAY WINAPI
    get_current_cols();

    BIDR_API LPSAFEARRAY WINAPI
    get_available_cols();

    BIDR_API BOOL WINAPI
    load_column( LPVARIANT v_col_title);

    BIDR_API BOOL WINAPI
    load_columns( LPSAFEARRAY* ppsa );

    BIDR_API BOOL WINAPI
    unload_column( LPVARIANT v_col_title );

    BIDR_API BOOL WINAPI
    unload_columns( LPSAFEARRAY* ppsa );

    BIDR_API BOOL WINAPI
    filter( LPVARIANT       v_col_title,
            const double&   cutoff,
            const uinteger& n_skip       = 2,
            const uinteger& max_range_sz = 0);

    BIDR_API BOOL WINAPI
    reduce_data( const uinteger& reduction_type = 0,
                 const uinteger& average_type   = 0,
                 const uinteger& n_group        = 1,
                 const uinteger& n_point        = 0 );

    BIDR_API BOOL WINAPI
    is_initialized();

    BIDR_API uinteger WINAPI
    n_rows();

    BIDR_API LPSAFEARRAY WINAPI
    get( LPVARIANT key );

    BIDR_API LPSAFEARRAY WINAPI
    get_error( LPVARIANT key );

    BIDR_API BOOL WINAPI
    contains( LPVARIANT key );

    BIDR_API integer WINAPI
    type( LPVARIANT key );

    BIDR_API BOOL WINAPI
    clear_changes();

    /*
     * Loading columns seems to work once then not again
     * after unloading?
     *
     * I think there is an issue with unload_column(...)
     * Maybe not being removed from type_map_?
     */

    /*
     * Check functionality:
     *   - init_spreadsheet: DONE
     *   - load_files: DONE
     *   - add_file: ----
     *   - add_files: ----
     *   - remove_file: ----
     *   - remove_files: ----
     *   - get_current_cols: DONE
     *   - get_available_cols: DONE
     *   - load_column: DONE
     *   - load_columns: DONE
     *   - unload_column: DONE
     *   - unload_columns: DONE
     *   - filter: DONE
     *   - apply_reduction: PARTIAL
     *      (COMPLETE: avg by cycle,
     *       TODO:
     *          avg by n_groups, avg by n_points)
     *   - clear_spreadsheet: DONE
     *   - is_initialized: DONE
     *   - n_rows: DONE
     *   - get: DONE (wraps get_i, get_d, get_s)
     *   - get_error: DONE
     *   - contains: DONE
     */
}
