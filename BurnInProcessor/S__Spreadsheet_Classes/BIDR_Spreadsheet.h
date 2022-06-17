#pragma once

#include "../F__Folder_Funcs/BIDR_FolderSearch.h"
#include "../S__Datastructures/BIDR_FileData.h"


namespace burn_in_data_report
{
    enum class avg_type
    {
        stable_mean,
        overall_mean,
        stable_median,
        overall_median
    };


    class spreadsheet
    {
    private:
        file_data file_;
        std::vector<std::pair<uinteger, uinteger>> filters_;
        // filter currently applied to data

        IMap int_data_; // int data
        DMap i_errors_; // int error vals
        DMap double_data_; // double data
        DMap d_errors_; // double error vals
        SMap string_data_; // string data
        DMap s_errors_; // string error vals (always filled with NaN)
        std::map<std::string, DataType> type_map_; // Maps keys to data type

        uinteger n_rows_;
        reduction_type reduction_type_; // Vars for reduced data
        avg_type average_type_;
        uinteger n_group_;
        uinteger n_points_;

        // Flag to check if global variable for dll
        // is initialized
        bool initialized_;

        [[nodiscard]] bool
        update_n_rows();

        bool
        apply_reduction(
            const std::string& _key,
            const reduction_type& _r_type = reduction_type::DEFAULT,
            const avg_type& _a_type = avg_type::stable_mean,
            const uinteger& _n_group = 1,
            const uinteger& _n_points = 0
        ) noexcept;

    public:
        spreadsheet();
        explicit
        spreadsheet( const std::vector<std::string>& filenames,
                     const std::string& config_loc_name,
                     const uinteger& max_header_sz,
                     const double& max_off_time_minutes,
                     const bool& do_trimming );
        explicit
        spreadsheet( const std::vector<std::filesystem::directory_entry>& files,
                     const std::filesystem::directory_entry& config_loc,
                     const uinteger& max_header_sz,
                     const nano& max_off_time,
                     const bool& do_trimming );
        explicit
        spreadsheet( const spreadsheet& other );
        spreadsheet&
        operator=( const spreadsheet& other );
        explicit
        spreadsheet( spreadsheet&& other ) noexcept;
        spreadsheet&
        operator=( spreadsheet&& other ) noexcept;
        ~spreadsheet() = default;

        /* (Load, Unload) (files, models) */
        bool
        load_files(
            const std::vector<std::filesystem::directory_entry>& files,
            const std::string& f_key,
            const std::filesystem::directory_entry& config_loc,
            const uinteger& header_max_lim,
            const nano& max_off_time
        ) noexcept;
        bool
        add_file( const std::filesystem::directory_entry& file ) noexcept;
        bool
        add_files(
            const std::vector<std::filesystem::directory_entry>& files ) noexcept;
        bool
        remove_file( const uinteger& index ) noexcept;
        bool
        remove_files( const std::vector<uinteger>& indexes ) noexcept;
        [[nodiscard]] std::vector<std::string>
        get_current_cols() const noexcept;
        [[nodiscard]] std::vector<std::string>
        get_available_cols() const noexcept;

        // Retrieval & functions to alter the data.
        bool
        load_column( const std::string& _key ) noexcept;
        bool
        load_columns( const std::vector<std::string>& _keys ) noexcept;
        bool
        unload_column( const std::string& _key ) noexcept;
        bool
        unload_columns( const std::vector<std::string>& _keys ) noexcept;
        bool
        filter( const std::string& _key,
                const double& _cutoff,
                const uinteger& _n,
                const uinteger& _max_range_sz ) noexcept;

        bool
        reduce( const reduction_type& r_type,
                const avg_type& a_type,
                const uinteger& n_group,
                const uinteger& n_points ) noexcept;

        bool
        clear_spreadsheet() noexcept;

        [[nodiscard]] uinteger
        n_rows() const noexcept {
            if ( filters_.empty() ) {
                return n_rows_;
            }
            else {
                uinteger size =
                    std::accumulate(filters_.cbegin(), filters_.cend(),
                                    static_cast<uinteger>(0),
                                    [](uinteger x, const std::pair<uinteger, uinteger>& p)
                                    { return x + (p.second - p.first); });
                return size;
            }
        }

        [[nodiscard]] bool
        is_initialized() const noexcept { return initialized_; }

        [[nodiscard]] std::vector<integer>
        get_i( const std::string& key ) const noexcept;

        [[nodiscard]] std::vector<double>
        get_d( const std::string& key ) const noexcept;

        [[nodiscard]] std::vector<std::string>
        get_s( const std::string& key ) const noexcept;

        [[nodiscard]] std::vector<double>
        get_error( const std::string& key ) const noexcept;

        [[nodiscard]] bool
        contains( const std::string& key ) const noexcept;

        [[nodiscard]] DataType
        type( const std::string& key ) const noexcept;

    };


    inline
    spreadsheet::spreadsheet() :
        n_rows_(0),
        reduction_type_(reduction_type::DEFAULT),
        average_type_(avg_type::stable_mean),
        n_group_(1),
        n_points_(0),
        initialized_(false) {}

    inline
    spreadsheet::spreadsheet( const std::vector<std::string>& filenames,
                              const std::string& config_loc_name,
                              const uinteger& max_header_sz = 256,
                              const double& max_off_time_minutes = 5.0,
                              const bool& do_trimming = true ) :
        n_rows_(0),
        reduction_type_(reduction_type::DEFAULT),
        average_type_(avg_type::stable_mean),
        n_group_(1),
        n_points_(0),
        initialized_(false) {
        try {
            write_log("Initializing spreadsheet.");
            std::vector<std::filesystem::directory_entry> files {};
            for ( const auto& filename : filenames ) {
                std::filesystem::directory_entry file { filename };
                if ( file.exists() && file.is_regular_file() ) { files.push_back(file); }
            }
            const std::filesystem::directory_entry config_location {
                    config_loc_name
                };
            if ( !config_location.exists() ) {
                throw std::runtime_error {
                        std::format("Failed to find config file location: {}.", config_loc_name)
                    };
            }
            if ( config_location.hard_link_count() == 0 ) {
                throw std::runtime_error {
                        std::format("Failed to find config file(s) in {}, count = {}.",
                                    config_loc_name, config_location.hard_link_count())
                    };
            }

            if ( max_off_time_minutes <= 0.0 ) {
                throw std::runtime_error {
                        std::format("Invalid time received: {}", max_off_time_minutes)
                    };
            }
            if ( max_off_time_minutes < 3.0 ) {
                write_err_log(
                              std::runtime_error(
                                                 std::format(
                                                             "WARNING: If laser is cycling on and off with an off period shorter than {}, all data from the off cycles will be removed.\n",
                                                             max_off_time_minutes
                                                            )
                                                )
                             );
            }
            using minutes = std::chrono::duration<double, std::ratio<60, 1>>;
            const auto max_off_time =
                std::chrono::duration_cast<nano>(minutes { max_off_time_minutes });

            if ( !files.empty() ) {
                file_ = file_data {
                        files,
                        config_location,
                        max_header_sz,
                        max_off_time,
                        do_trimming
                    };
                initialized_ = true;
                write_log("Spreadsheet initialized.");
            }
            else { write_log("Initialization failed."); }
        }
        catch ( const std::exception& err ) {
            throw std::runtime_error(err.what());
        }
    }

