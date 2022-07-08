#pragma once

#include "../F__Folder_Funcs/BIDR_FolderSearch.h"
#include "../S__Datastructures/BIDR_FileData.h"


namespace burn_in_data_report
{
    class spreadsheet
    {
    private:
        file_data file_;
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

        bool
        reduce( const reduction_type& r_type,
                const avg_type& a_type,
                const uinteger& n_group,
                const uinteger& n_points ) noexcept;

        bool
        clear_spreadsheet() noexcept;

        [[nodiscard]] uinteger n_rows() const noexcept {
            if ( filters_.empty() ) { return n_rows_; }
            uinteger size =
                std::accumulate(filters_.cbegin(), filters_.cend(),
                                static_cast<uinteger>( 0 ),
                                []( uinteger x, const range_t& p ) {
                                    return x + (p.second - p.first);
                                });
            return size;
        }

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
        try {
            bool result { true };
            uinteger size { 0 };
            write_log("update_n_rows:");

            constexpr auto get_size =
                []<typename T>(const std::vector<T>& v, const indices_t& f) {
                    if ( f.empty() ) {
                        return v.size();
                    }
                    else {
                        return
                            std::accumulate(
                                f.cbegin(), f.cend(),
                                static_cast<uinteger>(0),
                                [](const uinteger sum, const range_t& r)
                                { return sum + (r.second - r.first); }
                            );
                    }
                };

            for ( const auto& [key, type] : type_map_ ) {
                write_log(std::format("({}, {}):", key, type_string.at(type)));
                switch ( type ) {
                case DataType::INTEGER: {
                    if ( size == 0 ) {
                        size = get_size(int_data_.at(key), filters_);
                        write_log(std::format("\tsetting size = {}", size));
                    }
                    else {
                        if ( size != get_size(int_data_.at(key), filters_) ) {
                            write_log(std::format("\tsize = {}, data size = {}", size, get_size(int_data_.at(key), filters_)));
                            write_err_log(std::runtime_error("\tDLL: <spreadsheet::update_n_rows> Data length mismatch."));
                            result = false;
                        }
                        else { write_log(std::format("\t{}", size)); }
                    }
                }
                break;
                case DataType::DOUBLE: {
                    if ( size == 0 ) {
                        size = get_size(double_data_.at(key), filters_);
                        write_log(std::format("\tsetting size = {}, data size = {}", size, get_size(double_data_.at(key), filters_)));
                    }
                    else {
                        if ( size != get_size(double_data_.at(key), filters_) ) {
                            write_log(std::format("\tsize = {}, data size = {}", size, get_size(double_data_.at(key), filters_)));
                            write_err_log(std::runtime_error("DLL: <spreadsheet::update_n_rows> Data length mismatch."));
                            result = false;
                        }
                        else { write_log(std::format("\t{}", size)); }
                    }
                }
                break;
                case DataType::STRING: {
                    if ( size == 0 ) {
                        size = get_size(string_data_.at(key), filters_);
                        write_log(std::format("\tsetting size = {}", size));
                    }
                    else {
                        if ( size != get_size(string_data_.at(key), filters_) ) {
                            write_log(std::format("\tsize = {}, data size = {}", size, get_size(string_data_.at(key), filters_)));
                            write_err_log(std::runtime_error("DLL: <spreadsheet::update_n_rows> Data length mismatch."));
                            result = false;
                        }
                        else { write_log(std::format("\t{}", size)); }
                    }
                }
                break;
                case DataType::NONE:
                    result = false;
                    break;
                }
            }

            if ( result ) { n_rows_ = size; }

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
    * system. However, without a developed process for code review this is what we have.
    * The system used makes it impossible to apply weighted mean or weighted standard deviation
    * if there is pre-existing error data for the npoints & ngroup averaging.
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

            write_log(std::format("Applying reductions to {}", _key));

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
            uinteger no_rows { 0 };
            if ( filters_.empty() ) {
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
            }
            else {
                no_rows =
                    std::accumulate(filters_.cbegin(), filters_.cend(),
                                    static_cast<uinteger>( 0 ),
                                    []( const uinteger sum, const range_t& p ) {
                                        return sum + (p.second - p.first);
                                    });
            }

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
                 && !filters_.empty() ) {
                write_log(" - Applying default (averaging by cycle) reductions.");
                write_log(std::format("   Filter size: {}", filters_.size()));

                // Need to think of acceptable way to do strings
                // For now will use lambda which returns NaN values
                // As it doesn't make much sense to "take the average
                // of a string"
                const auto avg =
                    [&, filters_copy, _a_type, _n_group, _n_points]<ArithmeticType T>
                    (const std::vector<T>& data, const std::vector<double>& stdevs, const func_map<T>& a_func_map)
                    -> std::pair<std::vector<T>, std::vector<double>> {
                        const auto& [func, s_func] = a_func_map.at(_a_type);
                        std::vector<T> avgs;
                        std::vector<double> new_stdevs;
                        avgs.reserve(filters_copy.size());
                        new_stdevs.reserve(filters_copy.size());
                        for ( const auto& [first, last] : filters_copy ) {
                            avgs.emplace_back(
                                static_cast<T>(func(data, first, last, stdevs))
                            );
                            new_stdevs.emplace_back(
                                s_func(data, first, last, avgs.back(), stdevs, 0)
                            );
                        }
                        return { avgs, new_stdevs };
                    };

                switch ( type ) {
                case DataType::INTEGER: {
                    const auto& [avgs, std] =
                        avg(int_data_.at(_key), i_errors_.at(_key), avg_func_map<integer>);
                    int_data_.at(_key) = avgs;
                    i_errors_.at(_key) = std;
                } break;
                case DataType::DOUBLE: {
                    const auto& [avgs, std] =
                        avg(double_data_.at(_key), d_errors_.at(_key), avg_func_map<double>);
                    double_data_.at(_key) = avgs;
                    d_errors_.at(_key) = std;
                } break;
                case DataType::STRING: {
                    /*
                     * Averaging string data doesn't make much sense.
                     * Instead, concatenate data & give NaN as the standard deviation.
                     */
                    const auto& data = string_data_.at(_key);

                    // { { start_idx, end_idx }, ... } ==> { { start_idx, ..., end_idx }, ... }
                    const auto range_to_span =
                        [](const range_t& p){ return std::views::iota(p.first, p.second); };
                    // idx -> data[idx]
                    const auto idx_to_str =
                        [&data](const uinteger& idx){ return data[idx]; };
                    // { { start_idx, ..., end_idx }, ... } ==> { { data[start_idx], ..., data[end_idx] }, ... }
                    const auto span_to_strs =
                        [&idx_to_str](const auto& x) { return x | std::views::transform(idx_to_str); };
                    // { { data[start_idx], ..., data[end_idx] }, ... } ==> { { data[start_idx] + ", " + ... + data[end_idx] }, ... }
                    const auto strs_to_str =
                        [](const auto& x){ return concatenate(x.begin(), x.end(), std::string_view{", "}); };

                    auto filt_rng_to_str =
                        std::views::transform(range_to_span)
                        | std::views::transform(span_to_strs)
                        | std::views::transform(strs_to_str);

                    std::vector<std::string> tmp;
                    tmp.reserve(filters_copy.size());
                    s_errors_.at(_key).clear();
                    s_errors_.reserve(filters_copy.size());

                    for ( const auto& s : filters_copy | filt_rng_to_str ) {
                        tmp.emplace_back(s);
                        s_errors_.at(_key).emplace_back(std::numeric_limits<double>::signaling_NaN());
                    }

                    string_data_.at(_key) = std::move(tmp);
                } break;
                case DataType::NONE: {
                    throw std::runtime_error("Invalid type received.");
                } break;
                }

                // Clear filter now cycles have been
                // avg'd into single data points
                filters_copy.clear();
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
                  const indices_t& filters,
                  const uinteger& n_group,
                  const func_map<K>& a_func_map)
                -> bool {
                    try {
                        // Clear any existing data in results storage
                        if ( !reduced_storage.empty() ) {
                            reduced_storage.clear();
                        }

                        // Storage for copy of data about to be reduced
                        std::vector<K> tmp;
                        const auto& [avg_func, std_func] =
                            a_func_map.at(_a_type);

                        auto filters_copy = filters;

                        /*
                        * If filter exists:
                        * - Calc. num. of datapoints in filter
                        * - Add data & stdevs to temporary arrays
                        */

                        // Initial size of data
                        uinteger sz{ 0 };
                        if ( filters_copy.empty() ) {
                            sz = data.size();
                        }
                        else {
                            sz =
                                std::accumulate(
                                    filters_copy.cbegin(), filters_copy.cend(),
                                    static_cast<uinteger>( 0 ),
                                    []( const uinteger& lhs,const range_t& rhs )
                                    { return lhs + (rhs.second - rhs.first); }
                                );
                        }
                        tmp.reserve(sz);

                        // Copy data into temporaries
                        if ( filters_copy.empty() ) {
                            for ( const auto& [first, last] : filters ) {
                                tmp.insert(tmp.end(), data.cbegin() + first,
                                           data.cbegin() + last);
                            }
                        }
                        else {
                            tmp = data;
                        }

                        /*
                        * Calculate no. rows at end of process.
                        * Overflow handles remaining points if no_rows % n_group_ > 0
                        * by adding an extra point to the last x rows where x is the remainder
                        */
                        const uinteger total_rows { tmp.size() / n_group };
                        const uinteger overflow{ tmp.size() % n_group };
                        reduced_storage.reserve(total_rows + (overflow ? 1 : 0));

                        // Perform averaging:

                        const auto valid_data_pipeline =
                            std::views::transform(
                                [](const auto& pair)
                                { return std::views::iota(pair.first, pair.second); })
                            | std::views::join
                            | std::views::transform([&](const auto& idx){ return data[idx]; });

                        if ( filters_copy.empty() ) {
                            filters_copy = { {0, data.size() } };
                        }

                        auto const buf = new K[n_group];
                        uinteger buf_sz{ 0 }, count{ 0 };
                        for ( const auto& v_data : filters_copy | valid_data_pipeline ) {
                            if ( buf_sz < n_group - 1 ) {
                                buf[buf_sz++] = v_data;
                            }
                            else {
                                buf[buf_sz++] = v_data;
                                tmp.emplace_back( static_cast<K>(avg_func(std::vector<K>(buf, buf + buf_sz), 0, buf_sz, {})) );
                                buf_sz = 0;
                            }
                        }

                        if ( buf_sz > 0 ) {
                            tmp.emplace_back(static_cast<K>(avg_func(std::vector<K>(buf, buf + buf_sz), 0, buf_sz, {})));
                            buf_sz = 0;
                        }

                        delete[] buf;

                        return true;
                    }
                    catch ( const std::exception& err ) {
                        write_err_log(err, "DLL: <spreadsheet::apply_reduction::avg>");
                        return false;
                    }
                };

