#pragma once

#include "../BIDR_Defines.h"

namespace burn_in_data_report
{
    class file_settings
    {
    private:
        // Private vars
        std::string filename_;                      // Filename
        FileFormat                                  // FileFormat --> std::pair<std::string, nlohmann::json>
        format_;                                    // Config name + configuration in nlohmann::json format
        std::chrono::sys_time<nano>
        last_write_;                                // Last write time rel. to epoch (nanoseconds)
        std::chrono::sys_time<nano>
        start_time_;                                // Test start time rel. to epoch (nanoseconds)
        nano
        measurement_period_;                        // Time period between each measurement (nanoseconds)
        TypeMap
        col_types_;                                 // Represents the expected data type of each column
        uinteger n_cols_;                           // Number of columns
        uinteger n_rows_;                           // Number of rows
        uinteger data_pos_;                         // Row data begins
        uinteger
        header_lim_;                                // Header Limit (end point exclusive). Data starts on this row.
        nlohmann::json config_;                     // Config for the file in question
        uinteger
        header_max_lim_;                            // Maximum value for the header limit, by default=256

    public:
        // Public setters
        void set_filename( const std::string& filename ) noexcept { filename_ = filename; }

        void set_format( const FileFormat& format ) noexcept { format_ = format; }

        void set_last_write( const std::chrono::sys_time<nano>& last_write ) noexcept { last_write_ = last_write; }

        void set_start_time( const std::chrono::sys_time<nano>& start_time ) noexcept { start_time_ = start_time; }

        void set_measurement_period( const nano& measurement_period ) noexcept {
            measurement_period_ = measurement_period;
        }

        void set_data_pos( const uinteger& data_pos ) noexcept { data_pos_ = data_pos; }

        void set_col_types( const TypeMap& col_types ) noexcept { col_types_ = col_types; }

        void set_n_cols( const uinteger& n_cols ) noexcept { n_cols_ = n_cols; }

        void set_n_rows( const uinteger& n_rows ) noexcept { n_rows_ = n_rows; }

        void set_header_lim( const uinteger& header_lim ) { header_lim_ = header_lim; }

        void set_config( const nlohmann::json& config ) noexcept { config_ = config; }

        void set_headermaxlim( const uinteger& header_max_lim ) noexcept { header_max_lim_ = header_max_lim; }

        // Public access to vectors
        TypeMap& col_types() noexcept { return col_types_; }

        file_settings( const std::string& filename, const uinteger& header_max_lim ) :
            filename_(filename),
            format_(FileFormat("", "{}")),
            last_write_(std::chrono::sys_time<nano> {}),
            start_time_(std::chrono::sys_time<nano> {}),
            measurement_period_(nano::zero()),
            col_types_({}),
            n_cols_(0),
            n_rows_(0),
            data_pos_(0),
            header_lim_(0),
            config_(0),
            header_max_lim_(header_max_lim) {}

        ~file_settings() noexcept = default;                                       // destructor.
        file_settings( const file_settings& _other ) noexcept = default;              // copy const.
        file_settings&
        operator=( const file_settings& _other ) noexcept = default;   // copy assign.
        file_settings( file_settings&& _other ) noexcept   = default;                   // move const.
        file_settings&
        operator=( file_settings&& _other ) noexcept = default;        // move assign.

        const FileFormat get_format() const noexcept { return format_; }

        const std::string get_filename() const noexcept { return filename_; }

        const std::chrono::sys_time<nano> get_last_write() const noexcept { return last_write_; }

        const std::chrono::sys_time<nano> get_start_time() const noexcept { return start_time_; }

        const nano get_measurement_period() const noexcept { return measurement_period_; }

        const uinteger get_data_pos() const noexcept { return data_pos_; }

        const std::vector<std::string> get_col_titles() const noexcept { return get_keys(col_types_); }

        const TypeMap get_col_types() const noexcept { return col_types_; }

        const uinteger get_n_cols() const noexcept { return n_cols_; }

        const uinteger get_n_rows() const noexcept { return n_rows_; }

        const uinteger get_header_lim() const noexcept { return header_lim_; }

        const nlohmann::json get_config() const noexcept { return config_; }

        const uinteger get_headermaxlim() const noexcept { return header_max_lim_; }

        const DataType
        get_type( const std::string& _key ) const noexcept;

        FileFormat& get_format() noexcept { return format_; }

        std::string& get_filename() noexcept { return filename_; }

        std::chrono::sys_time<nano>& get_last_write() noexcept { return last_write_; }

        std::chrono::sys_time<nano>& get_start_time() noexcept { return start_time_; }

        nano& get_measurement_period() noexcept { return measurement_period_; }

        uinteger& get_data_pos() noexcept { return data_pos_; }

        TypeMap& get_col_types() noexcept { return col_types_; }

        uinteger& get_n_cols() noexcept { return n_cols_; }

        uinteger& get_n_rows() noexcept { return n_rows_; }

        uinteger& get_header_lim() noexcept { return header_lim_; }

        nlohmann::json& get_config() noexcept { return config_; }

        uinteger& get_headermaxlim() noexcept { return header_max_lim_; }
    };


    inline const DataType
    file_settings::get_type( const std::string& _key ) const noexcept {
        try { return col_types_.at(_key); }
        catch ( const std::exception& err ) {
            write_err_log( err, "DLL: <file_settings::get_type>" );
            return DataType::NONE;
        }
    }
} // NAMESPACE: burn_in_data_report