    // COMPLETE?
    inline
    spreadsheet::spreadsheet(
        const std::vector<std::filesystem::directory_entry>& files,
        const std::filesystem::directory_entry& config_loc,
        const uinteger& max_header_sz = 256,
        const nano& max_off_time = 5min,
        const bool& do_trimming = true ) :
        file_(files, config_loc, max_header_sz, max_off_time, do_trimming),
        n_rows_(0),
        reduction_type_(reduction_type::DEFAULT),
        average_type_(avg_type::stable_mean),
        n_group_(1),
        n_points_(0),
        initialized_(true) {}

    inline spreadsheet&
    spreadsheet::operator=( const spreadsheet& other ) {
        file_ = other.file_;
        int_data_ = other.int_data_;
        i_errors_ = other.i_errors_;
        double_data_ = other.double_data_;
        d_errors_ = other.d_errors_;
        string_data_ = other.string_data_;
        s_errors_ = other.s_errors_;
        type_map_ = other.type_map_;
        n_rows_ = other.n_rows_;
        reduction_type_ = other.reduction_type_;
        average_type_ = other.average_type_;
        n_group_ = other.n_group_;
        n_points_ = other.n_points_;
        initialized_ = other.initialized_;

        return *this;
    }

    inline
    spreadsheet::spreadsheet( const spreadsheet& other ) :
        file_(other.file_),
        int_data_(other.int_data_),
        i_errors_(other.i_errors_),
        double_data_(other.double_data_),
        d_errors_(other.d_errors_),
        string_data_(other.string_data_),
        s_errors_(other.s_errors_),
        type_map_(other.type_map_),
        n_rows_(other.n_rows_),
        reduction_type_(other.reduction_type_),
        average_type_(other.average_type_),
        n_group_(other.n_group_),
        n_points_(other.n_points_),
        initialized_(other.initialized_) {}

    inline spreadsheet&
    spreadsheet::operator=( spreadsheet&& other ) noexcept {
        file_ = std::move(other.file_);
        int_data_ = std::move(other.int_data_);
        i_errors_ = std::move(other.i_errors_);
        double_data_ = std::move(other.double_data_);
        d_errors_ = std::move(other.d_errors_);
        string_data_ = std::move(other.string_data_);
        s_errors_ = std::move(other.s_errors_);
        type_map_ = std::move(other.type_map_);
        n_rows_ = other.n_rows_;
        reduction_type_ = other.reduction_type_;
        average_type_ = other.average_type_;
        n_group_ = other.n_group_;
        n_points_ = other.n_points_;
        initialized_ = other.initialized_;

        return *this;
    }

    inline
    spreadsheet::spreadsheet( spreadsheet&& other ) noexcept :
        file_(std::move(other.file_)),
        int_data_(std::move(other.int_data_)),
        i_errors_(std::move(other.i_errors_)),
        double_data_(std::move(other.double_data_)),
        d_errors_(std::move(other.d_errors_)),
        string_data_(std::move(other.string_data_)),
        s_errors_(std::move(other.s_errors_)),
        type_map_(std::move(other.type_map_)),
        n_rows_(other.n_rows_),
        reduction_type_(other.reduction_type_),
        average_type_(other.average_type_),
        n_group_(other.n_group_),
        n_points_(other.n_points_),
        initialized_(other.initialized_) {}

    [[nodiscard]] inline bool
    spreadsheet::update_n_rows() {
        bool result{ true };
        uinteger size{ 0 };
        write_log("update_n_rows:");

        for ( const auto& [key, type] : type_map_ ) {
            write_log(std::format("({}, {}):", key, type_string.at(type)));
            switch ( type ) {
            case DataType::INTEGER: {
                if ( size == 0 ) {
                    size = int_data_.at(key).size();
                    write_log(std::format("\tsetting size = {}", size));
                }
                else {
                    if ( size != int_data_.at(key).size() ) {
                        write_log(std::format("\tsize = {}, data size = {}", size, int_data_.at(key).size()));
                        write_err_log( std::runtime_error("\tDLL: <spreadsheet::update_n_rows> Data length mismatch."));
                        result = false;
                    }
                    else {
                        write_log(std::format("\t{}", size));
                    }
                }
            }
                break;
            case DataType::DOUBLE: {
                if ( size == 0 ) {
                    size = double_data_.at(key).size();
                    write_log(std::format("\tsetting size = {}", size));
                }
                else {
                    if ( size != double_data_.at(key).size() ) {
                        write_log(std::format("\tsize = {}, data size = {}", size, double_data_.at(key).size()));
                        write_err_log( std::runtime_error("DLL: <spreadsheet::update_n_rows> Data length mismatch."));
                        result = false;
                    }
                    else {
                        write_log(std::format("\t{}", size));
                    }
                }
            }
                break;
            case DataType::STRING: {
                if ( size == 0 ) {
                    size = string_data_.at(key).size();
                    write_log(std::format("\tsetting size = {}", size));
                }
                else {
                    if ( size != string_data_.at(key).size() ) {
                        write_log(std::format("\tsize = {}, data size = {}", size, string_data_.at(key).size()));
                        write_err_log( std::runtime_error("DLL: <spreadsheet::update_n_rows> Data length mismatch."));
                        result = false;
                    }
                    else {
                        write_log(std::format("\t{}", size));
                    }
                }
            }
                break;
            case DataType::NONE:
                result = false;
                break;
            }
        }

        if ( result ) {
            n_rows_ = size;
        }

        return result;
    }

    // COMPLETE?
    inline bool
    spreadsheet::load_files(
        const std::vector<std::filesystem::directory_entry>& files,
        const std::string& f_key = "PDOF 1",
        const std::filesystem::directory_entry& config_loc =
            std::filesystem::directory_entry { "" },
        const uinteger& header_max_lim = 256,
        const nano& max_off_time = 5min
    ) noexcept {
        try {
            file_ = file_data { files, config_loc, header_max_lim, max_off_time };
            return check_valid_state(file_.get_load_info());
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <spreadsheet::load_files");
            return false;
        }
    }