                uinteger sz{ 0 };
                if ( filters_copy.empty() ) {
                    switch ( type ) {
                    case DataType::INTEGER:
                        sz = int_data_.at(_key).size();
                        break;
                    case DataType::DOUBLE:
                        sz = double_data_.at(_key).size();
                        break;
                    case DataType::STRING:
                        sz = string_data_.at(_key).size();
                        break;
                    case DataType::NONE:
                        break;
                    }
                }
                else {
                    sz =
                    std::accumulate(
                        filters_copy.cbegin(), filters_copy.cend(),
                        static_cast<uinteger>( 0 ),
                        []( const uinteger& lhs, const range_t & rhs )
                        { return lhs + (rhs.second - rhs.  first); }
                    );
                }

                uinteger _no_rows { sz / _n_group }, offset { sz % _n_group };
                no_rows += offset == 0
                                ? 0
                                : 1;

                // Perform averaging for each column currently loaded
                switch ( type ) {
                case DataType::INTEGER: {
                    avg(int_data_.at(_key),
                        i_reduced,
                        filters_copy,
                        _n_group,
                        avg_func_map<integer>);

                    std_deviations.insert(std_deviations.end(),
                                          _no_rows + offset,
                                          std::numeric_limits<double>::signaling_NaN());

                    int_data_[_key] = i_reduced;
                    i_errors_[_key] = std::move(std_deviations);
                    break;
                }
                case DataType::DOUBLE: {
                    avg(double_data_.at(_key),
                        d_reduced,
                        filters_copy,
                        _n_group,
                        avg_func_map<double>);

                    
                    std_deviations.insert(std_deviations.end(),
                                          _no_rows + offset,
                                          std::numeric_limits<double>::signaling_NaN());

                    double_data_[_key] = d_reduced;
                    d_errors_[_key] = std::move(std_deviations);
                    break;
                }
                case DataType::STRING: {
                    auto& data { string_data_.at(_key) };

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
                    std_deviations.insert(std_deviations.end(),
                                          _no_rows + offset,
                                          std::numeric_limits<double>::signaling_NaN());

                    string_data_[_key] = s_reduced;
                    s_errors_[_key] = std_deviations;
                    break;
                }
                case DataType::NONE: { break; }
                }

