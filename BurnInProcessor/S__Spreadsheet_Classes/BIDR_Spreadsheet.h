#pragma once

#include "../F__Folder_Funcs/BIDR_FolderSearch.h"
#include "../S__Datastructures/BIDR_FileData.h"


namespace burn_in_data_report
{
    class spreadsheet
    {
    private:
        file_data file_;
        indices_t ranges_;
        indices_t filters_;
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

        template <typename T> indices_t
        apply_filter( std::vector<T>& data,
                      const indices_t& filter ) noexcept;

        bool
        reduce( const reduction_type& r_type,
                const avg_type& a_type,
                const uinteger& n_group,
                const uinteger& n_points ) noexcept;

        bool
        clear_spreadsheet() noexcept;

        [[nodiscard]] uinteger n_rows() const noexcept { return n_rows_; }

        [[nodiscard]] bool is_initialized() const noexcept { return initialized_; }

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

        [[nodiscard]] bool
        clear_changes() noexcept;

        [[nodiscard]] auto
        get_file_boundaries() const noexcept { return file_.get_file_boundaries(); }
        [[nodiscard]] auto&
        file_boundaries() noexcept { return file_.file_boundaries(); }
    };


    inline
    spreadsheet::spreadsheet() :
        n_rows_(0),
        reduction_type_(reduction_type::none),
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
        reduction_type_(reduction_type::none),
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
        catch ( const std::exception& err ) { throw std::runtime_error(err.what()); }
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
        reduction_type_(reduction_type::none),
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
        try {
            bool result { true };
            uinteger size { 0 };
            write_log("update_n_rows:");

            for ( const auto& [key, type] : type_map_ ) {
                switch ( type ) {
                case DataType::INTEGER: {
                    if ( size == 0 ) {
                        size = int_data_.at(key).size();
                    }
                    else {
                        if ( size != int_data_.at(key).size() ) {
                            throw
                                std::runtime_error("\tDLL: <spreadsheet::update_n_rows> Data length mismatch.");
                            result = false;
                        }
                    }
                }
                break;
                case DataType::DOUBLE: {
                    if ( size == 0 ) {
                        size = double_data_.at(key).size();
                    }
                    else {
                        if ( size != double_data_.at(key).size() ) {
                            throw
                                std::runtime_error("DLL: <spreadsheet::update_n_rows> Data length mismatch.");
                            result = false;
                        }
                    }
                }
                break;
                case DataType::STRING: {
                    if ( size == 0 ) {
                        size = string_data_.at(key).size();
                    }
                    else {
                        if ( size != string_data_.at(key).size() ) {
                            throw
                                std::runtime_error("DLL: <spreadsheet::update_n_rows> Data length mismatch.");
                            result = false;
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
                write_log(std::format("n_rows_: {}", n_rows_));
            }

            return result;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <spreadsheet::update_n_rows>");
            return false;
        }
    }

    // COMPLETE?
    inline bool
    spreadsheet::load_files(
        const std::vector<std::filesystem::directory_entry>& files,
        const std::string& f_key = "Channel A",
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

    /*
    * WARNING:
    * This is incredibly janky! There are probably smarter ways to build this whole
    * system. However, I'm making this on my own so it's what you get I guess.
    */
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
            if ( !type_map_.contains(_key) ) { return false; }

            std::vector<integer> i_reduced;
            std::vector<double> d_reduced;
            std::vector<std::string> s_reduced;
            const DataType type = type_map_.at(_key);
            auto ranges_copy{ ranges_ };

            // get current no. rows. Will be updated as reductions applied
            uinteger no_rows { 0 };
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

            write_log(std::format("Average type: {}", avg_string.at(_a_type)));
            write_log(std::format("no_rows: {}", no_rows));

            // No reduction if it isn't set & no_rows < MAX_ROWS
            // If no_rows > MAX_ROWS, MUST be reduced to fit in excel
            if ( _r_type == reduction_type::none && no_rows < MAX_ROWS ) {
                write_log(" - No reductions to apply.");
                return true;
            }

            // Average by each cycle, this will likely always be true
            // Still may be useful to have the option to not use it
            if ( (_r_type == reduction_type::DEFAULT
                  || _r_type == reduction_type::all)
                 && !ranges_.empty() ) {
                write_log(" - Applying default (averaging by cycle) reductions.");
                write_log(std::format("   ranges_copy.size() = {}", ranges_copy.size()));

                // Need to think of acceptable way to do strings
                // For now will use lambda which returns NaN values
                // As it doesn't make much sense to "take the average
                // of a string"
                const auto avg =
                    [&ranges_copy, &_a_type]<ArithmeticType T>
                    (const std::vector<T>& data,
                    const std::vector<double>& stdevs,
                    const func_map_t<T>& a_func_map)
                -> std::pair<std::vector<T>, std::vector<double>> {
                        const auto& [avg_func, std_func] = a_func_map.at(_a_type);
                        std::vector<T> avgs;
                        std::vector<double> new_stdevs;

                        avgs.reserve(ranges_copy.size());
                        new_stdevs.reserve(ranges_copy.size());

                        for ( const auto& [first, last] : ranges_copy ) {
                            avgs.emplace_back(static_cast<T>(avg_func(data, first, last, stdevs)));
                            new_stdevs.emplace_back(std_func(data, first, last, avgs.back(), stdevs, 0));
                        }

                        return { avgs, new_stdevs };
                    };

                switch ( type ) {
                case DataType::INTEGER: {
                    const auto& [avgs, std] =
                        avg(int_data_[_key], i_errors_[_key], avg_func_map<integer>);
                    int_data_[_key] = avgs;
                    i_errors_[_key] = std;
                } break;
                case DataType::DOUBLE: {
                    const auto& [avgs, std] =
                        avg(double_data_[_key], d_errors_[_key], avg_func_map<double>);
                    double_data_[_key] = avgs;
                    d_errors_[_key] = std;
                } break;
                case DataType::STRING: {
                    /*
                     * Averaging string data doesn't make much sense.
                     * Instead, concatenate data & give NaN as the standard deviation.
                     */
                    const auto& data = string_data_.at(_key);

                    // { { start_idx, end_idx }, ... } ==> { { start_idx, ..., end_idx }, ... }
                    constexpr auto range_to_span =
                        [](const range_t& p){ return std::views::iota(p.first, p.second); };
                    // idx -> data[idx]
                    const auto idx_to_str =
                        [&data](const uinteger& idx){ return data[idx]; };
                    // { { start_idx, ..., end_idx }, ... } ==> { { data[start_idx], ..., data[end_idx] }, ... }
                    const auto span_to_strs =
                        [&idx_to_str](const auto& x) { return x | std::views::transform(idx_to_str); };
                    // { { data[start_idx], ..., data[end_idx] }, ... } ==> { { data[start_idx] + ", " + ... + data[end_idx] }, ... }
                    constexpr auto strs_to_str =
                        [](const auto& x){ return concatenate(x.begin(), x.end(), std::string_view{", "}); };

                    const auto filt_rng_to_str =
                        std::views::transform(range_to_span)
                        | std::views::transform(span_to_strs)
                        | std::views::transform(strs_to_str);

                    std::vector<std::string> tmp;
                    tmp.reserve(ranges_copy.size());
                    s_errors_[_key].clear();
                    s_errors_.reserve(ranges_copy.size());

                    for ( const auto& s : ranges_copy | filt_rng_to_str ) {
                        tmp.emplace_back(s);
                        s_errors_[_key].emplace_back(std::numeric_limits<double>::signaling_NaN());
                    }

                    string_data_[_key] = std::move(tmp);
                } break;
                case DataType::NONE: {
                    throw std::runtime_error("Invalid type received.");
                }
                }

                // Clear ranges_ now cycles have been
                // avg'd into single data points
                ranges_copy.clear();
            }

            // Average by groups of n_group_ points
            if ( (_r_type == reduction_type::ngroup
                  || _r_type == reduction_type::all) &&
                  _n_group > 0 && _n_group < no_rows ) {
                write_log(" - Applying number of points per group reduction.");

                // Lambda function to handle averaging
                auto avg
                    = [&_a_type]<typename K>
                ( const std::vector<K>& data,
                  std::vector<K>& reduced_storage,
                  std::vector<double>& stdevs,
                  const uinteger& n_group,
                  const func_map_t<K>& a_func_map)
                -> bool {
                    try {
                        // Clear any existing data in results storage
                        if ( !reduced_storage.empty() ) {
                            reduced_storage.clear();
                        }

                        // Storage for copy of data about to be reduced
                        const auto& [avg_func, std_func] =
                            a_func_map.at(_a_type);

                        const uinteger n_points =
                            data.size() / n_group;
                        const uinteger overflow =
                            data.size() % n_group ? 1 : 0;

                        reduced_storage.clear();
                        reduced_storage.reserve(n_points + overflow);
                        std::vector<double> tmp_stdevs;
                        tmp_stdevs.reserve( n_points + overflow );

                        for ( uinteger i{ 0 }; i < n_points; ++i ) {
                            reduced_storage.emplace_back(
                                static_cast<K>(
                                    avg_func(
                                        data,
                                        i * n_group, (i + 1) * n_group,
                                        stdevs
                                    )
                                )
                            );
                            tmp_stdevs.emplace_back(
                                std_func(
                                    data,
                                    i * n_group, (i + 1) * n_group,
                                    static_cast<double>(reduced_storage.back()),
                                    stdevs, 0
                                )
                            );
                        }

                        if ( overflow ) {
                            reduced_storage.emplace_back(
                                static_cast<K>(
                                    avg_func(
                                        data,
                                        n_points * n_group, (n_points * n_group) + (data.size() % n_group),
                                        stdevs
                                    )
                                )
                            );
                            tmp_stdevs.emplace_back(
                                std_func(
                                    data,
                                    n_points * n_group, (n_points * n_group) + (data.size() % n_group),
                                    static_cast<double>(reduced_storage.back()),
                                    stdevs, 0
                                )
                            );
                        }

                        stdevs = std::move(tmp_stdevs);
                        
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
                    avg(
                        int_data_.at(_key),
                        i_reduced, i_errors_[_key],
                        _n_group, avg_func_map<integer>
                    );

                    int_data_[_key] = i_reduced;
                    break;
                }
                case DataType::DOUBLE: {
                    avg( double_data_.at( _key ),
                         d_reduced, d_errors_[_key],
                        _n_group, avg_func_map<double>);

                    double_data_[_key] = d_reduced;
                    break;
                }
                case DataType::STRING: {
                    auto& data { string_data_[_key] };

                    uinteger _no_rows { data.size() / _n_group}, offset{ data.size() % _n_group };

                    s_reduced.clear();
                    s_reduced.reserve(_no_rows + (offset > 0 ? 1 : 0));

                    for ( uinteger i{ 0 }; i < _no_rows; ++i ) {
                        const uinteger first{ i * _n_group }, last{ (i + 1) * _n_group };
                        s_reduced.emplace_back(concatenate(data.cbegin() + first, data.cbegin() + last, ","));
                    }
                    if ( offset > 0 ) {
                        s_reduced.emplace_back(concatenate(data.cbegin() + _no_rows * _n_group, data.cend(), ","));
                    }
                    
                    s_errors_[_key].clear();
                    s_errors_[_key].insert(
                        s_errors_[_key].end(),
                        _no_rows + offset,
                        std::numeric_limits<double>::signaling_NaN()
                    );

                    string_data_[_key] = s_reduced;
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
                 (_n_points > 0 &&
                  _n_points < no_rows ||
                  (_n_points > MAX_ROWS || no_rows > MAX_ROWS)) ) {
                write_log(" - Applying reduction by total n_points.");
                
                auto avg =
                    [&_a_type]<ArithmeticType R>
                    ( const std::vector<R>& data, std::vector<R>& reduced_storage,
                    std::vector<double>& stdevs, const uinteger& _n_points,
                    const func_map_t<R>& a_func_map) -> void {
                        // Storage for tmp data
                        const auto& [avg_func, std_func] = a_func_map.at(_a_type);

                        // Calculate num of points per grouping.
                        // If provided max. num. of points > MAX_ROWS (excel limit), default to MAX_ROWS
                        const auto n_points_arr = std::vector<uinteger>{ _n_points, MAX_ROWS, data.size() };
                        const uinteger n_points = *std::min_element( n_points_arr.cbegin(), n_points_arr.cend() );

                        reduced_storage.clear();
                        reduced_storage.reserve( n_points );

                        std::vector<double> tmp_stdevs;
                        tmp_stdevs.reserve( n_points );

                        const uinteger n_group { data.size() / n_points};
                        const uinteger overflow { data.size() % n_points};

                        for ( uinteger i{ 0 }; i < n_points - overflow; ++i ) {
                            reduced_storage.emplace_back(
                                static_cast<R>(
                                    avg_func(
                                        data,
                                        i * n_group, (i + 1) * n_group,
                                        stdevs
                                    )
                                )
                            );
                            tmp_stdevs.emplace_back(
                                std_func(
                                    data,
                                    i * n_group, (i + 1) * n_group,
                                    static_cast<double>(reduced_storage.back()),
                                    stdevs, 0
                                )
                            );
                        }

                        // Add overflow to last few points
                        uinteger count{ 0 }, start_pos{ (n_points - overflow) * n_group }, end_pos{ start_pos };
                        for ( uinteger i{ (n_points - overflow) * n_group }; i < data.size(); ++i ) {
                            if ( ++count == n_group + 1 ) {
                                reduced_storage.emplace_back(
                                    static_cast<R>(
                                        avg_func(
                                            data,
                                            start_pos, i + 1,
                                            stdevs
                                        )
                                    )
                                );
                                tmp_stdevs.emplace_back(
                                    std_func(
                                        data,
                                        start_pos, i + 1,
                                        static_cast<double>(reduced_storage.back()),
                                        stdevs, 0
                                    )
                                );
                                start_pos = i + 1;
                                count = 0;
                            }
                        }

                        stdevs = std::move( tmp_stdevs );
                    };

                // Calculate num of points per grouping.
                // If provided max. num. of points > MAX_ROWS (excel limit), default to MAX_ROWS

                switch ( type ) {
                case DataType::INTEGER: {
                    avg(int_data_.at(_key),
                        i_reduced, i_errors_[_key],
                        _n_points, avg_func_map<integer>);

                    int_data_[_key] = i_reduced;
                    break;
                }
                case DataType::DOUBLE: {
                    avg(double_data_.at( _key ),
                        d_reduced, d_errors_[_key],
                        _n_points, avg_func_map<double>);

                    double_data_[_key] = d_reduced;
                    break;
                }
                case DataType::STRING: {
                    auto& data = string_data_.at(_key);

                    const uinteger n_points =
                    MAX_ROWS < _n_points || _n_points == 0
                        ? MAX_ROWS
                        : _n_points;
                    const uinteger n_group { data.size() / n_points}, overflow{ data.size() % n_points };

                    s_reduced.clear();
                    s_reduced.reserve(n_points);

                    for ( uinteger i{ 0 }; i < n_points - overflow; ++i ) {
                        const uinteger start{i * n_group}, end{(i+1) * n_group};
                        s_reduced.emplace_back(
                            concatenate(
                                data.cbegin() + start,
                                data.cbegin() + end,
                                ","
                            )
                        );
                    }
                    const uinteger startpoint{ (n_points - overflow) * n_group };
                    for ( uinteger i{ 0 }; i < overflow; ++i ) {
                        const uinteger start{startpoint + i * n_group + i},
                            end{startpoint + (i + 1) * n_group + i + 1};
                        s_reduced.emplace_back(
                            concatenate(
                                data.cbegin() + start,
                                data.cbegin() + end,
                                ","
                            )
                        );
                    }

                    s_errors_[_key].insert(
                        s_errors_.at(_key).end(), n_points,
                        std::numeric_limits<double>::signaling_NaN()
                     );

                    string_data_[_key] = s_reduced;
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

            reduction_type_ = r_type;
            average_type_ = a_type;
            n_group_ = n_group;
            n_points_ = n_points;

            for ( const auto& key : type_map_ | std::views::keys ) {
                write_log(std::format("Reducing {}...", key));
                write_log(std::format("{} - ranges_.size(): {}", key, ranges_.size()));
                bool tmp{ apply_reduction(key, r_type, a_type, n_group, n_points) };
                write_log( tmp ? "Success." : "Fail.");
                result &= tmp;
            }

            if ( result ) { ranges_.clear(); }

            result &= update_n_rows();

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
            cols.reserve(type_map_.size());

            for ( const auto& title : type_map_ | std::views::keys ) {
                cols.emplace_back(title);
            }

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
            if ( file_.contains(_key) && type_map_.contains(_key) ) {
                // Column is already loaded, exit
                write_log(std::format("<spreadsheet::load_column> Column already loaded ({}).", _key));
                return true;
            }
            if ( !file_.contains(_key) ) {
                throw std::runtime_error("Key does not exists.");
            }

            write_log(std::format("<spreadsheet::load_column> Loading {}", _key));
            dtype = file_.get_col_types().at(_key);
            
            const bool ranges_empty { ranges_.empty() };

            // Add to type_map_
            type_map_[_key] = dtype;

            // Load unfiltered data, then:
            // Apply filters & reductions
            switch ( dtype ) {
            case DataType::INTEGER: {
                int_data_[_key] = file_.get_i(_key);
                i_errors_[_key] = {};
                ranges_ = apply_filter( int_data_.at(_key), filters_);
                apply_reduction(_key,
                                reduction_type_,
                                average_type_,
                                n_group_,
                                n_points_);
                if ( ranges_empty ) { ranges_.clear(); }
                if ( !update_n_rows() ) {
                    throw std::runtime_error("DLL: <spreadsheet::load_column> Data length mismatch.");
                }
                break;
            }
            case DataType::DOUBLE: {
                double_data_[_key] = file_.get_d(_key);
                d_errors_[_key] = {};
                ranges_ = apply_filter(double_data_.at(_key), filters_);
                apply_reduction(_key,
                                reduction_type_,
                                average_type_,
                                n_group_,
                                n_points_);
                if ( ranges_empty ) { ranges_.clear(); }
                if ( !update_n_rows() ) {
                    throw std::runtime_error("DLL: <spreadsheet::load_column> Data length mismatch.");
                }
                break;
            }
            case DataType::STRING: {
                string_data_[_key] = file_.get_s(_key);
                s_errors_[_key] = {};
                ranges_ = apply_filter(string_data_.at(_key), filters_);
                apply_reduction(_key,
                                reduction_type_,
                                average_type_,
                                n_group_,
                                n_points_);
                if ( ranges_empty ) { ranges_.clear(); }
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

    template <typename T> indices_t
    spreadsheet::apply_filter( std::vector<T>& data,
                               const indices_t& filter ) noexcept {
        try {
            if ( filter.empty() ) { return indices_t{}; }

            indices_t ranges;
            ranges.reserve(filter.size());

            uinteger pos{ 0 };
            for ( const auto& [first, last] : filter ) {
                if ( pos != first ) {
                    std::copy(
                        data.begin() + first,
                        data.begin() + last,
                        data.begin() + pos
                    );
                }
                ranges.emplace_back(pos, pos + (last - first));
                pos += last - first;
            }
            data.resize(pos);

            return ranges;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <spreadsheet::apply_filter>");
            return indices_t{};
        }
    }

    // COMPLETE?
    inline bool
    spreadsheet::filter( const std::string& _key,
                         const double& _cutoff = 0.8,
                         const uinteger& _n = 2,
                         const uinteger& _max_range_sz = 0 ) noexcept {
        try {
            write_log("Filtering data:");

            // Check valid cutoff fraction & that _key exists / has been loaded
            if ( !(0 <= _cutoff && _cutoff < 1 && type_map_.contains(_key)) ) {
                throw std::runtime_error("Invalid filter parameters received.");
            }
            if ( type_map_.at(_key) == DataType::STRING ||
                 type_map_.at(_key) == DataType::NONE ) {
                throw std::runtime_error("Selected column type is unsupported.");
            }

            if ( _cutoff == 0.0 && _max_range_sz == 1 ) {
                write_log(" - cutoff = 0.0, max_range_sz = 1 -> No filter required.");
                return true;
            }

            // get DataType of provided _key
            const DataType dtype = type_map_.at(_key);

            // Map key --> cutoff value for filter
            std::map<std::string, integer> int_cutoff;
            std::map<std::string, double> double_cutoff;

            constexpr auto calculate_cutoff =
                []<ArithmeticType T>
                (const T& min, const T& max,
                const double& fraction) -> T
                {
                    const T result{ static_cast<T>(min + fraction * (max - min)) };
                    write_log(std::format("min: {}, max: {}, fraction: {}, cutoff: {}", min, max, fraction, result));
                    return result;
                };

            // Calculate cutoff values as fraction of max datapoint
            for ( const auto& [key, type] : type_map_ ) {
                switch ( type ) {
                case DataType::INTEGER: {
                    int_cutoff[key] =
                        _cutoff == 0.0 ?
                        std::numeric_limits<integer>::lowest() :
                        calculate_cutoff(check_min(int_data_.at(_key)), check_max(int_data_.at(_key)), _cutoff);
                    if ( key == _key ) { write_log(std::format(" - {} Cutoff: {}", _key, int_cutoff[key])); }
                }
                break;
                case DataType::DOUBLE: {
                    double_cutoff[key] =
                        _cutoff == 0.0 ?
                        std::numeric_limits<double>::lowest() :
                        calculate_cutoff(check_min(double_data_.at(_key)), check_max(double_data_.at(_key)), _cutoff);
                    if ( key == _key ) { write_log(std::format(" - {} Cutoff: {}", _key, double_cutoff[key])); }
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
            ) -> indices_t {
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
                indices_t results;
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
                    // If count >= _n, no longer in continuous range
                        continuous_range = !(count >= _n);
                        if ( !continuous_range ) { // If not in continuous range
                            r_end = i - count + 1;
                            const auto subranges = sub_range_split(
                                                                   _data, r_start, r_end, _max_range_sz);
                            results.insert(results.cend(), subranges.cbegin(), subranges.cend());
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

            write_log(std::format(" - Extracting ranges: {}", _key));

            indices_t filters{};
            switch ( dtype ) {
            case DataType::INTEGER:
                filters = ExtractRanges(int_data_.at(_key),
                                         int_cutoff.at(_key),
                                         _n, _max_range_sz);
                break;
            case DataType::DOUBLE:
                filters = ExtractRanges(double_data_.at(_key),
                                         double_cutoff.at(_key),
                                         _n, _max_range_sz);
                break;
            default:
                throw std::runtime_error("Invalid filter type received.");
            }

            filters_ = filters;

            const uinteger sz =
                std::accumulate(filters.cbegin(), filters.cend(),
                                static_cast<uinteger>( 0 ),
                                []( const uinteger x, const range_t& p )
                                { return x + (p.second - p.first); }
                );
            write_log(std::format("Done. Filter size: {} ranges, {} elements.", filters.size(), sz));

            // Make in-place changes to loaded data
            for ( const auto& [title, type] : type_map_ ) {
                switch ( type ) {
                case DataType::INTEGER: {
                    ranges_ = apply_filter<integer>(int_data_.at(title), filters);
                } break;
                case DataType::DOUBLE: {
                    ranges_ = apply_filter<double>(double_data_.at(title), filters);
                } break;
                case DataType::STRING: {
                    ranges_ = apply_filter<std::string>(string_data_.at(title), filters);
                } break;
                case DataType::NONE: {
                    throw std::runtime_error("DataType::NONE encountered.");
                }
                }
            }

            // Update n_rows_
            if ( !update_n_rows() ) {
                throw std::runtime_error("Failed to filter data.");
            }

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
        try { return int_data_.at(key); }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <spreadsheet::get_i>");
            return {};
        }
    }

    inline [[nodiscard]] std::vector<double>
    spreadsheet::get_d( const std::string& key ) const noexcept {
        try {  return double_data_.at(key); }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <spreadsheet::get_d>");
            return {};
        }
    }

    inline [[nodiscard]] std::vector<std::string>
    spreadsheet::get_s( const std::string& key ) const noexcept {
        try { return string_data_.at(key); }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <spreadsheet::get_s>");
            return {};
        }
    }

    inline [[nodiscard]] std::vector<double>
    spreadsheet::get_error( const std::string& key ) const noexcept {
        try {
            std::vector<double> errors;

            switch ( type_map_.at(key) ) {
            case DataType::INTEGER: {
                errors = i_errors_.at(key);
            }
                break;
            case DataType::DOUBLE: {
                errors = d_errors_.at(key);
            }
                break;
            case DataType::STRING: {
                errors = s_errors_.at(key);
            }
            break;
            case DataType::NONE: { errors = {}; }
            }

            return errors;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <spreadsheet::get_error>");
            return {};
        }
    }

    inline [[nodiscard]] bool
    spreadsheet::contains( const std::string& key ) const noexcept {
        try { return type_map_.contains(key); }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <spreadsheet::contains>");
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
            write_err_log(err, "DLL: <spreadsheet::type>");
            return DataType::NONE;
        }
    }

    inline [[nodiscard]] bool
    spreadsheet::clear_changes() noexcept {
        try {
            write_log("Clearing changes:");

            int_data_.clear();
            i_errors_.clear();
            double_data_.clear();
            d_errors_.clear();
            string_data_.clear();
            s_errors_.clear();

            ranges_.clear();
            filters_.clear();

            reduction_type_ = reduction_type::none;
            average_type_ = avg_type::stable_mean;
            n_group_ = 1;
            n_points_ = 0;
            n_rows_ = 0;

            for ( const auto& [key, type] : type_map_ ) {
                switch ( type ) {
                case DataType::INTEGER:
                    int_data_[key] = file_.get_i(key);
                    break;
                case DataType::DOUBLE:
                    double_data_[key] = file_.get_d(key);
                    break;
                case DataType::STRING:
                    string_data_[key] = file_.get_s(key);
                    break;
                case DataType::NONE:
                    break;
                }
            }

            if ( !update_n_rows() ) {
                throw std::runtime_error("DLL: <spreadsheet::load_column> Data length mismatch.");
            }

            return true;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <spreadsheet::clear_changes>");
            return false;
        }
    }

}