    // COMPLETE?
    inline bool
    spreadsheet::add_file( const std::filesystem::directory_entry& file ) noexcept {
        try { return file_.add_file(file); }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <spreadsheet::add_file>");
            return false;
        }
    }

    // COMPLETE?
    inline bool
    spreadsheet::add_files(
        const std::vector<std::filesystem::directory_entry>& files ) noexcept {
        try { return file_.add_files(files); }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <spreadsheet::add_files>");
            return false;
        }
    }

    // COMPLETE?
    inline bool
    spreadsheet::remove_file( const uinteger& index ) noexcept {
        try { return file_.remove_file(index); }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <spreadsheet::remove_file>");
            return false;
        }
    }

    // COMPLETE?
    inline bool
    spreadsheet::remove_files( const std::vector<uinteger>& indexes ) noexcept {
        try { return file_.remove_files(indexes); }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <spreadsheet::remove_files");
            return false;
        }
    }

    // COMPLETE?
    inline bool
    spreadsheet::apply_reduction(
        // Column title
        const std::string& _key,
        // 3 bit flag indicating how to apply reduction. Default: {100}
        const reduction_type& _r_type,
        // Indicates how to apply reduction. Default: {avg_type::stable_mean}
        const avg_type& _a_type,
        // Averaging by groups of n_group_ points. Default: 1
        const uinteger& _n_group,
        // Averaging to produce n_points_ at the end. Default: 0
        const uinteger& _n_points
    ) noexcept {
        try {
            /*
            * All reductions are done in place, so data must be reset to get
            * original state back.
            */

            // Check _key is valid
            if ( !type_map_.contains(_key) )
                return false;

            std::vector<integer> i_reduced;
            std::vector<double> d_reduced;
            std::vector<std::string> s_reduced;
            std::vector<double> std_deviations;
            auto filters_copy(filters_);
            const DataType type = type_map_.at(_key);

            // get current no. rows. Will be updated as reductions applied
            uinteger no_rows;
            switch ( type ) {
            case DataType::INTEGER:
                no_rows = int_data_.at(_key).size();
                break;
            case DataType::DOUBLE:
                no_rows = double_data_.at(_key).size();
                break;
            case DataType::STRING:
                no_rows = string_data_.at(_key).size();
                break;
            case DataType::NONE:
                break;
            }

            // No reduction if it isn't set & no_rows < MAX_ROWS
            // If no_rows > MAX_ROWS, MUST be reduced to fit in excel
            if ( _r_type == reduction_type::none && no_rows < MAX_ROWS )
                return true;

            // Average by each cycle, this will likely always be true
            // Still may be useful to have the option to not use it
            if ( !filters_copy.empty() || no_rows > MAX_ROWS ) {
                // Need to think of acceptable way to do strings
                // For now will use lambda which returns NaN values
                // As it doesn't make much sense to "take the average
                // of a string"

                // Calculate averages
                switch ( type ) {
                case DataType::INTEGER: {
                    switch ( _a_type ) {
                    case avg_type::overall_mean: {
                        if ( !i_errors_.contains(_key) )
                            std_deviations.clear();
                        else
                            std_deviations = i_errors_.at(_key);

                        const auto& [mean_values, std_deviation_values] =
                            cycle_average(int_data_.at(_key), filters_copy,
                                          std_deviations, mean, stdev);

                        int_data_[_key].clear();
                        int_data_[_key].reserve(mean_values.size());
                        for ( const auto& val : mean_values ) {
                            int_data_[_key].emplace_back(
                                                         static_cast<integer>( val ));
                        }
                        i_errors_[_key] = std_deviation_values;
                        break;
                    }
                    case avg_type::overall_median: {
                        if ( !i_errors_.contains(_key) )
                            std_deviations.clear();
                        else
                            std_deviations = i_errors_.at(_key);

                        const auto& [median_values, std_deviation_values] =
                            cycle_average(int_data_.at(_key),
                                          filters_copy,
                                          std_deviations,
                                          median,
                                          stdev);

                        int_data_[_key].clear();
                        int_data_[_key].reserve(median_values.size());
                        for ( const auto& val : median_values ) {
                            int_data_[_key].emplace_back(
                                                         static_cast<integer>( val ));
                        }
                        i_errors_[_key] = std_deviation_values;
                        break;
                    }
                    case avg_type::stable_mean: {
                        if ( !i_errors_.contains(_key) )
                            std_deviations.clear();
                        else
                            std_deviations = i_errors_.at(_key);

                        const auto& [s_mean_values, s_std_deviation_values] =
                            cycle_average(
                                          int_data_.at(_key),
                                          filters_copy,
                                          std_deviations,
                                          stable_mean,
                                          stable_stdev);

                        int_data_[_key].clear();
                        int_data_[_key].reserve(s_mean_values.size());
                        for ( const auto& val : s_mean_values ) {
                            int_data_[_key].emplace_back(
                                                         static_cast<integer>( val ));
                        }
                        i_errors_[_key] = s_std_deviation_values;
                        break;
                    }
                    case avg_type::stable_median: {
                        if ( !i_errors_.contains(_key) )
                            std_deviations.clear();
                        else
                            std_deviations = i_errors_.at(_key);

                        const auto& [s_median_values, s_std_deviation_values] =
                            cycle_average(
                                          int_data_.at(_key),
                                          filters_copy,
                                          std_deviations,
                                          stable_median,
                                          stable_stdev);

                        int_data_[_key].clear();
                        int_data_[_key].reserve(s_median_values.size());
                        for ( const auto& val : s_median_values ) {
                            int_data_[_key].emplace_back(
                                                         static_cast<integer>( val ));
                        }
                        i_errors_[_key] = s_std_deviation_values;
                        break;
                    }
                    }
                    no_rows = int_data_.at(_key).size();
                    break;
                }
                case DataType::DOUBLE: {
                    switch ( _a_type ) {
                    case avg_type::overall_mean: {
                        if ( !d_errors_.contains(_key) )
                            std_deviations.clear();
                        else
                            std_deviations = d_errors_.at(_key);

                        const auto& [mean_values, std_deviation_values] =
                            cycle_average(
                                          double_data_.at(_key),
                                          filters_copy,
                                          std_deviations,
                                          mean,
                                          stdev);

                        double_data_[_key] = mean_values;
                        d_errors_[_key] = std_deviation_values;
                        break;
                    }
                    case avg_type::overall_median: {
                        if ( !d_errors_.contains(_key) )
                            std_deviations.clear();
                        else
                            std_deviations = d_errors_.at(_key);

                        const auto& [median_values, std_deviation_values] =
                            cycle_average(double_data_.at(_key),
                                          filters_copy,
                                          std_deviations,
                                          median,
                                          stdev);

                        double_data_[_key] = median_values;
                        d_errors_[_key] = std_deviation_values;
                        break;
                    }
                    case avg_type::stable_mean: {
                        if ( !d_errors_.contains(_key) )
                            std_deviations.clear();
                        else
                            std_deviations = d_errors_.at(_key);

                        const auto& [s_mean_values, s_std_deviation_values] =
                            cycle_average(double_data_.at(_key),
                                          filters_copy,
                                          std_deviations,
                                          stable_mean,
                                          stable_stdev);
                        double_data_[_key] = s_mean_values;
                        d_errors_[_key] = s_std_deviation_values;
                        break;
                    }
                    case avg_type::stable_median: {
                        if ( !d_errors_.contains(_key) )
                            std_deviations.clear();
                        else
                            std_deviations = d_errors_.at(_key);

                        auto [s_median_values, s_std_deviation_values] =
                            cycle_average(
                                          double_data_.at(_key),
                                          filters_copy,
                                          std_deviations,
                                          stable_median,
                                          stable_stdev);

                        double_data_[_key] = s_median_values;
                        d_errors_[_key] = s_std_deviation_values;
                        break;
                    }
                    }
                    no_rows = double_data_.at(_key).size();
                    
                    break;
                }
                case DataType::STRING: {
                    // TODO: Find a better solution for this?
                    // Currently just return NaN
                    const auto& data = string_data_.at(_key);

                    std::vector<std::string> tmp;
                    tmp.reserve(filters_copy.size());

                    for ( const auto& [first, last] : filters_copy ) {
                        uinteger idx { (last - first) / 2 };
                        tmp.emplace_back(data[idx]);
                    }

                    string_data_[_key] = tmp;
                    s_errors_[_key] =
                        std::vector(filters_copy.size(),
                                    std::numeric_limits<double>::signaling_NaN());

                    no_rows = string_data_.at(_key).size();
                    break;
                }
                case DataType::NONE: { break; }
                }

                // Clear filter now cycles have been
                // avg'd into single data points
                filters_copy.clear();

                write_log("Done.");
            }

            // Average by groups of n_group_ points
            if ( (_r_type == reduction_type::ngroup
                  || _r_type == reduction_type::all)
                 && _n_group > 0
                 && _n_group < file_.get_n_rows() ) {
                write_log("Averaging by group...");

                // Tmp storage to pass to AVG function

                // Lambda function to handle averaging
                auto avg
                    = []<typename K>
                ( const std::vector<K>& data,
                  std::vector<K>& reduced_storage,
                  std::vector<double>& std_deviations,
                  const std::vector<std::pair<uinteger, uinteger>>& filters,
                  const uinteger& n_group )
                -> bool {
                    try {
                        // Clear any existing data in results storage
                        if ( !reduced_storage.empty() )
                            reduced_storage.clear();

                        // Storage for copy of data about to be reduced
                        std::vector<K> tmp;
                        std::vector<double> std_deviation_tmp;

                        /*
                        * If filter exists:
                        * - Calc. num. of datapoints in filter
                        * - Add data & stdevs to temporary arrays
                        */
                        if ( !filters.empty() ) {
                            uinteger sz = std::accumulate(
                                                          filters.cbegin(), filters.cend(),
                                                          static_cast<uinteger>( 0 ),
                                                          []( const uinteger& lhs,
                                                              const std::pair<uinteger, uinteger>& rhs ) {
                                                              return lhs + (rhs.second - rhs.first);
                                                          }
                                                         );

                            tmp.reserve(sz);
                            std_deviation_tmp.reserve(sz);

                            for ( const auto& [first, last] : filters ) {
                                tmp.insert(tmp.end(), data.cbegin() + first,
                                           data.cbegin() + last);
                                std_deviation_tmp.insert(std_deviation_tmp.end(),
                                                         std_deviations.cbegin() + static_cast<integer>( first ),
                                                         std_deviations.cbegin() + static_cast<integer>( last ));
                            }

                            tmp.shrink_to_fit();
                            std_deviation_tmp.shrink_to_fit();
                        }
                        else {
                            tmp = data;
                            std_deviation_tmp = std_deviations;
                        }

                        /*
                        * Calculate no. rows at end of process.
                        * Overflow handles remaining points if no_rows % n_group_ > 0
                        * by adding an extra point to the last x rows where x is the remainder
                        */
                        uinteger total_rows { tmp.size() / n_group }, overflow {
                                         tmp.size() % n_group
                                     };
                        overflow += ((overflow > 0)
                                         ? 1
                                         : 0);
                        reduced_storage.reserve(total_rows + overflow);
                        std_deviations.clear();
                        std_deviations.reserve(total_rows + overflow);

                        // Perform averaging:
                        uinteger startpoint { 0 };
                        for ( uinteger i { 0 }; i < total_rows; ++i ) {
                            reduced_storage.emplace_back(
                                                         static_cast<K>( mean(
                                                                              tmp, startpoint, startpoint + n_group,
                                                                              std_deviation_tmp) ));
                            std_deviations.emplace_back(stdev(
                                                              tmp, startpoint, startpoint + n_group,
                                                              static_cast<double>( reduced_storage[
                                                                  reduced_storage.size() - 1] ),
                                                              std_deviation_tmp));
                            startpoint += n_group;
                        }

                        // Average final row
                        if ( overflow > 0 ) {
                            reduced_storage.emplace_back(
                                                         static_cast<K>( mean(
                                                                              tmp, startpoint,
                                                                              startpoint + overflow) ));
                            std_deviations.emplace_back(stdev(
                                                              tmp, startpoint, startpoint + overflow,
                                                              static_cast<double>( reduced_storage[
                                                                  reduced_storage.size() - 1] ),
                                                              std_deviation_tmp));
                        }

                        reduced_storage.shrink_to_fit();
                        std_deviations.shrink_to_fit();

                        return true;
                    }
                    catch ( const std::exception& err ) {
                        write_err_log(err, "DLL: <spreadsheet::apply_reduction::avg>");
                        return false;
                    }
                };

                // Perform averaging for each column currently loaded
                switch ( type ) {
                case DataType::INTEGER: {
                    if ( i_errors_.contains(_key) )
                        std_deviations = i_errors_.at(_key);
                    else
                        std_deviations.clear();

                    avg(int_data_.at(_key),
                        i_reduced,
                        std_deviations,
                        filters_copy,
                        _n_group);

                    int_data_[_key] = i_reduced;
                    i_errors_[_key] = std_deviations;
                    break;
                }
                case DataType::DOUBLE: {
                    if ( d_errors_.contains(_key) ) { std_deviations = d_errors_.at(_key); }
                    else { std_deviations.clear(); }

                    avg(double_data_.at(_key),
                        d_reduced,
                        std_deviations,
                        filters_copy,
                        _n_group);

                    double_data_[_key] = d_reduced;
                    d_errors_[_key] = std_deviations;
                    break;
                }
                case DataType::STRING: {
                    auto& data { string_data_.at(_key) };
                    uinteger sz;
                    if ( !filters_copy.empty() ) {
                        sz = std::accumulate(filters_copy.cbegin(), filters_copy.cend(),
                                             static_cast<uinteger>( 0 ),
                                             []( const uinteger& lhs,
                                                 const std::pair<uinteger, uinteger>
                                                 & rhs ) {
                                                 return lhs + (rhs.second - rhs.
                                                               first);
                                             });
                    }
                    else
                        sz = data.size();

                    uinteger _no_rows { sz / _n_group }, offset { sz % _n_group };
                    no_rows += ((offset == 0)
                                    ? 0
                                    : 1);
                    s_reduced.reserve(_no_rows);
                    for ( uinteger i { 0 }; i < _no_rows; ++i ) {
                        const auto start {
                                std::next(data.begin(), static_cast<integer>( i ) * _n_group)
                            };
                        const auto end {
                                std::next(data.begin(),
                                          (i == _no_rows - 1)
                                              ? offset
                                              : _n_group)
                            };
                        s_reduced.emplace_back(concatenate(start, end, ","));
                    }
                    std_deviations.insert(
                                          std_deviations.end(),
                                          _no_rows + offset,
                                          std::numeric_limits<double>::signaling_NaN()
                                         );
                    string_data_[_key] = s_reduced;
                    s_errors_[_key] = std_deviations;
                    break;
                }
                case DataType::NONE: { break; }
                }

                write_log("Done.");
            }

            // Average if no_rows > _max_points (user specified) OR no_rows > MAX_ROWS (excel specified)
            // (Is this test type selected? and parameter n_points_ is valid?
            // OR is n_points_ > MAX_ROWS OR is _no_rows (pre-averaging) > MAX_ROWS
            if ( (_r_type == reduction_type::npoints ||
                  _r_type == reduction_type::all) &&
                 _n_points > 0 &&
                 _n_points < file_.get_n_rows() ||
                 _n_points > MAX_ROWS ||
                 no_rows > MAX_ROWS ) {
                auto avg =
                    []<ArithmeticType R>
                ( const std::vector<R>& data,
                  std::vector<R>& reduced_storage,
                  std::vector<double>& stdevs,
                  const std::vector<std::pair<uinteger, uinteger>>& filters,
                  const uinteger& _n_points ) -> void {
                    // Storage for tmp data
                    std::vector<R> tmp;
                    std::vector<double> std_deviation_tmp;

                    uinteger sz { 0 };
                    if ( !filters.empty() ) {
                        sz = std::accumulate(filters.cbegin(), filters.cend(),
                                             static_cast<uinteger>( 0 ),
                                             []( uinteger lhs,
                                                 const std::pair<uinteger, uinteger>
                                                 & pair ) {
                                                 return lhs + (pair.second - pair.
                                                               first);
                                             });
                        tmp.reserve(sz);
                        std_deviation_tmp.reserve(sz);
                    }
                    else {
                        tmp.reserve(data.size());
                        std_deviation_tmp.reserve(data.size());
                    }

                    // Calculate num of points per grouping.
                    // If provided max. num. of points > MAX_ROWS (excel limit), default to MAX_ROWS
                    const uinteger n_points = (
                        ((MAX_ROWS < _n_points) || (_n_points == 0))
                            ? MAX_ROWS
                            : _n_points);
                    const uinteger n_group { sz / _n_points }, overflow {
                                           sz % _n_points
                                       };

                    // Handle n(n_points_ - overflow) points
                    for ( uinteger i { 0 }; i < _n_points - overflow; ++i ) {
                        tmp.emplace_back(static_cast<R>(
                                             mean(data,
                                                  i * n_group,
                                                  (i + 1) * n_group,
                                                  stdevs) )
                                        );
                        std_deviation_tmp.emplace_back(
                                                       stdev(data,
                                                             i * n_group,
                                                             (i + 1) * n_group,
                                                             static_cast<double>( tmp[tmp.size() - 1] ),
                                                             stdevs)
                                                      );
                    }

                    // Handle last n(overflow) points adding 1 extra per point so total = n_points_
                    for ( uinteger i { n_points - overflow }; i < n_points; ++i ) {
                        tmp.emplace_back(static_cast<R>(
                                             mean(data,
                                                  i * (n_group + 1),
                                                  (i + 1) * (n_group + 1),
                                                  stdevs) )
                                        );
                        std_deviation_tmp.emplace_back(
                                                       stdev(data,
                                                             i * (n_group + 1),
                                                             (i + 1) * (n_group + 1),
                                                             static_cast<double>( tmp[tmp.size() - 1] ),
                                                             stdevs)
                                                      );
                    }

                    tmp.shrink_to_fit();
                    std_deviation_tmp.shrink_to_fit();

                    reduced_storage = tmp;
                    stdevs = std_deviation_tmp;
                };

                switch ( type ) {
                case DataType::INTEGER: {
                    if ( i_errors_.contains(_key) )
                        std_deviations = i_errors_.at(_key);
                    else
                        std_deviations.clear();

                    avg(int_data_.at(_key),
                        i_reduced,
                        std_deviations,
                        filters_copy,
                        _n_points);

                    int_data_[_key] = i_reduced;
                    i_errors_[_key] = std_deviations;
                    break;
                }
                case DataType::DOUBLE: {
                    if ( d_errors_.contains(_key) )
                        std_deviations = d_errors_.at(_key);
                    else
                        std_deviations.clear();

                    avg(double_data_.at(_key),
                        d_reduced,
                        std_deviations,
                        filters_copy,
                        _n_points);

                    double_data_[_key] = d_reduced;
                    d_errors_[_key] = std_deviations;
                    break;
                }
                case DataType::STRING: {
                    auto& data = string_data_.at(_key);
                    uinteger sz;
                    if ( !filters_.empty() ) {
                        sz = std::accumulate(filters_copy.cbegin(), filters_copy.cend(),
                                             static_cast<uinteger>( 0 ),
                                             []( uinteger lhs,
                                                 const std::pair<uinteger, uinteger>
                                                 & rhs ) {
                                                 return lhs + (rhs.second - rhs.
                                                               first);
                                             }
                                            );
                    }
                    else { sz = data.size(); }

                    const uinteger n_points = (
                        ((MAX_ROWS < _n_points) || (_n_points == 0))
                            ? MAX_ROWS
                            : _n_points);
                    const uinteger n_group { sz / n_points }, overflow {
                                           sz % n_points
                                       };

                    s_reduced.reserve(n_points);
                    for ( uinteger i { 0 }; i < n_points; ++i ) {
                        const auto start {
                                std::next(data.begin(), static_cast<integer>( i ) * n_group)
                            };
                        const auto end {
                                std::next(data.begin(),
                                          (i == n_points - overflow)
                                              ? static_cast<integer>( i ) * (static_cast<integer>( n_group ) + 1)
                                              : static_cast<integer>( i ) * (static_cast<integer>( n_group ) + 1) + 1)
                            };
                        s_reduced.emplace_back(
                                               concatenate(start, end, ",")
                                              );
                    }
                    std_deviations.insert(
                                          std_deviations.end(),
                                          n_points,
                                          std::numeric_limits<double>::signaling_NaN()
                                         );
                    string_data_[_key] = s_reduced;
                    s_errors_[_key] = std_deviations;
                    break;
                }
                case DataType::NONE: { break; }
                }

                write_log("Done.");
            }

            return true;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <spreadsheet::apply_reduction>");
            return false;
        }
    }

    inline bool
    spreadsheet::reduce( const reduction_type& r_type,
                         const avg_type& a_type,
                         const uinteger& n_group,
                         const uinteger& n_points ) noexcept {
        try {
            write_log(std::format("Reducing data... (current size: {})", n_rows_));

            bool result { true };

            for ( const auto& key : type_map_ | std::views::keys ) {
                write_log(std::format("Reducing {}...", key));
                result &= apply_reduction(key, r_type, a_type, n_group, n_points);
                write_log(result
                              ? "Success."
                              : "Fail.");
            }

            result &= update_n_rows();

            if ( result ) {
                filters_.clear();
            }

            write_log(std::format("Done. (new size: {})", n_rows_));

            return result;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <spreadsheet::reduce>");
            return false;
        }
    }

    // COMPLETE
    inline std::vector<std::string>
    spreadsheet::get_current_cols() const noexcept {
        try {
            std::vector<std::string> cols;
            cols.reserve(
                         int_data_.size() + double_data_.size() + string_data_.size());

            for ( const auto& title : int_data_ | std::views::keys ) { cols.emplace_back(title); }
            for ( const auto& title : double_data_ | std::views::keys ) { cols.emplace_back(title); }
            for ( const auto& title : string_data_ | std::views::keys ) { cols.emplace_back(title); }

            return cols;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <spreadsheet::get_current_cols>");
            return std::vector<std::string> {};
        }
    }

    // COMPLETE
    inline std::vector<std::string>
    spreadsheet::get_available_cols() const noexcept {
        try { return file_.get_col_titles(); }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <spreadsheet::get_available_cols>");
            return std::vector<std::string> {};
        }
    }

    // COMPLETE?
    inline bool
    spreadsheet::load_column( const std::string& _key ) noexcept {
        try {
            auto dtype { DataType::NONE };

            // Check column exists & not already loaded
            for ( const auto& [key, type] : file_.get_col_types() ) {
                if ( key == _key ) {
                    if ( type_map_.contains(key) /*|| _key == std::string{"Combined Time"}*/ ) {
                        // Column is already loaded, exit
                        write_log(std::format("<spreadsheet::load_column> Column already loaded ({}).", _key));
                        return true;
                    }
                    write_log(std::format("<spreadsheet::load_column> Loading {}", _key));
                    dtype = type;
                    break;
                }
            }

            if ( dtype == DataType::NONE ) {
                // key doesn't exist hmm, should probably call some sort of error here
                write_log(std::format("<spreadsheet::load_column> Column not found ({})", _key));
                return false;
            }

            bool flag { true };

            // Add to type_map_
            type_map_[_key] = dtype;

            // Load unfiltered data, then:
            // Apply filters & reductions
            switch ( dtype ) {
            case DataType::INTEGER: {
                int_data_[_key] = file_.get_i(_key);
                i_errors_[_key] = {};
                apply_reduction(_key,
                                reduction_type_,
                                average_type_,
                                n_group_,
                                n_points_);
                if ( !update_n_rows() ) {
                    throw std::runtime_error("DLL: <spreadsheet::load_column> Data length mismatch.");
                }
                break;
            }
            case DataType::DOUBLE: {
                double_data_[_key] = file_.get_d(_key);
                d_errors_[_key] = {};
                apply_reduction(_key,
                                reduction_type_,
                                average_type_,
                                n_group_,
                                n_points_);
                if ( !update_n_rows() ) {
                    throw std::runtime_error("DLL: <spreadsheet::load_column> Data length mismatch.");
                }
                break;
            }
            case DataType::STRING: {
                string_data_[_key] = file_.get_s(_key);
                s_errors_[_key] = {};
                apply_reduction(_key,
                                reduction_type_,
                                average_type_,
                                n_group_,
                                n_points_);
                if ( !update_n_rows() ) {
                    throw std::runtime_error("DLL: <spreadsheet::load_column> Data length mismatch.");
                }
                break;
            }
            case DataType::NONE:
                break;
            }

            return true;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <spreadsheet::load_column>");
            return false;
        }
    }

    // COMPLETE
    inline bool
    spreadsheet::load_columns( const std::vector<std::string>& _keys ) noexcept {
        try {
            bool result { true };
            for ( const auto& key : _keys ) {
                write_log(std::format("<spreadsheet::load_columns> loading: {}", key));
                result &= load_column(key);
                write_log(std::format("{}", result
                                                ? "success"
                                                : "failure"));
            }

            return result;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <spreadsheet::load_columns>");
            return false;
        }
    }

    // COMPLETE
    inline bool
    spreadsheet::unload_column( const std::string& _key ) noexcept {
        try {
            /*
            * - Check valid _key
            * - Remove from relevant *_data column
            * - Remove from relevant *_errors column
            */

            const auto type_data_iter { type_map_.find(_key) };

            // Invalid _key
            if ( type_data_iter == type_map_.end() ) {
                write_log(std::format("<spreadsheet::unload_column> Column not found ({})", _key));
                return false;
            }
            write_log(std::format("<spreadsheet::unload_column> Column found ({})", _key));
            const auto d_type = type_data_iter->second;
            type_map_.erase(type_data_iter);

            // Remove data
            switch ( d_type ) {
            case DataType::INTEGER:
                int_data_.erase(_key);
                i_errors_.erase(_key);
                break;
            case DataType::DOUBLE:
                double_data_.erase(_key);
                d_errors_.erase(_key);
                break;
            case DataType::STRING:
                string_data_.erase(_key);
                s_errors_.erase(_key);
                break;
            case DataType::NONE:
                write_err_log(std::runtime_error("DLL: <spreadsheet::unload_column> NONE type received."));
                return false;
            }

            return true;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <spreadsheet::unload_column>");
            return false;
        }
    }

    // COMPLETE
    inline bool
    spreadsheet::unload_columns( const std::vector<std::string>& _keys ) noexcept {
        try {
            bool result { true };
            for ( const auto& key : _keys ) {
                write_log(std::format("<spreadsheet::unload_columns> loading: {}", key));
                result &= unload_column(key);
                write_log(std::format("{}", result
                                                ? "success"
                                                : "failure"));
            }
            return result;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <spreadsheet::unload_columns>");
            return false;
        }
    }

    // COMPLETE?
    inline bool
    spreadsheet::filter( const std::string& _key,
                         const double& _cutoff = 0.8,
                         const uinteger& _n = 2,
                         const uinteger& _max_range_sz = 0 ) noexcept {
        try {
            // Check valid cutoff fraction & that _key exists / has been loaded
            if ( !(0 <= _cutoff && _cutoff < 1 && type_map_.contains( _key ) ) ) {
                throw std::runtime_error("Invalid filter parameters received.");
            }
            if ( type_map_.at( _key ) == DataType::STRING ||
                 type_map_.at( _key ) == DataType::NONE ) {
                throw std::runtime_error("Selected column type is unsupported.");
            }

            // get DataType of provided _key
            const DataType dtype = type_map_.at(_key);

            // Clear existing filters
            filters_.clear();

            // Map key --> cutoff value for filter
            std::map<std::string, integer> int_cutoff;
            std::map<std::string, double> double_cutoff;

            // Calculate cutoff values as fraction of max datapoint
            auto check_MAX =
                []<ArithmeticType T>( const std::vector<T>& data ) -> T {
                assert(!data.empty());
                T max { data[0] };
                for ( auto iter { data.begin() + 1 }; ++iter != data.end(); )
                    max = MAX(max, *iter);
                return max;
            };
            for ( const auto& [key, type] : type_map_ ) {
                switch ( type ) {
                case DataType::INTEGER:
                    int_cutoff[key] = static_cast<integer>(
                        _cutoff * static_cast<double>(
                            check_MAX(int_data_.at(key)) ) );
                    if ( key == _key ) {
                        write_log(std::format("\t- {} Cutoff: {}", _key, int_cutoff[key]));
                    }
                    break;
                case DataType::DOUBLE:
                    double_cutoff[key] = _cutoff * check_MAX(double_data_.at(key));
                    if ( key == _key ) {
                        write_log(std::format("\t- {} Cutoff: {}", _key, double_cutoff[key]));
                    }
                    break;
                case DataType::STRING:
                    break;
                case DataType::NONE:
                    throw std::runtime_error("Invalid DataType received.");
                }
            }

            /*
            * Lambda function extracts valid ranges.
            * Returns vector of pairs (start, finish)
            * indicating valid ranges.
            * "finish" = 1 past the end, e.g:
            * Data   : 1 2 3 4 5 6 7 8 9
            * Indexes: 0 1 2 3 4 5 6 7 8
            * The range (2, 3, 4, 5) would be denoted:
            * (1, 5) where 1 & 5 are indexes
            */
            const auto ExtractRanges =
                []<ArithmeticType T>(
                const std::vector<T>& _data,
                const T& _threshold,
                const uinteger& _n = 1,
                const uinteger& _max_range_sz = 0
            ) -> std::vector<std::pair<uinteger, uinteger>> {
                /*
                * - results: Stores resulting {start point, end point} pairs.
                * - continuous_range: Indicates currently in a continuous run
                *					  of values either >= or < threshold.
                * - r_start: Tracks start position of most recent range >= threshold.
                * - r_end: Tracks end position of most recent range >= threshold.
                * - count: Tracks transition across threshold. If count >= _n
                *		   transition is considered complete and not caused by
                *		   noise in otherwise continuous data.
                * - branch: Tracks which possible branch is true
                */
                std::vector<std::pair<uinteger, uinteger>> results;
                results.reserve(_data.size() / 10);
                bool continuous_range { false };
                uinteger r_start { 0 }, r_end { 0 }, count { 0 };
                for ( uinteger i = 0; i < _data.size(); ++i ) {
                    uinteger branch { 0 };
                    branch += _data[i] >= _threshold
                                   ? 2
                                   : 1;
                    branch += continuous_range
                                   ? 1
                                   : -1;

                    switch ( branch ) {
                    case 0:
                        /*
                        * Either:
                        * (value < _threshold & !continuous_range)
                        * Count tracks a change from continuous to non-continuous range.
                        */
                        count = 0;
                        break;
                    case 1:
                        /*
                        * If value >= threshold && we are not in a continuous range:
                        * - We've (potentially!) reached a new range.
                        * - Increment count, if count >= _n the range is at least _n
                        *   long and we set r_start to i - count + 1 (first value in
                        *   the range).
                        */
                        count++;
                        continuous_range = count >= _n;
                        if ( continuous_range ) {
                            r_start = i - count + 1;
                            count = 0;
                        }
                        break;
                    case 2:
                        /*
                        * If value < threshold and we're in a continuous range:
                        * - We've (potentially!) left a continuous range.
                        * - Opposite to _data[i] >= _threshold && !continuous_range.
                        */
                        count++;
                        continuous_range = !(count >= _n);
                        if ( !continuous_range ) {
                            r_end = i - count + 1;
                            const auto subranges = sub_range_split(
                                                                   _data, r_start, r_end, _max_range_sz);

                            results.insert(results.cend(), subranges.cbegin(),
                                           subranges.cend());

                            count = 0;
                        }
                        break;
                    case 3:
                        /*
                        * If value >= threshold and we are in a continuous range:
                        * - continue & reset count.
                        */
                        count = 0;
                        break;
                    default:
                        break;
                    }
                }
                if ( continuous_range && r_start > r_end ) {
                    /*
                    * Handle case where a valid range reaches to the end of the data
                    * without transitioning back below the threshold.
                    */
                    r_end = _data.size() - 1;
                    const auto subranges = sub_range_split(
                                                           _data, r_start, r_end, _max_range_sz);
                    results.insert(results.cend(), subranges.cbegin(),
                                   subranges.cend());
                }
                results.shrink_to_fit();
                return results;
            };

            write_log("Extracting ranges...");
            // Calculate filters
            switch ( dtype ) {
            case DataType::INTEGER:
                filters_ = ExtractRanges(int_data_.at(_key),
                                         int_cutoff.at(_key),
                                         _n, _max_range_sz);
                break;
            case DataType::DOUBLE:
                filters_ = ExtractRanges(double_data_.at(_key),
                                         double_cutoff.at(_key),
                                         _n, _max_range_sz);
                break;
            default:
                throw std::runtime_error("Invalid filter type received.");
            }
            const uinteger sz =
            std::accumulate( filters_.cbegin(), filters_.cend(),
                             static_cast<uinteger>(0),
                             [](const uinteger x, const std::pair<uinteger, uinteger>& p)
                             { return x + (p.second - p.first); } );
            write_log(std::format("Done. Filter size: {} ranges, {} elements.", filters_.size(), sz) );

            /*
             * No need to update n_rows_.
             * It should represent the size of the data
             * currently loaded, not the size of the data if
             * you applied the filter. n_rows_ is then updated
             * if the underlying data is changed, e.g if
             * reductions are applied.
             */

            return true;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <file_data::filter>");
            return false;
        }
    }

    // COMPLETE
    inline bool
    spreadsheet::clear_spreadsheet() noexcept {
        try {
            file_ = file_data();

            filters_.clear();

            int_data_.clear();
            double_data_.clear();
            string_data_.clear();

            i_errors_.clear();
            d_errors_.clear();
            s_errors_.clear();

            type_map_.clear();

            n_rows_ = 0;
            reduction_type_ = reduction_type { 100 };
            average_type_ = avg_type::stable_mean;
            n_group_ = 1;
            n_points_ = 0;

            initialized_ = false;

            return true;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <spreadsheet::clear_spreadsheet>");
            return false;
        }
    }

    inline [[nodiscard]] std::vector<integer>
    spreadsheet::get_i( const std::string& key ) const noexcept {
        try {
            if ( filters_.empty() ) {
                return int_data_.at(key);
            }
            const uinteger sz =
            std::accumulate(filters_.cbegin(), filters_.end(),
                            static_cast<uinteger>(0),
                            [](uinteger sum, const std::pair<uinteger, uinteger>& p)
                            { return sum + (p.second - p.first); });

            std::vector<integer> data;
            data.reserve(sz);

            for ( const auto& [first, last] : filters_ ) {
                data.insert(data.cend(),
                            int_data_.at(key).cbegin() + first,
                            int_data_.at(key).cbegin() + last);
            }

            return data;
        }
        catch ( const std::exception& err ) {
            write_err_log( err, "DLL: <spreadsheet::get_i>" );
            return {};
        }
    }

    inline [[nodiscard]] std::vector<double>
    spreadsheet::get_d( const std::string& key ) const noexcept {
        try {
            if ( filters_.empty() ) {
                return double_data_.at(key);
                
            }
            const uinteger sz =
            std::accumulate(filters_.cbegin(), filters_.end(),
                            static_cast<uinteger>(0),
                            [](uinteger sum, const std::pair<uinteger, uinteger>& p)
                            { return sum + (p.second - p.first); });

            std::vector<double> data;
            data.reserve(sz);

            for ( const auto& [first, last] : filters_ ) {
                data.insert(data.cend(),
                            double_data_.at(key).cbegin() + first,
                            double_data_.at(key).cbegin() + last);
            }

            return data;
        }
        catch ( const std::exception& err ) {
            write_err_log( err, "DLL: <spreadsheet::get_d>" );
            return {};
        }
    }

    inline [[nodiscard]] std::vector<std::string>
    spreadsheet::get_s( const std::string& key ) const noexcept {
        try {
            if ( filters_.empty() ) {
                return string_data_.at(key);
                
            }
            const uinteger sz =
            std::accumulate(filters_.cbegin(), filters_.end(),
                            static_cast<uinteger>(0),
                            [](uinteger sum, const std::pair<uinteger, uinteger>& p)
                            { return sum + (p.second - p.first); });

            std::vector<std::string> data;
            data.reserve(sz);

            for ( const auto& [first, last] : filters_ ) {
                data.insert(data.cend(),
                            string_data_.at(key).cbegin() + first,
                            string_data_.at(key).cbegin() + last);
            }

            return data;
        }
        catch ( const std::exception& err ) {
            write_err_log( err, "DLL: <spreadsheet::get_s>" );
            return {};
        }
    }

    inline [[nodiscard]] std::vector<double>
    spreadsheet::get_error( const std::string& key ) const noexcept {
        try {
            std::vector<double> errors;

            switch ( type_map_.at(key) ) {
            case DataType::INTEGER: {
                if ( filters_.empty() ) {
                    errors = i_errors_.at(key);
                }
                else {
                    const uinteger sz =
                    std::accumulate(filters_.cbegin(), filters_.cend(),
                                    static_cast<uinteger>(0),
                                    [](const uinteger size, const std::pair<uinteger, uinteger>& p)
                                    { return size + (p.second - p.first); });

                    errors.reserve(sz);

                    for ( const auto& [first, last] : filters_ ) {
                        errors.insert(errors.cend(),
                                      i_errors_.at(key).cbegin() + first,
                                      i_errors_.at(key).cbegin() + last);
                    }
                }
            } break;
            case DataType::DOUBLE: {
                if ( filters_.empty() ) {
                    errors = d_errors_.at(key);
                }
                else {
                    const uinteger sz =
                    std::accumulate(filters_.cbegin(), filters_.cend(),
                                    static_cast<uinteger>(0),
                                    [](const uinteger size, const std::pair<uinteger, uinteger>& p)
                                    { return size + (p.second - p.first); });

                    errors.reserve(sz);

                    for ( const auto& [first, last] : filters_ ) {
                        errors.insert(errors.cend(),
                                      d_errors_.at(key).cbegin() + first,
                                      d_errors_.at(key).cbegin() + last);
                    }
                }
            } break;
            case DataType::STRING: {
                if ( filters_.empty() ) {
                    errors = s_errors_.at(key);
                }
                else {
                    const uinteger sz =
                    std::accumulate(filters_.cbegin(), filters_.cend(),
                                    static_cast<uinteger>(0),
                                    [](const uinteger size, const std::pair<uinteger, uinteger>& p)
                                    { return size + (p.second - p.first); });

                    errors.reserve(sz);

                    for ( const auto& [first, last] : filters_ ) {
                        errors.insert(errors.cend(),
                                      s_errors_.at(key).cbegin() + first,
                                      s_errors_.at(key).cbegin() + last);
                    }
                }
            } break;
            case DataType::NONE: {
                errors = {};
            }
            }

            return errors;
        }
        catch ( const std::exception& err ) {
            write_err_log( err, "DLL: <spreadsheet::get_error>" );
            return {};
        }
    }

    inline [[nodiscard]] bool
    spreadsheet::contains( const std::string& key ) const noexcept {
        try {
            return type_map_.contains(key);
        }
        catch ( const std::exception& err ) {
            write_err_log( err, "DLL: <spreadsheet::contains>" );
            return false;
        }
    }

    inline [[nodiscard]] DataType
    spreadsheet::type( const std::string& key ) const noexcept {
        try {
            /*
             * If key exists:
             *   - Returns correct DataType
             * If key doesn't exist:
             *   - std::map::at(...) throws a std::out_of_range exception
             *     which is caught, logging an error &
             *     returning DataType::NONE.
             */
            return file_.get_type(key);
        }
        catch ( const std::exception& err ) {
            write_err_log( err, "DLL: <spreadsheet::type>" );
            return DataType::NONE;
        }
    }

}