                std_deviations.clear();

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
                    const indices_t& filters, const uinteger& _n_points,
                    const func_map<R>& a_func_map) -> void {
                        // Storage for tmp data
                        std::vector<R> tmp;
                        const auto& [avg_func, std_func] = a_func_map.at(_a_type);

                        auto filters_copy = filters;

                        uinteger sz { 0 };
                        if ( filters_copy.empty() ) { sz = data.size(); }
                        else {
                            sz =
                                std::accumulate(
                                    filters_copy.cbegin(), filters_copy.cend(),
                                    static_cast<uinteger>( 0 ),
                                    []( uinteger lhs, const range_t & pair )
                                    { return lhs + (pair.second - pair.first); }
                                );
                        }

                        // Calculate num of points per grouping.
                        // If provided max. num. of points > MAX_ROWS (excel limit), default to MAX_ROWS
                        const uinteger n_points =
                            MAX_ROWS < _n_points || _n_points == 0
                                ? MAX_ROWS
                                : _n_points;

                        tmp.reserve(n_points);

                        const uinteger n_group { sz / n_points };
                        const uinteger overflow { sz % n_points };

                        if ( filters_copy.empty() ) {
                            filters_copy = { { 0, data.size() }};
                        }

                        const auto valid_data_pipeline =
                            std::views::transform([](const auto& pair){ return std::views::iota(pair.first, pair.second); }) |
                            std::views::join |
                            std::views::transform([&](const auto& idx){ return data[idx]; });

                        uinteger buf_sz{ 0 }, count{ 0 };
                        auto const buf = new R[n_group + 1];

                        // Perform averaging in points
                        for ( const auto& v_data : filters_copy | valid_data_pipeline ) {
                            // Points up to n_points - overflow are as expected.
                            if ( count < n_points - overflow ) {
                                if ( buf_sz < n_group - 1 ) {
                                    buf[buf_sz++] = v_data;
                                }
                                else {
                                    buf[buf_sz++] = v_data;
                                    const auto avg_group = std::vector<R>{buf, buf + buf_sz};
                                    tmp.emplace_back(static_cast<R>(avg_func(avg_group, buf, buf_sz, {})));
                                    buf_sz = 0;
                                }
                            }
                            // Add 1 extra point per group to prevent overflow
                            // and get exactly n_points at the end.
                            else {
                                if ( buf_sz < n_group ) {
                                    buf[buf_sz++] = v_data;
                                }
                                else {
                                    buf[buf_sz++] = v_data;
                                    const auto avg_group = std::vector<R>{buf, buf + buf_sz + 1};
                                    tmp.emplace_back( static_cast<R>(avg_func(avg_group, buf, buf_sz + 1, {})) );
                                    buf_sz = 0;
                                }
                            }
                        }

                        delete[] buf;

                        reduced_storage = std::move(tmp);
                    };

