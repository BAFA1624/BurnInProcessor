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
    BIDR_API bool WINAPI
    clear_spreadsheet();

    // Initialise global spreadsheet
    BIDR_API bool WINAPI
    init_spreadsheet( _In_ const LPSAFEARRAY* ppsa,
                      _In_ LPVARIANT          raw_config_loc_name,
                      _In_ const uinteger&    max_header_sz,
                      _In_ const double&      max_off_time_minutes,
                      _In_ const bool&        do_trimming );

    BIDR_API bool WINAPI
    add_file( LPVARIANT v_filename );

    BIDR_API bool WINAPI
    add_files( LPSAFEARRAY* ppsa );

    BIDR_API bool WINAPI
    remove_file( const uinteger& idx );

    BIDR_API bool WINAPI
    remove_files( LPSAFEARRAY* ppsa );

    BIDR_API LPSAFEARRAY WINAPI
    get_current_cols();

    BIDR_API LPSAFEARRAY WINAPI
    get_available_cols();

    BIDR_API bool WINAPI
    load_column( LPVARIANT v_col_title);

    BIDR_API bool WINAPI
    load_columns( LPSAFEARRAY* ppsa );

    BIDR_API bool WINAPI
    unload_column( LPVARIANT v_col_title );

    BIDR_API bool WINAPI
    unload_columns( LPSAFEARRAY* ppsa );

    BIDR_API bool WINAPI
    filter( LPVARIANT       v_col_title,
            const double&   cutoff,
            const uinteger& n_skip       = 2,
            const uinteger& max_range_sz = 0);

    BIDR_API bool WINAPI
    apply_reduction( LPVARIANT      v_col_title,
                    const uinteger& type      = 2,
                    const uinteger& reduction = 0,
                    const uinteger& average   = 0,
                    const uinteger& n_group   = 1,
                    const uinteger& n_point   = 0 );

    BIDR_API bool WINAPI
    is_initialized();

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
     *   - unload_columns: ----
     *   - filter: ----
     *   - apply_reduction: ----
     *   - clear_spreadsheet: DONE
     *   - is_initialized: DONE
     */
}