                uinteger sz { 0 };
                if ( filters_copy.empty() ) {
                    switch ( type ) {
                    case DataType::INTEGER:
                        sz = int_data_.at(_key).size();
                        break;
                    case DataType::DOUBLE:
                        sz = double_data_.at(_key).size();
                        break;
                    case DataType::STRING:
                        sz = string_data_.at(_key).size();
                        break;
                    case DataType::NONE:
                        break;
                    }
                }
                else {
                    sz =
                        std::accumulate(
                            filters_copy.cbegin(), filters_copy.cend(),
                            static_cast<uinteger>( 0 ),
                            []( uinteger lhs, const range_t & pair )
                            { return lhs + (pair.second - pair.first); }
                        );
                }

                // Calculate num of points per grouping.
                // If provided max. num. of points > MAX_ROWS (excel limit), default to MAX_ROWS
                const uinteger n_points =
                    MAX_ROWS < _n_points || _n_points == 0
                        ? MAX_ROWS
                        : _n_points;
                const uinteger n_group { sz / n_points }, overflow { sz % n_points };

                switch ( type ) {
                case DataType::INTEGER: {
                    avg(int_data_.at(_key),
                        i_reduced,
                        filters_copy,
                        n_points,
                        avg_func_map<integer>);

                    std_deviations.insert(std_deviations.end(),
                                          n_points,
                                          std::numeric_limits<double>::signaling_NaN());

                    int_data_[_key] = i_reduced;
                    i_errors_[_key] = std_deviations;
                    break;
                }
                case DataType::DOUBLE: {
                    avg(double_data_.at(_key),
                        d_reduced,
                        filters_copy,
                        n_points,
                        avg_func_map<double>);

                    std_deviations.insert(std_deviations.end(),
                                          n_points,
                                          std::numeric_limits<double>::signaling_NaN());

                    double_data_[_key] = d_reduced;
                    d_errors_[_key] = std_deviations;
                    break;
                }
                case DataType::STRING: {
                    auto& data = string_data_.at(_key);
                    uinteger sz;
                    if ( filters_.empty() ) {
                        sz = data.size();
                        
                    }
                    else {
                        sz =
                            std::accumulate(
                                filters_copy.cbegin(), filters_copy.cend(),
                                static_cast<uinteger>( 0 ),
                                []( uinteger lhs, const range_t & rhs )
                                { return lhs + (rhs.second - rhs.  first); }
                            );
                    }

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
                        std_deviations.end(), n_points,
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

            filters_ = filters_copy;

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

            if ( result ) { filters_.clear(); }

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
                    if ( type_map_.contains(key) ) {
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
                write_log(std::format(" - Initial size: {}", int_data_[_key].size()));
                i_errors_[_key] = {};
                apply_reduction(_key,
                                reduction_type_,
                                average_type_,
                                n_group_,
                                n_points_);
                write_log(std::format(" - Final size: {}", int_data_[_key].size()));
                if ( !update_n_rows() ) {
                    throw std::runtime_error("DLL: <spreadsheet::load_column> Data length mismatch.");
                }
                break;
            }
            case DataType::DOUBLE: {
                double_data_[_key] = file_.get_d(_key);
                write_log(std::format(" - Initial size: {}", double_data_[_key].size()));
                d_errors_[_key] = {};
                apply_reduction(_key,
                                reduction_type_,
                                average_type_,
                                n_group_,
                                n_points_);
                write_log(std::format(" - Final size: {}", double_data_[_key].size()));
                if ( !update_n_rows() ) {
                    throw std::runtime_error("DLL: <spreadsheet::load_column> Data length mismatch.");
                }
                break;
            }
            case DataType::STRING: {
                string_data_[_key] = file_.get_s(_key);
                write_log(std::format(" - Initial size: {}", string_data_[_key].size()));
                s_errors_[_key] = {};
                apply_reduction(_key,
                                reduction_type_,
                                average_type_,
                                n_group_,
                                n_points_);
                write_log(std::format(" - Final size: {}", string_data_[_key].size()));
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

            // Clear existing filters
            filters_.clear();

            // Map key --> cutoff value for filter
            std::map<std::string, integer> int_cutoff;
            std::map<std::string, double> double_cutoff;

            if ( _cutoff == 0.0 ) {
                for ( const auto& [key, type] : type_map_ ) {
                    switch ( type ) {
                    case DataType::INTEGER:
                        int_cutoff[key] = std::numeric_limits<integer>::min();
                        break;
                    case DataType::DOUBLE:
                        double_cutoff[key] = std::numeric_limits<double>::min();
                        break;
                    case DataType::STRING:
                        break;
                    case DataType::NONE:
                        throw std::runtime_error("Invalid DataType received.");
                    }
                }
            }
            else {
                // Calculate cutoff values as fraction of max datapoint
                for ( const auto& [key, type] : type_map_ ) {
                    write_log(std::format(" - {}: {}", key, type_string.at(type)));

                    // Calculate cutoff value for each numeric key in loaded data
                    switch ( type ) {
                    case DataType::INTEGER: {
                        write_log(std::format(" - size of data({}): {}", key, int_data_.at(key).size()));
                        int_cutoff[key] = static_cast<integer>(
                            _cutoff * static_cast<double>( check_max(int_data_.at(key)) )
                        );
                        if ( key == _key ) { write_log(std::format(" - {} Cutoff: {}", _key, int_cutoff[key])); }
                    }
                    break;
                    case DataType::DOUBLE: {
                        write_log(std::format(" - size of data({}): {}", key, double_data_.at(key).size()));
                        double_cutoff[key] = _cutoff * check_max(double_data_.at(key));
                        if ( key == _key ) { write_log(std::format(" - {} Cutoff: {}", _key, double_cutoff[key])); }
                    }
                    break;
                    case DataType::STRING:
                        break;
                    case DataType::NONE:
                        throw std::runtime_error("Invalid DataType received.");
                    }
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
                std::accumulate(filters_.cbegin(), filters_.cend(),
                                static_cast<uinteger>( 0 ),
                                []( const uinteger x, const range_t& p ) {
                                    return x + (p.second - p.first);
                                });
            write_log(std::format("Done. Filter size: {} ranges, {} elements.", filters_.size(), sz));

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
            if ( filters_.empty() ) { return int_data_.at(key); }
            const uinteger sz =
                std::accumulate(filters_.cbegin(), filters_.end(),
                                static_cast<uinteger>( 0 ),
                                []( uinteger sum, const range_t& p ) {
                                    return sum + (p.second - p.first);
                                });

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
            write_err_log(err, "DLL: <spreadsheet::get_i>");
            return {};
        }
    }

    inline [[nodiscard]] std::vector<double>
    spreadsheet::get_d( const std::string& key ) const noexcept {
        try {
            if ( filters_.empty() ) { return double_data_.at(key); }
            const uinteger sz =
                std::accumulate(filters_.cbegin(), filters_.end(),
                                static_cast<uinteger>( 0 ),
                                []( uinteger sum, const range_t& p ) {
                                    return sum + (p.second - p.first);
                                });

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
            write_err_log(err, "DLL: <spreadsheet::get_d>");
            return {};
        }
    }

    inline [[nodiscard]] std::vector<std::string>
    spreadsheet::get_s( const std::string& key ) const noexcept {
        try {
            if ( filters_.empty() ) { return string_data_.at(key); }
            const uinteger sz =
                std::accumulate(filters_.cbegin(), filters_.end(),
                                static_cast<uinteger>( 0 ),
                                []( uinteger sum, const range_t& p ) {
                                    return sum + (p.second - p.first);
                                });

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
                if ( filters_.empty() ) { errors = i_errors_.at(key); }
                else {
                    const uinteger sz =
                        std::accumulate(filters_.cbegin(), filters_.cend(),
                                        static_cast<uinteger>( 0 ),
                                        []( const uinteger size, const range_t& p ) {
                                            return size + (p.second - p.first);
                                        });

                    errors.reserve(sz);

                    for ( const auto& [first, last] : filters_ ) {
                        errors.insert(errors.cend(),
                                      i_errors_.at(key).cbegin() + first,
                                      i_errors_.at(key).cbegin() + last);
                    }
                }
            }
            break;
            case DataType::DOUBLE: {
                if ( filters_.empty() ) { errors = d_errors_.at(key); }
                else {
                    const uinteger sz =
                        std::accumulate(filters_.cbegin(), filters_.cend(),
                                        static_cast<uinteger>( 0 ),
                                        []( const uinteger size, const range_t& p ) {
                                            return size + (p.second - p.first);
                                        });

                    errors.reserve(sz);

                    for ( const auto& [first, last] : filters_ ) {
                        errors.insert(errors.cend(),
                                      d_errors_.at(key).cbegin() + first,
                                      d_errors_.at(key).cbegin() + last);
                    }
                }
            }
            break;
            case DataType::STRING: {
                if ( filters_.empty() ) { errors = s_errors_.at(key); }
                else {
                    const uinteger sz =
                        std::accumulate(filters_.cbegin(), filters_.cend(),
                                        static_cast<uinteger>( 0 ),
                                        []( const uinteger size, const range_t& p ) {
                                            return size + (p.second - p.first);
                                        });

                    errors.reserve(sz);

                    for ( const auto& [first, last] : filters_ ) {
                        errors.insert(errors.cend(),
                                      s_errors_.at(key).cbegin() + first,
                                      s_errors_.at(key).cbegin() + last);
                    }
                }
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

            filters_.clear();
            int_data_.clear();
            i_errors_.clear();
            double_data_.clear();
            d_errors_.clear();
            string_data_.clear();
            s_errors_.clear();

            reduction_type_ = reduction_type::none;
            average_type_ = avg_type::stable_mean;
            n_group_ = 1;
            n_points_ = 0;
            n_rows_ = 0;

            for ( const auto& [key, type] : type_map_ ) {
                write_log(std::format(" - {}: {}", key, type_string.at(type)));
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
