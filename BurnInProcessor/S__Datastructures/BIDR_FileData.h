#pragma once

#include <algorithm>
#include <any>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <thread>

#include "../BIDR_Defines.h"
#include "../F__File_Parse/BIDR_FileParse.h"
#include "BIDR_StorageTypes.h"

#include "BIDR_Timer.h"

#undef max
#undef min

//using integer = int64_t;
//using uinteger = uint64_t;

namespace burn_in_data_report
{
    enum class encoding_type
    {
        UNKNOWN = 0, // No recognised BOM, assume UTF8
        UTF8 = 1,
        UTF8_BOM = 2,
        UTF16_BE = 3,
        UTF16_LE = 4
    };


    const std::map<std::vector<unsigned>, encoding_type> file_encoding {
            { { 0xffffffef, 0xffffffbb, 0xffffffbf }, encoding_type::UTF8_BOM },
            { { 0xfffffffe, 0xffffffff }, encoding_type::UTF16_BE },
            { { 0xffffffff, 0xfffffffe }, encoding_type::UTF16_LE }
        };
    const std::map<encoding_type, std::vector<unsigned>> encoding_bom {
            { encoding_type::UTF8_BOM, { 0xFFFFFFEF, 0xFFFFFFBB, 0xFFFFFFBF } },
            { encoding_type::UTF16_BE, { 0xFFFFFFFE, 0xFFFFFFFF } },
            { encoding_type::UTF16_LE, { 0xFFFFFFFF, 0xFFFFFFFE } }
        };

    // Array maps values of encoding_type enum to representative strings
    inline const char*
        encoding_name[] = {
            "UNKNOWN", "UTF8", "UTF8_BOM", "UTF16_BE",
            "UTF16_LE"
        };

    // These maps are only provided for logging purposes
    const std::map<encoding_type, std::string> encoding_string {
            { encoding_type::UTF8_BOM, "UTF8_BOM" },
            { encoding_type::UTF16_BE, "UTF16_BE" },
            { encoding_type::UTF16_LE, "UTF16_LE" }
        };

    static bool
    check_valid_state( const std::vector<bool>& bools ) {
        bool result = false;
        for ( const auto b : bools )
            if ( b )
                return true;
#ifdef DEBUG
        std::cout << "<check_valid_state> State: ";
        if ( result )
            std::cout << "True" << std::endl;
        else
            std::cout << "False" << std::endl;
#endif
        return false;
    }

    template <typename T>
    static std::vector<T>
    apply_filter( const std::vector<T>& _data,
                  const indices_t& _filter ) {
        try {
            if ( _filter.size() == 0 ) { return _data; }
            uinteger sz = std::accumulate(
                                          _data.cbegin(), _data.cend(), static_cast<uinteger>( 0 ),
                                          []( uinteger a, const range_t& b ) {
                                              return a + (b.second - b.first);
                                          });
            // Calculate size of new vector:
            uinteger sz2 { 0 };
            for ( const auto& [first, last] : _filter ) { sz2 += (last - first); }
            assert(sz == sz2);
            std::vector<T> result;
            result.reserve(sz);

            for ( const auto& [first, last] : _filter ) {
                result.insert(result.end(), _data.begin() + first,
                              _data.begin() + last);
            }

            result.shrink_to_fit();
            return result;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <apply_filter>");
            return std::vector<T> {};
        }
    }


    class file_stats
    {
    public:
        std::map<std::string, integer> max_ints;   // Max int. val per col. title
        std::map<std::string, double> max_doubles; // "   double. " " " "
        std::map<std::string, integer> min_ints;   // Min int. " " " "
        std::map<std::string, double> min_doubles; // " double. " " " "
        std::map<std::string, double> max_strings; // double value must equal NaN
        std::map<std::string, double> min_strings; // ""
        IMap _n;                                   // n for each grouping of values ^

        file_stats() :
            max_ints({}),
            max_doubles({}),
            min_ints({}),
            min_doubles({}) {}
    };


    class file_data : protected memory_handle, public file_settings, public file_stats
    {
    private:
        // Path to additional configs
        std::filesystem::path config_loc_;
        // File data, e.g path, file size, etc.
        std::vector<std::filesystem::directory_entry> files_;
        std::vector<std::string_view> lines_; // Stores lines of text file
        // lines_ for each file loaded
        std::vector<std::vector<std::string_view>> file_lines_;
        std::vector<encoding_type> encodings_; // Type of encoding in file
        std::vector<memory_handle> handles_;   // Handles for multiple files
        std::vector<file_settings> settings_;  // Vector of settings for the files
        std::vector<bool> success_;           // Indicates successful parses
        IMap ints_;                           // Map of col. title --> std::vec<ints>
        DMap doubles_;                // Map of col. title --> std::vec<doubles>
        SMap strings_;                // Map of col. title --> std::vec<strings>
        std::vector<IMap> file_ints_; // Stores individiual file data before combining
        std::vector<uinteger> ints_lens_;
        std::vector<DMap> file_doubles_;
        std::vector<uinteger> doubles_lens_;
        std::vector<SMap> file_strings_;
        std::vector<uinteger> strings_lens_;
        std::vector<file_stats> statistics_;
        // std::vector<std::pair<uint64_t, uint64_t>> filters_;
        uinteger header_max_sz_; // Max size of file headers, set in spreadsheet
        // "Settings" page
        nano max_off_time_; // Max expected duration of OFF cycle during laser test
        // Internal time representation, as if files are continuous
        std::vector<nano> internal_time_;
        /*
        * Indicates boundary between files
        * - Key(uint64_t): Indicates order, should be unique
        * - Value(std::pair<nano, nano>): Timestamps; first is
        *   relative to the internal time representation, second
        *   is the true start time.
        * Empty if files_.size() == 0.
        */
        std::map<uinteger, std::pair<nano, std::chrono::sys_time<nano>>> file_boundaries_;
        // Controls whether detected test failures are trimmed from the data.
        bool do_trimming_;

        char*
        get() const noexcept;
        char*
        get( const uinteger& start, const uinteger& end ) const;
        bool
        async_get_files() noexcept;
        bool
        async_encode_adjust_files() noexcept;
        bool
        async_process_files() noexcept;
        bool
        async_text_to_lines() noexcept;
        bool
        async_parse_file_type( const std::filesystem::path& _configPath );
        bool
        async_header_halt_scan() noexcept;
        bool
        async_time_stamp_scan() noexcept;
        bool
        async_column_title_scan() noexcept;
        bool
        async_parse_data() noexcept;
        bool
        async_trim_data() noexcept;
        bool
        async_combine_data() noexcept;
        bool
        swap( const uinteger& _a, const uinteger& _b ) noexcept;
        bool
        erase( const uinteger& _pos ) noexcept;
        bool
        erase( const uinteger& _start, const uinteger& _end ) noexcept;
        bool
        clear_failed_loads() noexcept;
        bool
        sort_files() noexcept;
        bool
        recalculate_files() noexcept;

    public:
        file_data() :
            memory_handle {},
            file_settings { "", 256 },
            file_stats(),
            config_loc_(""),
            files_(0),
            lines_(0),
            file_lines_(0),
            encodings_({}),
            handles_({}),
            settings_({}),
            success_({}),
            ints_(),
            doubles_(),
            strings_(),
            ints_lens_(0),
            doubles_lens_(0),
            strings_lens_(0),
            statistics_(0),
            header_max_sz_ { 0 },
            max_off_time_ { 5min },
            internal_time_(),
            file_boundaries_(),
            do_trimming_ { true } {}

        file_data( const std::filesystem::directory_entry& file,
                   const std::filesystem::directory_entry& config_loc =
                       std::filesystem::directory_entry { "" },
                   const uinteger& header_max_lim = 256,
                   const nano& max_off_time = 5min,
                   const bool& trimming = true ) :
            memory_handle {},
            file_settings(file.path().string().substr(
                                                      file.path().string().rfind('.'),
                                                      file.path().string().length()),
                          header_max_lim),
            file_stats(),
            config_loc_ { config_loc },
            files_ { { file } },
            lines_(),
            file_lines_ { {} },
            encodings_({ encoding_type::UNKNOWN }),
            handles_ { { memory_handle {} } },
            settings_ { { file_settings { "", 256 } } },
            success_({ true }),
            ints_ { {} },
            doubles_ { {} },
            strings_ { {} },
            file_ints_(1),
            ints_lens_(1, 0),
            file_doubles_(1),
            doubles_lens_(1, 0),
            file_strings_(1),
            strings_lens_(1, 0),
            statistics_(1),
            header_max_sz_ { header_max_lim },
            max_off_time_ { max_off_time },
            internal_time_(),
            file_boundaries_(),
            do_trimming_ { trimming } {
            const Timer t;
            if ( !recalculate_files() ) {
                throw std::runtime_error("Failed to process files.");
            }
            write_log(std::format("- File processed, {}s.", t.elapsed()));
        }

        explicit file_data( std::vector<std::filesystem::directory_entry> files,
                            const std::filesystem::directory_entry& config_loc =
                                std::filesystem::directory_entry { "" },
                            const uinteger& header_max_lim = 256,
                            const nano& max_off_time = 5min,
                            const bool& trimming = true ) :
            memory_handle {},
            file_settings { std::string(""), header_max_lim },
            file_stats(),
            config_loc_(config_loc),
            files_(std::move(files)),
            lines_(0),
            encodings_({}),
            handles_({}),
            settings_({}),
            success_({}),
            ints_(),
            doubles_(),
            strings_(),
            file_ints_(0),
            ints_lens_(0),
            file_doubles_(0),
            doubles_lens_(0),
            file_strings_(0),
            strings_lens_(0),
            statistics_(0),
            header_max_sz_ { header_max_lim },
            max_off_time_ { max_off_time },
            internal_time_(),
            file_boundaries_(),
            do_trimming_ { trimming } {
            // Initialize vectors for async file processing
            const Timer t;
            if ( !recalculate_files() ) {
                throw std::runtime_error("Failed to process files.");
            }
            write_log(std::format("- File processed, {}s.", t.elapsed()));
        }

        ~file_data() = default;

        file_data( const file_data& _other ) = default;
        file_data&
        operator=( const file_data& _other ) noexcept;
        file_data( file_data&& _other ) = default;
        file_data&
        operator=( file_data&& _other ) noexcept;

        [[nodiscard]] std::vector<std::string> get_columns() const noexcept { return get_keys(get_col_types()); }

        [[nodiscard]] std::vector<nano> get_internal_time_nanoseconds() const noexcept { return internal_time_; }

        [[nodiscard]] std::vector<double> get_internal_time_double() const noexcept {
            std::vector<double> time_data;
            time_data.reserve(internal_time_.size());
            using seconds = std::chrono::duration<double, std::chrono::seconds::period>;
            for ( const auto& t : internal_time_ ) {
                time_data.emplace_back(std::chrono::duration_cast<seconds>(t).count());
            }
            return time_data;
        }

        [[nodiscard]] IMap get_i() const noexcept {
            try { return ints_; }
            catch ( const std::exception& err ) { write_err_log(err, "DLL: <file_data::get_i>"); }
        }

        [[nodiscard]] std::vector<integer> get_i( const std::string& _key ) const noexcept {
            try { return ints_.at(_key); }
            catch ( const std::exception& err ) {
                write_err_log(err, std::format("DLL: <file_data::get_i> (key = {})", _key));
                return std::vector<integer> {};
            }
        }

        [[nodiscard]] DMap get_d() const noexcept {
            try { return doubles_; }
            catch ( const std::exception& err ) {
                write_err_log(err, "DLL: <file_data::get_d>");
                return DMap {};
            }
        }

        [[nodiscard]] std::vector<double> get_d( const std::string& _key ) const noexcept {
            try { return doubles_.at(_key); }
            catch ( const std::exception& err ) {
                write_err_log(err, std::format("DLL: <file_data::get_d> (key = {})", _key));
                return std::vector<double> {};
            }
        }

        [[nodiscard]] SMap get_s() const noexcept {
            try { return strings_; }
            catch ( const std::exception& err ) {
                write_err_log(err, "DLL: <file_data::get_d>");
                return SMap {};
            }
        }

        [[nodiscard]] std::vector<std::string> get_s( const std::string& _key ) const noexcept {
            try { return strings_.at(_key); }
            catch ( const std::exception& err ) {
                write_err_log(err, std::format("DLL: <file_data::get_i> (key = {})", _key));
                return std::vector<std::string> {};
            }
        }

        [[nodiscard]] std::vector<IMap> get_vi() const noexcept { return file_ints_; }

        [[nodiscard]] std::vector<DMap> get_vd() const noexcept { return file_doubles_; }

        [[nodiscard]] std::vector<SMap> get_vs() const noexcept { return file_strings_; }

        [[nodiscard]] std::vector<bool> get_load_info() const noexcept { return success_; }

        bool
        add_file( const std::filesystem::directory_entry& file ) noexcept;
        bool
        add_files( const std::vector<std::filesystem::directory_entry>& files ) noexcept;
        bool
        remove_file( const uinteger& file ) noexcept;
        bool
        remove_files( const std::vector<uinteger>& indexes ) noexcept;

        [[nodiscard]] const std::vector<std::filesystem::directory_entry>& files() const noexcept { return files_; }

        [[nodiscard]] std::vector<encoding_type> get_encoding() const noexcept { return encodings_; }

        [[nodiscard]] file_stats get_stats() const noexcept { return *this; }

        // auto GetFilter() const noexcept { return filters_; }
        [[nodiscard]] DataType
        get_type( const std::string& title ) const noexcept;

        [[nodiscard]] bool do_trimming() const noexcept { return do_trimming_; }

        [[noreturn]] void do_trimming( const bool& b ) { do_trimming_ = b; }

        friend bool
        verify_configs( const std::map<std::string, nlohmann::json>& _configs,
                        const std::vector<std::string_view>& lines,
                        const uinteger& _header_lim, nlohmann::json& _result );
        friend encoding_type
        find_encoding_type( memory_handle& _handle );

        template <typename T>
        friend bool
        get_config_chunk( const nlohmann::json& _source, T& _target,
                          const std::vector<std::string>& _args ) noexcept;
        friend bool
        file_data_qsrt( file_data& _f, const uinteger& _lo,
                        const uinteger& _hi,
                        const uinteger& _depth ) noexcept;
    };


    inline file_data&
    file_data::operator=( const file_data& _other ) noexcept { // Copy assignment operator
        memory_handle::operator=(_other);
        file_settings::operator=(_other);
        file_stats::operator=(_other);

        config_loc_ = _other.config_loc_;
        files_ = _other.files_;
        lines_ = _other.lines_;
        file_lines_ = _other.file_lines_;
        encodings_ = _other.encodings_;
        handles_ = _other.handles_;
        settings_ = _other.settings_;
        success_ = _other.success_;
        ints_ = _other.ints_;
        doubles_ = _other.doubles_;
        strings_ = _other.strings_;
        file_ints_ = _other.file_ints_;
        ints_lens_ = _other.ints_lens_;
        file_doubles_ = _other.file_doubles_;
        doubles_lens_ = _other.doubles_lens_;
        file_strings_ = _other.file_strings_;
        strings_lens_ = _other.strings_lens_;
        statistics_ = _other.statistics_;
        header_max_sz_ = _other.header_max_sz_;
        max_off_time_ = _other.max_off_time_;
        internal_time_ = _other.internal_time_;
        do_trimming_ = _other.do_trimming_;

        return *this;
    }

    inline file_data&
    file_data::operator=( file_data&& _other ) noexcept { // Move assignment

        memory_handle::operator=(_other);
        file_settings::operator=(_other);
        file_stats::operator=(_other);

        config_loc_ = _other.config_loc_;
        files_ = _other.files_;
        lines_ = _other.lines_;
        file_lines_ = _other.file_lines_;
        encodings_ = _other.encodings_;
        handles_ = _other.handles_;
        settings_ = _other.settings_;
        success_ = _other.success_;
        ints_ = _other.ints_;
        doubles_ = _other.doubles_;
        strings_ = _other.strings_;
        file_ints_ = _other.file_ints_;
        ints_lens_ = _other.ints_lens_;
        file_doubles_ = _other.file_doubles_;
        doubles_lens_ = _other.doubles_lens_;
        file_strings_ = _other.file_strings_;
        strings_lens_ = _other.strings_lens_;
        statistics_ = _other.statistics_;
        header_max_sz_ = _other.header_max_sz_;
        max_off_time_ = _other.max_off_time_;
        internal_time_ = _other.internal_time_;
        do_trimming_ = _other.do_trimming_;

        return *this;
    }

    inline bool
    file_data::recalculate_files() noexcept {
        try {
            const std::string err_msg { "DLL: <file_data::recalculate_files>" };

            auto adjust_size =
                []<typename T>( std::vector<T>& v,
                                const uinteger& target_sz,
                                const T& fill_val ) -> bool {
                if ( v.size() < target_sz )
                    while ( v.size() != target_sz )
                        v.push_back(fill_val);
                else if ( v.size() > target_sz ) {
                    while ( v.size() != target_sz ) {
                        if ( v.size() == 0 )
                            return false;
                        v.pop_back();
                    }
                }
                return true;
            };

            adjust_size(encodings_, files_.size(), encoding_type::UNKNOWN);
            adjust_size(handles_, files_.size(), memory_handle {});
            adjust_size(settings_, files_.size(),
                        file_settings { std::string(""), header_max_sz_ });
            adjust_size(success_, files_.size(), true);
            adjust_size(file_lines_, files_.size(), {});
            adjust_size(file_ints_, files_.size(), IMap {});
            adjust_size(file_doubles_, files_.size(), DMap {});
            adjust_size(file_strings_, files_.size(), SMap {});
            adjust_size(ints_lens_, files_.size(), static_cast<uinteger>( 0 ));
            adjust_size(doubles_lens_, files_.size(), static_cast<uinteger>( 0 ));
            adjust_size(strings_lens_, files_.size(), static_cast<uinteger>( 0 ));
            adjust_size(statistics_, files_.size(), file_stats {});

            Timer t;
            if ( !this->async_get_files() ) { // Retrieve file
                write_err_log(std::runtime_error(err_msg), "FATAL ERROR: Failed to retreive files.");
                return false;
            }
            write_log(std::format(" - Files retrieved in {} seconds.", t.elapsed()));
            t.reset();
            if ( !this->async_encode_adjust_files() ) {
                // Scan & adjust file encoding if necessary
                write_err_log(std::runtime_error(err_msg), "FATAL ERROR: Failed to adjust file encoding.");
                return false;
            }
            write_log(std::format(" - Encoding adjusted in {} seconds.", t.elapsed()));
            t.reset();
            if ( !this->async_process_files() ) {
                // Perform all file processing required
                write_err_log(std::runtime_error(err_msg), "FATAL ERROR: Failed to process file.");
                return false;
            }
            write_log(std::format(" - File processed in {} seconds.", t.elapsed()));

            return true;
        }
        catch ( const std::exception& err ) {
            const std::string msg = "DLL: <file_data::recalculate_files>";
            write_err_log(err, msg);
            return false;
        }
    }

    inline bool
    file_data::add_file( const std::filesystem::directory_entry& file ) noexcept {
        try {
            files_.push_back(file);
            return recalculate_files();
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <file_data::add_file>");
            return false;
        }
    }

    inline bool
    file_data::add_files(
        const std::vector<std::filesystem::directory_entry>& files ) noexcept {
        try {
            files_.insert(files_.end(), files.begin(), files.end());
            return recalculate_files();
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <file_data::add_files>");
            return false;
        }
    }

    inline bool
    file_data::remove_file( const uinteger& file_idx ) noexcept {
        try {
            files_.erase(files_.begin() + file_idx);
            return recalculate_files();
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <file_data::remove_file>");
            return false;
        }
    }

    inline bool
    file_data::remove_files( const std::vector<uinteger>& indexes ) noexcept {
        try {
            std::vector<uinteger> sorted_list { indexes };

            // Sort indexes so largest at front, smallest at back
            std::qsort(sorted_list.data(), sorted_list.size(),
                       sizeof(uinteger), []( const void* a, const void* b ) {
                           return static_cast<int>( *( uinteger* ) b - *( uinteger* ) a );
                       });

            // Iterate through sorted_list, removing indexes
            for ( const auto& idx : sorted_list )
                files_.erase(files_.begin() + idx);

            return recalculate_files();
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <file_data::remove_files>");
            return false;
        }
    }

    static memory_handle
    get_file( const std::filesystem::directory_entry& _file ) noexcept {
        try {
            memory_handle handle;
#ifdef DEBUG
            std::string s = _file.path().string();
            std::string name = s.substr(s.rfind("\\") + 1, std::string::npos);
#endif
            if ( _file.exists() ) {
#ifdef DEBUG
                print("Opening: " + name, 3);
#endif
                std::fstream stream(_file.path(), std::ios::in | std::ios::binary);
                if ( !stream.is_open() ) {
#ifdef DEBUG
                    print("Failed to open " + name, 3);
#endif
                    return memory_handle {};
                }
                const uintmax_t true_file_size = _file.file_size();
                uinteger size =
                    (true_file_size > std::numeric_limits<uinteger>::max())
                        ? std::numeric_limits<uinteger>::max()
                        : static_cast<uinteger>( true_file_size );

                memory_handle::alloc(handle._data, size);

                if ( !handle._data ) {
#ifdef DEBUG
                    write_err_log(std::runtime_error("DLL: <file_data::get_file> Failed to allocate memory for file."));
#else
                    write_err_log( std::runtime_error("DLL: <file_data::get_file> Failed to allocate memory for file.") );
#endif
                    stream.close();
                    return memory_handle {};
                }
#ifdef DEBUG
                print(std::to_string(size) + " bytes allocated for file.", 3);
#endif
                handle._sz = size;

#ifdef DEBUG
                print("Reading stream buffer: ", 3);
                Timer t;
#endif
                std::filebuf* file_buffer = stream.rdbuf();
                file_buffer->sgetn(handle._data, size - 1);
                handle._data[size - 1] = '\0';
#ifdef DEBUG
                print("Buffer read in " + std::to_string(t.elapsed()) + " seconds.", 3);
#endif

                if ( stream.fail() || !handle._data ) {
#ifdef DEBUG
                    write_err_log(std::runtime_error("DLL: <file_data::get_file> Errorbit set in file stream."));
#else
                    write_err_log( std::runtime_error("DLL: <file_data::get_file> Errorbit set in file stream.") );
#endif
                    write_err_log(std::runtime_error("DLL: <file_data::get_file> "),
                                  std::format("eof: {}; fail: {}; bad: {}",
                                              stream.eof(), stream.fail(), stream.bad()));
                    handle.free();
                    stream.close();
                    return memory_handle {};
                }

                stream.close();
                return handle;
            }
#ifdef DEBUG
            print(name + " did not exist.", 3);
#endif
            return memory_handle {};
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <file_data::get_file>");
            return memory_handle {};
        }
    }

    inline bool
    file_data::async_get_files() noexcept {
        try {
            std::vector<std::future<memory_handle>> futures(files_.size());

            for ( uinteger i = 0; i < files_.size(); ++i ) { futures[i] = std::async(get_file, std::cref(files_[i])); }

            for ( uinteger i = 0; i < files_.size(); ++i ) {
                if ( success_[i] ) {
                    handles_[i] = futures[i].get();
                    if ( !handles_[i]._data )
                        success_[i] = false;
                }
            }

            if ( !check_valid_state(success_) ) { return false; }

            return true;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "<file_data::async_get_files>");
            return false;
        }
    }

    static encoding_type
    find_encoding_type( memory_handle& _handle ) {
        // Set default return type & ptr for characters
        auto result = encoding_type::UNKNOWN; // --> Assume UTF8 w/ no BOM
        char* chars = nullptr;

        std::size_t max_encode_len = 0;
        for ( const auto& value : encoding_bom | std::views::values ) {
            if ( value.size() > max_encode_len )
                max_encode_len = value.size();
        }

        chars = new char[max_encode_len];

        for ( auto& [type, encode_arr] : encoding_bom ) // Iter. through types of encoding
        {
#ifdef DEBUG
            print("- Checking encoding type: " + encoding_string.at(type), 4);
#endif
            _handle.get(chars, _handle._data,
                        encode_arr.size()); // get x chars to compare against
            if ( !chars )                    // Failed break out & return default
            {
                break;
            }

            bool matching = true;
            for ( uinteger i = 0; i < encode_arr.size(); ++i ) {
                if ( !(matching = (static_cast<unsigned>( chars[access_checked(i)] ) ==
                                   encode_arr[access_checked(i)])) )
                    break;
            }

            if ( matching ) // Match found!
            {
                result = type;
                break;
            }
        }

        delete[] chars;

        return result;
    }

    static bool
    encode_adjust_file( encoding_type& _encoding, memory_handle& _handle ) {
        char* tmp_data = nullptr;
        try {
            _encoding = find_encoding_type(_handle);

            if ( _encoding != encoding_type::UNKNOWN ) {
#ifdef DEBUG
                print("- Encoding found: " + encoding_string.at(_encoding) +
                      ", adjusting...",
                      3);
#endif
                const uinteger offset = encoding_bom.at(_encoding).size();
                tmp_data = new char[access_checked(_handle._sz - offset)];

                _handle.get(tmp_data, offset, _handle._sz);
                _handle.free();

                _handle._sz -= offset;

                _handle._data = tmp_data;
            }

            return true;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <file_data::encode_adjust_file>");
            _encoding = encoding_type::UNKNOWN;
            _handle.free();

            delete[] tmp_data;

            return false;
        }
    }

    // Scan & account for encoding
    inline bool
    file_data::async_encode_adjust_files() noexcept {
        try {
            std::vector<std::future<bool>> futures(handles_.size());

            for ( uinteger i { 0 }; i < handles_.size(); ++i ) {
                if ( success_[i] ) {
                    futures[i] = std::async(encode_adjust_file, std::ref(encodings_[i]),
                                            std::ref(handles_[i]));
                }
                else {
                    encodings_[i] = encoding_type::UNKNOWN;
                    handles_[i].free();
                }
            }

            for ( uinteger i = 0; i < files_.size(); ++i ) { if ( success_[i] ) { success_[i] = futures[i].get(); } }

            if ( !check_valid_state(success_) )
                return false;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <file_data::async_encode_adjust_files>");
            return false;
        }

        return true;
    }

    inline bool
    file_data::async_text_to_lines() noexcept {
        try {
            std::vector<std::future<bool>> futures;

            for ( uinteger i = 0; i < files_.size(); ++i ) {
                if ( success_[i] ) {
                    futures.push_back(std::async(text_to_lines,
                                                 std::string_view { handles_[i].get() },
                                                 std::ref(file_lines_[i])));
                }
            }
            for ( uinteger i = 0; i < files_.size(); ++i ) { success_[i] = (success_[i] && futures[i].get()); }

            if ( !check_valid_state(success_) )
                return false;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <file_data::aync_text_to_lines>");
            return false;
        }

        return true;
    }

    // Transform raw data into array of extracted data.
    // Will eventually pass some form of settings to this.
    // Should handle things like sensible defaults when a char
    // which can't be converted is found.

    // Also delimiters, etc.
    inline bool
    file_data::async_process_files() noexcept {
        Timer t;
        if ( !async_text_to_lines() ) { return false; }
        // Scan for file style, e.g Starlabs .txt or normal .csv style.
        if ( !async_parse_file_type(config_loc_) ) { return false; }
        if ( !async_header_halt_scan() ) { return false; }
        if ( !async_time_stamp_scan() ) { return false; }
        if ( !async_column_title_scan() ) { return false; }
        write_log(std::format("   - Header info parsed in {} seconds.", t.elapsed()));
        t.reset();
        if ( !async_parse_data() ) { return false; }
        write_log(std::format("   - Data parsed in {} seconds.", t.elapsed()));
        t.reset();
        const Timer t2;
        if ( !async_trim_data() ) { return false; }
        write_log(std::format("     - Data trimmed in {} seconds.", t.elapsed()));
        if ( !async_combine_data() ) { return false; }
        write_log(std::format("     - Data combined in {} seconds.", t.elapsed()));
        write_log(std::format("   - Post-processing completed in {} seconds.", t2.elapsed()));
        return true;
    }

    inline char*
    file_data::get() const noexcept {
        const auto result = new char[access_checked(this->_sz)];
        memory_handle::get(result, this->_data, this->_sz);
        if ( result[this->_sz - 1] != '\0' ) { result[this->_sz - 1] = '\0'; }
        return result;
    }

    inline char*
    file_data::get( const uinteger& start, const uinteger& end ) const {
        uinteger offset = 0;

        if ( start <= this->_sz - 1 && end <= this->_sz - 1 )
            offset = 1;

        const uinteger r_sz =
            (start <= end)
                ? (end - start) + offset
                : (start - end) + offset;
        const auto result = new char[access_checked(r_sz)];

        memory_handle::get(result, start, end);

        if ( offset )
            result[r_sz - 1] = '\0';

        return result;
    }

    inline DataType
    file_data::get_type( const std::string& title ) const noexcept {
        try {
            if ( title == std::string{ "Combined Time" } ) {
                return DataType::DOUBLE;
            }
            for ( const auto& [key, type] : get_col_types() ) {
                if ( title == key )
                    return type;
            }
            throw std::out_of_range("Key " + title + " does not exist.");
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <file_data::get_type>");
            return DataType::NONE;
        }
    }

    template <typename T>
    static bool
    get_config_chunk( const nlohmann::json& _source, T& _target,
                      const std::vector<std::string>& _args ) noexcept {
        // std::cout << std::this_thread::get_id() << ": " << _source.dump() <<
        // "\n\n";
        nlohmann::json tmp;
        try {
            if ( _args.size() > 0 ) {
                tmp = _source.at(_args[0]);
                for ( auto i = _args.begin() + 1; i != _args.end(); ++i ) { tmp = tmp.at(*i); }
                _target = static_cast<T>( tmp );
                return true;
            }
            return false;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <get_config_chunk>");
            return false;
        }
    }

    static bool
    verify_configs( const std::map<std::string, nlohmann::json>& _configs,
                    const std::vector<std::string_view>& lines,
                    const uinteger& _header_lim,
                    nlohmann::json& _result ) {
        try {
            for ( auto& [name, config] : _configs ) {
#ifdef DEBUG
                write_log(std::format("\t- Checking: {}", name));
#endif
                std::string pattern_string;
                try { pattern_string = config.at("file_identifier").get<std::string>(); }
                catch ( [[maybe_unused]] const std::exception& err ) {
                    write_log(std::format("DLL: <verify_configs> Invalid config detected -> {}", name));
                    continue;
                }

                if ( uinteger x { _header_lim }; line_check(lines, pattern_string, x) ) {
#ifdef DEBUG
                    print("Success.", 4);
#endif
                    write_log(std::format("     - Config matched: {}", name));
                    _result = config;
                    return true;
                }
            }

#ifdef DEBUG
            print("Failure.", 4);
#endif
            _result = nlohmann::json::parse("{}");
            return false;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <verify_configs>");
            return false;
        }
    }

    static bool
    retrieve_ext_configs( const std::filesystem::path& _configPath,
                          std::vector<nlohmann::json>& _storage ) {
        try {
            bool result = false;

            _storage = {};
            const std::filesystem::directory_entry _configLocation(_configPath);
            const std::string path { _configPath.string() };

            if ( !_configLocation.exists() ) {
                print("WARNING: <retrieve_ext_configs> No external config location found.",
                      4);
                return true;
            }

            if ( _configLocation.is_directory() ) {
                for ( auto& entry : std::filesystem::directory_iterator(_configLocation) ) {
                    const auto filename { entry.path().string() };
                    if ( const auto f_extension_pos { filename.rfind('.') }; f_extension_pos == std::string::npos ) { }
                    else if ( const auto extension { filename.substr(f_extension_pos, filename.length()) };
                        extension == ".json" ) {
                        nlohmann::json config;
                        if ( parse_json(entry, config) ) { _storage.push_back(config); }
                    }
                }

                return true;
            }
            if ( _configLocation.is_regular_file() &&
                 path.substr(path.rfind('.'), path.length()) == ".json" ) {
                nlohmann::json config;
                if ( parse_json(_configPath, config) ) { _storage = { config }; }

                return true;
            }
            write_err_log(std::runtime_error("DLL: <retrieve_ext_configs> Invalid config location received."));
            return false;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <retrieve_ext_configs>");
            _storage.clear();
            return false;
        }
    }

    inline bool
    file_data::async_parse_file_type( const std::filesystem::path& _configPath ) {
        try {
            std::map<std::string, nlohmann::json> potential_configs {};
            std::vector<nlohmann::json> ext_configs;

            if ( !retrieve_ext_configs(_configPath, ext_configs) ) {
                write_err_log(std::runtime_error("DLL: <file_data::async_parse_file_type> \"retrieve_ext_configs\" failed"));
                return false;
            }

            // Add read in configs to defaults
            for ( auto& config : ext_configs ) {
                try {
                    std::string config_name = config.at("name");
                    if ( !potential_configs.contains(config_name) ) { potential_configs[config_name] = config; }
                    else {
                        write_err_log(std::runtime_error("DLL: <file_data::async_parse_file_type> namespace collision in config files."));
                    }
                }
                catch ( const std::exception& err ) {
                    write_err_log(err, "DLL: <file_data::async_parse_file_type> Invalid config file.");
                }
            }

            std::vector<std::future<bool>> futures(files_.size());
            std::vector<nlohmann::json> configs(files_.size());

            for ( uinteger i = 0; i < files_.size(); ++i ) {
                if ( success_[i] ) {
                    futures[i] = std::async(
                                            verify_configs, potential_configs, std::cref(file_lines_[i]),
                                            settings_[i].get_headermaxlim(), std::ref(configs[i]));
                }
            }

            for ( uinteger i = 0; i < files_.size(); ++i ) {
                if ( success_[i] ) {
                    success_[i] = futures[i].get();

                    if ( success_[i] ) {
                        settings_[i].set_config(configs[i]);
                        settings_[i].set_format(FileFormat(configs[i].at("name"), configs[i]));
#ifdef DEBUG
                        print("LOGGING CONFIG: " +
                              std::string(settings_[i].get_config().dump()),
                              4);
#endif
                    }
                }
            }

            if ( !check_valid_state(success_) )
                return false;

            return true;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <file_data::async_parse_file_type>");
            return false;
        }
    }

    inline bool
    file_data::async_header_halt_scan() noexcept {
        try {
            std::vector<std::future<bool>> futures(files_.size());
            std::vector<uinteger> header_lims(files_.size());

            // Launch thread checking for header limit of each file.
            for ( uinteger i = 0; i < files_.size(); ++i ) {
                if ( success_[i] ) // Only launch if file has been successfully loaded so
                // far.
                {
                    // get config -> regex pattern & get maximum header limit set in
                    // spreadsheet.
                    nlohmann::json config = settings_[i].get_config();
                    auto pattern = std::regex(config.at("header_identifier"));
                    header_lims[i] = settings_[i].get_headermaxlim();

                    // Launch async process executing line_check function
                    futures[i] =
                        std::async(
                            static_cast<bool(*)(const std::vector<std::string_view>&, const std::regex&, uinteger&)>(line_check),
                            std::cref(file_lines_[i]), pattern, std::ref(header_lims[i])
                        );
                }
            }

            for ( uinteger i = 0; i < files_.size(); ++i ) {
                if ( success_[i] ) {
                    // Set header limit to start of data
                    if ( futures[i].get() )
                        settings_[i].set_header_lim(header_lims[i] + 1);
                    // process failed
                    else {
                        settings_[i].set_header_lim(0);
                        success_[i] = false;
                    }
                }
            }

            if ( !check_valid_state(success_) )
                return false;

            return true;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <file_data::async_header_halt_scan>");
            return false;
        }
    }

    static bool
    time_stamp_scan( const std::filesystem::directory_entry& file,
                     const std::vector<std::string_view>& lines,
                     file_settings& settings ) noexcept {
        try {
            auto file_write_time =
                std::chrono::sys_time<nano>(get_file_write_time<nano>(file));
            settings.set_last_write(file_write_time);

            std::string method = settings.get_config().at("start_time").at("method");
            auto& params = settings.get_config().at("start_time").at("params");
            std::vector<std::string> start_matches, increment_matches;

#ifdef DEBUG
            write_log("Checking test start time:\n");
#endif
            // get test start time
            if ( method == "InFile" ) {
                const std::regex start_pattern = params.at("re_pattern");
                if ( line_capture(lines, start_matches, start_pattern,
                                  settings.get_header_lim()) ) {
                    assert(start_matches.size() == 2);
                    const std::chrono::sys_time<nano> start_time =
                        time_format(start_matches[1], std::string { params.at("time_pattern") },
                                    std::chrono::system_clock::time_point {});
                    settings.set_start_time(start_time);
                }
            }
            else if ( method == "InFilePath" ) {
                const std::regex start_pattern = params.at("re_pattern");
                std::smatch match;
                std::string path_str { file.path().string() };
                if ( std::regex_search(path_str, match, start_pattern) ) {
                    const std::chrono::sys_time<nano> start_time =
                        time_format(match.str(), std::string { params.at("time_pattern") },
                                    std::chrono::system_clock::time_point{});
                    settings.set_start_time(start_time);
                }
            }
            else if ( method == "InData" ) {
                const std::regex start_pattern = params.at("re_pattern");
                const std::string delim{ settings.get_config().at("delim").get<std::string>() };

                // Parse start time from first line of data
                const auto& data{ lines[settings.get_header_lim()] };
                const auto split_data{ split_str(data, delim) };
                const auto start_time_str{ trim(split_data[params.at("col_index").get<uinteger>()]) };
                settings.set_start_time(
                    time_format(start_time_str, params.at("time_pattern").get<std::string>(),
                        std::chrono::system_clock::time_point{}
                    )
                );
            }
            else {
                // Some sort of failure
                throw std::runtime_error("<time_stamp_scan> Unknown test start time scanning method.");
            }
#ifdef DEBUG
            write_log(std::format("Finished, start time: {}.\n", settings.get_start_time()));
#endif

            method = settings.get_config().at("interval").at("method").get<std::string>();
            params = settings.get_config().at("interval").at("params");

#ifdef DEBUG
            write_log("Checking interval:\n");
#endif
            // get interval period
            if ( method == "InFile" ) {
                const std::regex incr_pattern = params.at("re_pattern");
                if ( line_capture(lines, increment_matches, incr_pattern,
                                  settings.get_header_lim()) ) {
                    const nano incr_time =
                        time_format(increment_matches[1],
                                    std::string { params.at("time_pattern") }, nano::zero());
                    settings.set_measurement_period(incr_time);
                }
            }
            else if ( method == "value" ) {
                using seconds = std::chrono::duration<integer, std::ratio<1i64>>;
                const seconds incr_time { params.at("increment").get<integer>() };
                settings.set_measurement_period(incr_time);
            }
            else if ( method == "Automatic" ) {
                if ( !is_in(params.at("title").get<std::string>(),
                            settings.get_config().at("titles").get<std::vector<std::string>>()) ) {
                    throw std::runtime_error("Invalid title parsed in configuration file to automatically detect measurement interval.");
                }
                // Data hasn't been read in yet, interval is detected in parsing step
            }
            else {
                throw std::runtime_error(
                    std::format("Invalid \"interval\" method: \"{}\" received. Check for mistakes in {} configuration.",
                    method, settings.get_config().at("name").get<std::string>()) );
            }
#ifdef DEBUG
            if ( method != "Automatic" ) {
                write_log(std::format("Success, interval: {}.\n", settings.get_measurement_period()));
            }
            else { write_log("Automatic detection, interval checking postponed.\n"); }
#endif

            return true;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <time_stamp_scan>");
            return false;
        }
    }

    inline bool
    file_data::async_time_stamp_scan() noexcept {
        try {
            std::vector<std::future<bool>> future(files_.size());
            for ( uinteger i = 0; i < files_.size(); ++i ) {
                if ( success_[i] ) {
                    future[i] =
                        std::async(time_stamp_scan, std::cref(files_[i]),
                                   std::cref(file_lines_[i]), std::ref(settings_[i]));
                }
            }

            for ( uinteger i = 0; i < files_.size(); ++i ) { if ( success_[i] ) { success_[i] = future[i].get(); } }

            if ( !check_valid_state(success_) )
                return false;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <file_data::async_time_stamp_scan>");
            return false;
        }

        return true;
    }

    static bool
    column_title_scan( const std::vector<std::string_view>& lines,
                       file_settings& settings ) noexcept {
        try {
            const std::vector<std::string> cols = settings.get_config().at("titles");
            auto col_types =
                settings.get_config().at("types").get<std::vector<DataType>>();

            if ( cols.size() != col_types.size() ) {
                throw
                    std::runtime_error("Length mismatch between \"titles\" and \"types\" parameters in configuration file.");
            }
            // Add [title, type] pairs to type map in Settings object
            /*for ( uinteger i = 0; i < cols.size(); ++i ) { */
            for ( const auto& [i, col_title] : enumerate(cols) ) {
                settings.col_types()[col_title] = col_types[i];
                settings.col_order()[col_title] = i;
            }
            settings.set_n_cols(cols.size());

            return true;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <column_title_scan>");
            return false;
        }
    }

    inline bool
    file_data::async_column_title_scan() noexcept {
        try {
            std::vector<std::future<bool>> futures(files_.size());

            for ( uinteger i = 0; i < files_.size(); ++i ) {
                if ( success_[i] ) {
                    futures[i] = std::async(column_title_scan, std::cref(file_lines_[i]),
                                            std::ref(settings_[i]));
                }
            }

            for ( uinteger i = 0; i < files_.size(); ++i ) {
                if ( success_[i] ) {
                    success_[i] = futures[i].get();
                    for ( const auto& [title, type] : settings_[i].col_types() ) {
                    }
                }
            }

            if ( !check_valid_state(success_) )
                return false;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <file_data::async_column_title_scan>");
            return false;
        }

        return true;
    }

    static bool
    parse_data( const std::vector<std::string_view>& lines,
                file_settings& settings, IMap& ints, uinteger& ints_len,
                DMap& doubles, uinteger& doubles_len, SMap& strings,
                uinteger& strings_len, file_stats& statistics ) noexcept {
        try {
#ifdef DEBUG
            print("- <parse_data>:\n", 3);
#endif
            nlohmann::json config = settings.get_config();
            const auto& col_types = settings.col_types();
            const std::string delim { config.at("delim").get<std::string>() };

#ifdef DEBUG
            print("- Allocating storage.", 3);
#endif
            // Prepare storage to emplace_back values efficiently
            for ( auto& [key, type] : col_types ) {
                if ( key == "Combined Time" ) { continue; }

                switch ( type ) {
                case DataType::INTEGER:
                    ints[key].clear();
                    ints[key].reserve(lines.size() - settings.get_header_lim());
                    break;
                case DataType::DOUBLE:
                    doubles[key].clear();
                    doubles[key].reserve(lines.size() - settings.get_header_lim());
                    break;
                case DataType::STRING:
                    strings[key].clear();
                    strings[key].reserve(lines.size() - settings.get_header_lim());
                    break;
                case DataType::NONE:
                    strings[key].clear();
                    strings[key].reserve(lines.size() - settings.get_header_lim());
                    write_err_log(std::runtime_error("DLL: <parse_data> None type encountered while parsing."));
                    break;
                }
            }

#ifdef DEBUG
            print("- Parsing file.", 3);
#endif
            // Iterate through data lines
            for ( uinteger i { settings.get_header_lim() }; i < lines.size(); ++i ) {
                const std::vector<std::string_view> values = split_str(lines[i], delim);

                for ( const auto& [col_title, idx] : settings.col_order() ) {
                    if ( col_title == "Combined Time" ) { continue; }

                    const auto col_type{ settings.get_type(col_title) };
                    const std::string_view val{ values[idx]};

                    char buf[MAX_LINE_LENGTH];
                    // sscanf, which is used to parse values in further on, automatically
                    // trims whitespace at start & finish.
                    if ( col_type != DataType::STRING ) {
                        memcpy_s(buf, MAX_LINE_LENGTH,
                                 val.data(), val.size());

                        if ( val.size() < MAX_LINE_LENGTH )
                            buf[val.size()] = '\0';
                        else
                            buf[MAX_LINE_LENGTH - 1] = '\0';
                    }

                    // Scan values in
                    int sscanf_return_val { 0 };
                    switch ( col_type ) {
                    case DataType::INTEGER: {
                        integer _integer { 0 };
                        sscanf_return_val =
                            sscanf_s(buf, FMT, &_integer);
                        ints[col_title].emplace_back(_integer);
                        break;
                    }
                    case DataType::DOUBLE: {
                        double _double { 0.0 };
                        sscanf_return_val =
                            sscanf_s(buf, "%lf", &_double);
                        doubles[col_title].emplace_back(_double);
                        break;
                    }
                    case DataType::STRING:
                        strings[col_title].emplace_back(trim(val));
                        break;
                    case DataType::NONE:
                        strings[col_title].emplace_back("");
                    }

                    // TODO: Add some error handling
                    /*if ( sscanf_return_val == EOF ||
                         sscanf_return_val == EINVAL ) {}*/
                }
            }

            const auto set_max =
                []( const IMap& _ints, const DMap& _doubles,
                    const file_settings& _settings, file_stats& _stats ) {
                const auto title_type_vec = _settings.get_col_types();
                for ( const auto& title_type : title_type_vec ) {
                    switch ( title_type.second ) {
                    case DataType::STRING: {
                        _stats.max_strings[title_type.first] =
                            std::numeric_limits<double>::signaling_NaN();
                        break;
                    }
                    case DataType::INTEGER: {
                        const auto& data = _ints.at(title_type.first);
                        _stats.max_ints[title_type.first] = check_max(data);
                        break;
                    }
                    case DataType::DOUBLE: {
                        const auto& data = _doubles.at(title_type.first);
                        _stats.max_doubles[title_type.first] = check_max(data);
                        break;
                    }
                    case DataType::NONE: {}
                        break;
                    }
                }
            };
            const auto set_min =
                []( const IMap& _ints, const DMap& _doubles,
                    const file_settings& _settings, file_stats& _stats ) {
                const auto title_type_vec = _settings.get_col_types();
                for ( const auto& [title, type] : title_type_vec ) {
                    switch ( type ) {
                    case DataType::STRING: {
                        _stats.min_strings[title] =
                            std::numeric_limits<double>::signaling_NaN();
                        break;
                    }
                    case DataType::INTEGER: {
                        const auto& data = _ints.at(title);
                        _stats.min_ints[title] = check_min(data);
                        break;
                    }
                    case DataType::DOUBLE: {
                        const auto& data = _doubles.at(title);
                        _stats.min_doubles[title] = check_min(data);
                        break;
                    }
                    case DataType::NONE: {}
                        break;
                    }
                }
            };
            const auto set_n =
                []( const IMap& _ints, const DMap& _doubles,
                    const SMap& _strings, const file_settings& _settings,
                    file_stats& _stats ) {
                const auto title_type_vec { _settings.get_col_types() };
                for ( const auto& title_type : title_type_vec ) {
                    switch ( title_type.second ) {
                    case DataType::STRING:
                        _stats._n[title_type.first] = {
                                static_cast<integer>( _strings.at(title_type.first).size() )
                            };
                        break;
                    case DataType::INTEGER:
                        _stats._n[title_type.first] = {
                                static_cast<integer>( _ints.at(title_type.first).size() )
                            };
                        break;
                    case DataType::DOUBLE:
                        _stats._n[title_type.first] = {
                                static_cast<integer>( _doubles.at(title_type.first).size() )
                            };
                        break;
                    }
                }
            };

            std::future<void> launch_max =
                std::async(set_max, std::cref(ints), std::cref(doubles),
                           std::cref(settings), std::ref(statistics));
            std::future<void> launch_min =
                std::async(set_min, std::cref(ints), std::cref(doubles),
                           std::cref(settings), std::ref(statistics));
            std::future<void> launch_n =
                std::async(set_n, std::cref(ints), std::cref(doubles), std::cref(strings),
                           std::cref(settings), std::ref(statistics));

            if ( std::string method = settings.get_config().at("interval").at("method");
                method == "Automatic" ) {
                std::string title = settings.get_config().at("interval").at("params").at("title");
#ifdef DEBUG
                print("- Automatic interval detection:", 4);
#endif
                /*
                * Title isn't in file?? This should have already been filtered out
                * but just in case.
                */
                if ( !is_in(title, col_types) ) {
                    throw std::runtime_error(
                        std::format("No match for \"Automatic\" interval parsing title ({}) in file.", title)
                    );
                }
#ifdef DEBUG
                write_log(std::format("- Provided title, {}, exists.", title));
#endif

                /*
                * Title is in file, but data isn't a numeric type.
                */
                if ( col_types.at(title) != DataType::INTEGER
                     && col_types.at(title) != DataType::DOUBLE ) {
                    std::runtime_error err(
                        std::format(
                            "\"Automatic\" interval parsing title ({}) has invalid DataType ({}).",
                            title, type_string.at(col_types.at(title))
                        )
                    );
                    throw err;
                }

                const auto avg_diff =
                    []<ArithmeticType T>( const std::vector<T>& data ) -> double {
                        T sum_diff { 0 };
                        for ( uinteger i { 1 }; i < data.size(); ++i ) { sum_diff += std::abs(data[i] - data[i - 1]); }
                        return sum_diff / static_cast<double>( data.size() - 1 );
                    };
                auto std_diff =
                    []<ArithmeticType T>( const std::vector<T>& data,
                                          const double& mean ) -> double {
                        double sum { 0 };
                        for ( uinteger i { 1 }; i < data.size(); ++i ) {
                            sum += std::pow(std::abs(std::abs(data[i] - data[i - 1]) - mean), 2);
                        }
                        return std::sqrt(sum / static_cast<double>( data.size() - 1 ));
                    };

#ifdef DEBUG
                print("- Valid title confirmed.", 4);
#endif

                double mean { 0 }, std { 0 };
                switch ( col_types.at(title) ) {
                case DataType::INTEGER: {
                    const auto& data = ints.at(title);
                    mean = avg_diff(data);
                    std = std_diff(data, mean);
                    break;
                }
                case DataType::DOUBLE: {
                    const auto& data = doubles.at(title);
                    mean = avg_diff(data);
                    std = std_diff(data, mean);
                    break;
                }
                case DataType::STRING: {
                    mean = std::numeric_limits<double>::signaling_NaN();
                    std = std::numeric_limits<double>::signaling_NaN();
                    break;
                }
                case DataType::NONE:
                    break;
                }

#ifdef DEBUG
                write_log(std::format("- mean: {}, std: {}.", mean, std));
#endif

                using dur = std::chrono::duration<double, std::chrono::seconds::period>;
                if ( std > (0.25 * mean) ) {
#ifdef DEBUG
                    print("- Failed to automatically detect interval.", 3);
#endif
                    write_err_log(std::runtime_error("<parse_data> Failed to detect consistent measurement interval."));
                    settings.set_measurement_period(std::chrono::duration_cast<nano>(dur { mean }));
                }
                else {
#ifdef DEBUG
                    write_log(std::format("- Automatically detect interval: {}.", mean));
#endif
                    settings.set_measurement_period(std::chrono::duration_cast<nano>(dur { mean }));
                }
            }

            ints_len = 0;
            doubles_len = 0;
            strings_len = 0;
            for ( auto& val : ints | std::views::values ) {
                val.shrink_to_fit();
                uinteger x = val.size();
                if ( ints_len == 0 )
                    ints_len = x;
                assert(x == ints_len);
            }
            for ( auto& val : doubles | std::views::values ) {
                val.shrink_to_fit();
                uinteger x = val.size();
                if ( doubles_len == 0 )
                    doubles_len = x;
                assert(x == doubles_len);
            }
            for ( auto& val : strings | std::views::values ) {
                val.shrink_to_fit();
                uinteger x = val.size();
                if ( strings_len == 0 )
                    strings_len = x;
                assert(x == strings_len);
            }

            uinteger max_val = (ints_len >= doubles_len)
                                   ? ints_len
                                   : doubles_len;
            max_val = (max_val >= strings_len)
                          ? max_val
                          : strings_len;

            if ( !ints.empty() && ints_len != max_val ||
                 !doubles.empty() && doubles_len != max_val ||
                 !strings.empty() && strings_len != max_val ) {
                throw std::runtime_error { "ERROR: <parse_data> Size mismatch between columns." };
            }

#ifdef _DEBUG
            print("Row count: " + std::to_string(max_val), 2);
#endif

            settings.set_n_rows(max_val);

            launch_max.get();
            launch_min.get();
            launch_n.get();

            return true;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <parse_data>");
            return false;
        }
    }

    inline bool
    file_data::async_parse_data() noexcept {
        try {
            std::vector<std::future<bool>> futures(files_.size());
            for ( uinteger i = 0; i < files_.size(); ++i ) {
                if ( success_[i] ) {
                    futures[i] =
                        std::async(parse_data, std::cref(file_lines_[i]),
                                   std::ref(settings_[i]), std::ref(file_ints_[i]),
                                   std::ref(ints_lens_[i]), std::ref(file_doubles_[i]),
                                   std::ref(doubles_lens_[i]), std::ref(file_strings_[i]),
                                   std::ref(strings_lens_[i]), std::ref(statistics_[i]));
                }
            }
            for ( uinteger i = 0; i < files_.size(); ++i ) {
                if ( success_[i] ) {
                    success_[i] = futures[i].get();
                }
            }

            if ( !check_valid_state(success_) )
                return false;

            return true;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <file_data::async_parse_data>");
            return false;
        }
    }

    static bool
    trim_data( file_settings& settings,
               const file_stats& statistics, IMap& ints, uinteger& ints_len,
               DMap& doubles, uinteger& doubles_len, SMap& strings,
               uinteger& strings_len, const nano& max_off_time ) noexcept {
        /*
         * Current CW LTT cycle lasers with 5mins ON & 1min OFF. Legacy tests did
         * 5mins ON & 5mins OFF Therefore any data where laser power < some threshold
         * for time > MAX(OFF cycle time) is likely due to a test error. This error
         * could be caused by part failure, test equipment failure, or many other
         * things so it's hard to prevent and we want to remove it from the data
         * before filtering, averaging or adding an internal relative time scale to
         * the combined data.
         *
         * - get measurement interval of current file.
         * - Scan data for invalid data, making note of any found.
         * - Trim out invalid data, replace in storage.
         * - Return true.
         * - Else return false.
         */

        try {
            const auto& config = settings.get_config();
            const auto& filter_key = config.at("trim_filter_key").get<std::string>();
            const auto& type_map = settings.get_col_types();
            if ( !type_map.contains(filter_key) || filter_key == "Combined Time" ) {
                write_log(std::format(" | Invalid or no trim key provided ({}). | ", filter_key));
                return true;
            }
            const auto& interval = settings.get_measurement_period();
            const auto& data_type = type_map.at(filter_key);

            assert(data_type != DataType::NONE);

            auto measure_downtime =
                [&interval, &max_off_time]<ArithmeticType T>(
                const std::vector<T>& data,
                const T& cutoff ) -> indices_t {
                indices_t result;
                auto downtime = nano::zero();
                uinteger first { 0 };

                for ( auto [i, datapoint] : enumerate(data) ) {
                    #define cont_timer 1
                    #define restart_timer 0
                    switch ( datapoint >= cutoff
                                 ? restart_timer
                                 : cont_timer ) {
                    case cont_timer:
                        downtime += interval;
                        break;
                    case restart_timer:
                        downtime = nano::zero();
                        first = i + 1;
                        break;
                    default:
                        break;
                    }
                    #undef cont_timer
                    #undef restart_timer

                    // If "downtime" > max time expected for OFF section of laser cycle
                    if ( downtime > max_off_time ) {
                        // Find full OFF time, push to result, increment i to the next
                        // datapoint
                        for ( uinteger j { i }; j < data.size(); ++j ) {
                            downtime += interval;
                            if ( data[j] > cutoff || j == data.size() - 1 ) {
                                result.emplace_back(std::pair { first, j + 1 });
                                i = j + 1;
                                break;
                            }
                        }
                    }
                }

                return result;
            };

            indices_t trim_ranges;
            // get sections of assumed invalid data
            switch ( data_type ) {
            case DataType::INTEGER: {
                trim_ranges = measure_downtime(ints.at(filter_key),
                                               static_cast<integer>( 0.5 * statistics.max_ints.at(filter_key) ));
                break;
            }
            case DataType::DOUBLE: {
                trim_ranges = measure_downtime(doubles.at(filter_key),
                                               0.5 * statistics.max_doubles.at(filter_key));
                break;
            }
            case DataType::STRING: { throw bad_DataType("Invalid trim filter type: " + type_string.at(data_type)); }
            case DataType::NONE:
                break;
            }

            // Remove sections of invalid data from all columns & update length data
            if ( trim_ranges.empty() ) { return true; }

            const auto remove_data =
                [&trim_ranges]<typename T>( TMap<T>& map, const std::string& key ) {
                    for ( const auto& [first, last]
                          : trim_ranges | std::views::reverse ) {
                        map.at(key).erase(map.at(key).begin() + first, map.at(key).begin() + last);
                    }
                };

            for ( const auto& [key, type] : type_map ) {
                if ( key == "Combined Time" ) { continue; }
                if ( type == DataType::INTEGER ) { remove_data(ints, key); }
                else if ( type == DataType::DOUBLE ) { remove_data(doubles, key); }
                else if ( type == DataType::STRING ) { remove_data(strings, key); }
                else { throw std::runtime_error("DLL: <trim_data> Invalid type encountered."); }
            }

            // All columns should be same length
            // Update lengths in ints_len, doubles_len, strings_len
            uinteger len{ 0 };
            switch ( data_type ) {
            case DataType::INTEGER:
                len = ints.at(filter_key).size();
                break;
            case DataType::DOUBLE:
                len = doubles.at(filter_key).size();
                break;
            case DataType::STRING:
                len = strings.at(filter_key).size();
                break;
            case DataType::NONE:
                break;
            }

            for ( const auto& [key, type] : type_map ) {
                if ( key == "Combined Time" ) { continue; }
                switch ( type ) {
                case DataType::INTEGER:
                    if ( ints.at(key).size() != len ) {
                        throw
                            std::runtime_error(
                                std::format("DLL: <trim_data> Failed to update altered data size ({}, {}).",
                                                    key, type_string.at(type)
                                )
                            );
                    }
                    break;
                case DataType::DOUBLE:
                    if ( doubles.at(key).size() != len ) {
                        throw
                            std::runtime_error(
                                std::format("DLL: <trim_data> Failed to update altered data size ({}, {}).",
                                                    key, type_string.at(type)
                                )
                            );
                    }
                    break;
                case DataType::STRING:
                    if ( strings.at(key).size() != len ) {
                        throw
                            std::runtime_error(
                                std::format("DLL: <trim_data> Failed to update altered data size ({}, {}).",
                                                    key, type_string.at(type)
                                )
                            );
                    }
                    break;
                case DataType::NONE:
                    break;
                }
            }

            ints_len = len;
            doubles_len = len;
            strings_len = len;
            settings.set_n_rows(len);

            return true;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <trim_data>");
            return false;
        }
    }

    inline bool
    file_data::async_trim_data() noexcept {
        if ( !do_trimming_ ) { return true; }

        try {
            std::vector<std::future<bool>> futures(files_.size());

            for ( uinteger i { 0 }; i < files_.size(); ++i ) {
                if ( success_[i] ) {
                    futures[i] =
                        std::async(trim_data, std::ref(settings_[i]),
                                   std::ref(statistics_[i]), std::ref(file_ints_[i]),
                                   std::ref(ints_lens_[i]), std::ref(file_doubles_[i]),
                                   std::ref(doubles_lens_[i]), std::ref(file_strings_[i]),
                                   std::ref(strings_lens_[i]), max_off_time_);
                }
            }
            for ( uinteger i { 0 }; i < files_.size(); ++i ) {
                if ( success_[i] )
                    success_[i] = futures[i].get();
            }

            if ( !check_valid_state(success_) )
                return false;

            return true;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <file_data::async_trim_data>");
            return false;
        }
    }

    static bool
    compare_title_type_pair( const std::pair<std::string, DataType>& _a,
                             const std::pair<std::string, DataType>& _b ) noexcept {
        return _a.first == _b.first && _a.second == _b.second;
    }

    inline bool
    file_data::swap( const uinteger& _a, const uinteger& _b ) noexcept {
        try {
            std::swap(files_[_a], files_[_b]);
            std::swap(encodings_[_a], encodings_[_b]);
            std::swap(file_lines_[_a], file_lines_[_b]);
            std::swap(handles_[_a], handles_[_b]);
            std::swap(settings_[_a], settings_[_b]);
            std::swap(file_ints_[_a], file_ints_[_b]);
            std::swap(ints_lens_[_a], ints_lens_[_b]);
            std::swap(file_doubles_[_a], file_doubles_[_b]);
            std::swap(doubles_lens_[_a], doubles_lens_[_b]);
            std::swap(file_strings_[_a], file_strings_[_b]);
            std::swap(strings_lens_[_a], strings_lens_[_b]);
            const bool tmp = success_[_b];
            success_[_b] = success_[_a];
            success_[_a] = tmp;
            std::swap(statistics_[_a], statistics_[_b]);
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <file_data::swap>");
            return false;
        }

        return true;
    }

    inline bool
    file_data::erase( const uinteger& _pos ) noexcept {
        try {
            files_.erase(files_.begin() + _pos);
            encodings_.erase(encodings_.begin() + _pos);
            file_lines_.erase(file_lines_.begin() + _pos);
            handles_.erase(handles_.begin() + _pos);
            settings_.erase(settings_.begin() + _pos);
            file_ints_.erase(file_ints_.begin() + _pos);
            ints_lens_.erase(ints_lens_.begin() + _pos);
            file_doubles_.erase(file_doubles_.begin() + _pos);
            doubles_lens_.erase(doubles_lens_.begin() + _pos);
            file_strings_.erase(file_strings_.begin() + _pos);
            strings_lens_.erase(strings_lens_.begin() + _pos);
            success_.erase(success_.begin() + _pos);
            statistics_.erase(statistics_.begin() + _pos);

            return true;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <file_data::erase>");
            return false;
        }
    }

    inline bool
    file_data::erase( const uinteger& _start,
                      const uinteger& _end ) noexcept {
        try {
            files_.erase(files_.begin() + _start, files_.begin() + _end);
            encodings_.erase(encodings_.begin() + _start, encodings_.begin() + _end);
            file_lines_.erase(file_lines_.begin() + _start, file_lines_.begin() + _end);
            handles_.erase(handles_.begin() + _start, handles_.begin() + _end);
            settings_.erase(settings_.begin() + _start, settings_.begin() + _end);
            file_ints_.erase(file_ints_.begin() + _start, file_ints_.begin() + _end);
            ints_lens_.erase(ints_lens_.begin() + _start, ints_lens_.begin() + _end);
            file_doubles_.erase(file_doubles_.begin() + _start,
                                file_doubles_.begin() + _end);
            doubles_lens_.erase(doubles_lens_.begin() + _start,
                                doubles_lens_.begin() + _end);
            file_strings_.erase(file_strings_.begin() + _start,
                                file_strings_.begin() + _end);
            strings_lens_.erase(strings_lens_.begin() + _start,
                                strings_lens_.begin() + _end);
            success_.erase(success_.begin() + _start, success_.begin() + _end);
            statistics_.erase(statistics_.begin() + _start, statistics_.begin() + _end);
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL<file_data::erase>");
            return false;
        }

        return true;
    }

    inline bool
    file_data_qsrt( file_data& _f, const uinteger& _lo, const uinteger& _hi,
                    const uinteger& _depth ) noexcept {
        auto partition = []( file_data& _f, const uinteger& _lo,
                             const uinteger& _hi ) -> uinteger {
            const auto& pivot = _f.settings_[static_cast<uinteger>( floor((_lo + _hi) / 2) )]
                .get_start_time();
            integer i = _lo - 1;
            integer j = _hi + 1;
            uinteger count = 0;
            while ( true ) {
                count++;
                do { i += 1; }
                while ( _f.settings_[i].get_start_time() < pivot );

                do { j -= 1; }
                while ( _f.settings_[j].get_start_time() > pivot );

                if ( i >= j ) { return j; }

                if ( !_f.swap(i, j) )
                    return _f.files_.size();
            }
        };
        try {
            if ( _f.files_.empty() )
                return false;
            if ( _f.files_.size() == 1 || _hi == _lo )
                return true;

            if ( _lo >= 0 && _hi >= 0 && _lo < _hi ) {
                uinteger diff = _hi - _lo;
                if ( _f.files_.size() == 2 || (_hi - _lo) == 1 ) {
                    if ( _f.settings_[_lo].get_start_time() > _f.settings_[_hi].get_start_time() )
                        _f.swap(_lo, _hi);
                    return true;
                }

                uinteger p = partition(_f, _lo, _hi);
                if ( p == _f.files_.size() ) {
                    write_err_log(std::runtime_error("DLL: <file_data_qsrt> Invalid partition index."));
                    return false;
                }
                if ( !file_data_qsrt(_f, _lo, p, _depth + 1) ||
                     !file_data_qsrt(_f, p + 1, _hi, _depth + 1) ) {
                    write_err_log(std::runtime_error(std::format("DLL: <file_data_qsrt> Failed recursive sort, depth = {}",
                                                                 _depth)));
                    return false;
                }
            }
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <file_data_qsrt");
            return false;
        }
        return true;
    }

    inline bool
    file_data::clear_failed_loads() noexcept {
        try {
            std::vector<uinteger> indexes_to_erase;
            for ( uinteger i = 0; i < success_.size();
                  ++i ) { // Find all indexes to erase
                if ( !success_[i] )
                    indexes_to_erase.push_back(i);
            }
            // Iterate indexes_to_erase in reverse, removing vals
            // Prevents indexes being altered by the removal of other values
            // Also takes advantage of std::vector<>::erase to be more efficient
            if ( !indexes_to_erase.empty() ) {
                integer pos = indexes_to_erase.size() - 1;
                while ( pos >= 0 ) {
                    erase(indexes_to_erase[pos]);
                    pos--;
                }
            }
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <clear_failed_loads>");
            return false;
        }

        return true;
    }

    // Quick & easy alteration of qsort to work with file_data
    inline bool
    file_data::sort_files() noexcept {
        try {
            // Sort:
            if ( !file_data_qsrt(*this, 0, files_.size() - 1, 0) ) {
                write_err_log(std::runtime_error("DLL: <file_data::sort_files> file_data_qsrt failed."));
                return false;
            }
            return true;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <file_data::sort_files>");
            return false;
        }
    }

    /*
    * Combines (key, type) maps of each file together.
    * Also ensures that all keys are unique in the resulting
    * combined (key, type) map
    */
    inline bool
    combine_cols( const std::vector<file_settings>& _settings,
                  TypeMap& _col_types ) noexcept {
        try {
            // for each (i, settings).
            std::unordered_map<std::string, integer> repeated_keys;
            for ( const auto& settings : _settings ) {
                // For each (key, type) pair.
                for ( auto& [key, type] : settings.get_col_types() ) {
                    if ( !is_in(key, _col_types) ) {
                        // key not in col_types_ yet, add it.
                        _col_types[key] = type;
                    }
                }
            }
            return true;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <combine_cols>");
            return false;
        }
    }

    // TODO:
    inline bool
    combine_settings( const std::vector<file_settings>& _settings ) { return true; }

    inline bool
    combine_stats( const TypeMap& _type_map,
                   const std::vector<file_stats>& _source,
                   file_stats& _dest ) {
        try {
            // max_ints, max_doubles, min_ints, min_doubles, means, medians, stdevs,
            // _n
            // Add all titles to _dest
            for ( const auto& [key, type] : _type_map ) {
                _dest.max_doubles[key] = 0;
                _dest.max_ints[key] = 0;
                _dest.min_doubles[key] = std::numeric_limits<double>::max();
                _dest.min_ints[key] = std::numeric_limits<integer>::max();
                _dest._n[key] = {};
            }

            write_log("");

            for ( const auto& [key, type] : _type_map ) {

                integer max_int = std::numeric_limits<integer>::lowest(), min_int = std::numeric_limits<integer>::max();
                double max_double = std::numeric_limits<double>::lowest(), min_double = std::numeric_limits<double>::max();

                int i{ 0 };
                for ( const auto& stats : _source ) {
                    try {
                        switch ( type ) {
                        case DataType::INTEGER:
                            max_int = MAX(max_int, stats.max_ints.at( key ));
                            min_int = MIN(min_int, stats.min_ints.at( key ));
                            break;
                        case DataType::DOUBLE:
                            max_double = MAX(max_double, stats.max_doubles.at( key ));
                            min_double = MIN(min_double, stats.min_doubles.at( key ));
                            break;
                        }

                        _dest._n[key].insert(_dest._n[key].end(), stats._n.at(key).begin(),
                                         stats._n.at(key).end());
                    }
                    catch ( const std::exception& err ) { write_err_log(err, "DLL: <combine_stats>"); }
                }
                _dest.max_ints[key] = max_int;
                _dest.min_ints[key] = min_int;
                _dest.max_doubles[key] = max_double;
                _dest.min_doubles[key] = min_double;

            }
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <combine_stats>");
            std::cerr << "ERROR: <combine_stats> " << err.what() << std::endl;
            return false;
        }
        return true;
    }

    inline bool
    file_data::async_combine_data() noexcept {
        try {
            // Empty any previously combined data
            ints_.clear();
            doubles_.clear();
            strings_.clear();
            internal_time_.clear();
            file_boundaries_.clear();

            // Remove all files which failed to load:
            if ( !clear_failed_loads() ) {
                write_err_log(std::runtime_error("DLL: <file_data::async_combine_data> \"clear_failed_loads\" failed."));
                return false;
            }
            // Presort all entries here:
            if ( !sort_files() ) {
                write_err_log(std::runtime_error("DLL: <file_data::async_combine_data> \"sort_files\" failed."));
                return false;
            }
            // get all unique (ColTitle + ColType) pairs
            if ( !combine_cols(settings_, this->col_types()) ) {
                write_err_log(std::runtime_error("DLL: <file_data::async_combine_data> \"combine_cols\" failed."));
                return false;
            }

            // Add empty entries to ints_, doubles_, strings_
            for ( const auto& [key, type] : this->col_types() ) {
                write_log(std::format(" - {}: {}", key, type_string.at(type)));
                switch ( type ) {
                case DataType::INTEGER:
                    ints_[key] = {};
                    break;
                case DataType::DOUBLE:
                    doubles_[key] = {};
                    break;
                default: {
                    if ( type == DataType::STRING || type == DataType::NONE ) { strings_[key] = {}; }
                    else {
                        write_err_log(std::runtime_error("DLL: <file_data::async_combine_data> NONE type data encountered"));
                    }
                }
                }
            }

            if ( !combine_stats(this->get_col_types(), statistics_, *this) ) {
                write_err_log(std::runtime_error("DLL: <file_data::async_combine_data> \"combine_stats\" failed."));
                return false;
            }

            if ( !combine_settings(settings_) ) {
                write_err_log(std::runtime_error("DLL: <file_data::async_combine_data> \"combine_settings\" failed."));
                return false;
            }

            /*
            * Count total number of rows so memory can be
            * pre-reserved, avoiding reallocations.
            */
            uinteger n_rows =
                std::accumulate(settings_.cbegin(), settings_.cend(),
                                static_cast<uinteger>( 0 ),
                                []( uinteger lhs, const file_settings& rhs ) -> uinteger {
                                    return lhs += rhs.get_n_rows();
                                });

            /*
            * Create internal time representation as if all
            * the data is continuous.
            * Register boundaries between files
            */
            internal_time_.reserve(n_rows);
            internal_time_.resize(n_rows);
            uinteger pos { 0 };
            auto current_time { nano::zero() };
            for ( const auto& [i, settings] : enumerate(settings_) ) {
                /*
                * Add record of file boundary
                */
                file_boundaries_[i] = std::pair { current_time, settings.get_start_time() };
                /*
                * It isn't guaranteed that all the files have the same
                * measurement interval, so we need to check for that.
                */
                const nano& interval = settings.get_measurement_period();
                const uinteger& n = settings.get_n_rows();
                for ( uinteger j { 0 }; j < n; ++j ) {
                    internal_time_[pos++] = current_time;
                    current_time += interval;
                }
            }
            auto& cols{ col_types() };
            cols["Combined Time"] = DataType::DOUBLE;
            doubles_["Combined Time"] = get_internal_time_double();
            assert(pos == n_rows);

            /*
            * Iterate through (col, type) pairs
            * Lookup in each _file_(ints/doubles/strings)
            * If exists --> Concatenate
            * If not --> Concatenate array of 0/0.0/("NULL"/"")
            */
            const auto concat_vals =
                [this]<typename T>(
                const TMap<T>& _lookup_loc, TMap<T>& _storage_loc,
                const std::string& key, const DataType& type,
                const uinteger& _len, const file_stats& _stats,
                const T& _default_fill = 0 ) {
                    const auto iter = _lookup_loc.find(key);
                    if ( iter != _lookup_loc.end() ) {
                        // Insert separate data to end of combined storage
                        _storage_loc[key].insert(
                            _storage_loc[key].end(),
                            _lookup_loc.at(key).begin(), _lookup_loc.at(key).end()
                        );

                        try {
                            // Update value of max / min in (max/min)_[typename]
                            switch ( type ) {
                            case DataType::INTEGER:
                                max_ints[key] = MAX(max_ints.at( key ), _stats.max_ints.at( key ));
                                min_ints[key] = MIN(min_ints.at( key ), _stats.min_ints.at( key ));
                                break;
                            case DataType::DOUBLE:
                                max_doubles[key] =
                                    MAX(max_doubles.at( key ), _stats.max_doubles.at( key ));
                                min_doubles[key] =
                                    MIN(min_doubles.at( key ), _stats.min_doubles.at( key ));
                                break;
                            case DataType::STRING:
                                break;
                            case DataType::NONE:
                                write_err_log(std::runtime_error("DLL: <file_data::async_combine_data> Invalid type received."));
                                return false;
                            }
                        }
                        catch ( const std::exception& err ) {
                            write_err_log(err, "DLL: <file_data::async_combine_data::ConcatVals>");
                            return false;
                        }
                        return true;
                    }
                    // Key didn't exist insert _len default values as placeholder
                    _storage_loc[key].insert(
                                             _storage_loc[key].end(),
                                             _len,
                                             _default_fill
                                            );
                    return true;
                };
            /*
            * For all (title, type) pairs in ALL files.
            * It is NOT guaranteed that all files have the same title type pairs.
            * It IS guaranteed that all titles will be unique.
            */
            for ( const auto& [title, type] : this->col_types() ) {
                // Skip 
                if ( title == "Combined Time" ) {
                    continue;
                }

                for ( uinteger j = 0; j < files_.size(); ++j ) {
                    switch ( type ) {
                    case DataType::INTEGER:
                        concat_vals(file_ints_[j], ints_, title, type, ints_lens_[j],
                                   statistics_[j], 0);
                        break;
                    case DataType::DOUBLE:
                        concat_vals(file_doubles_[j], doubles_, title, type, doubles_lens_[j],
                                   statistics_[j], 0.);
                        break;
                    case DataType::STRING:
                        concat_vals(file_strings_[j], strings_, title, type, strings_lens_[j],
                                   statistics_[j], std::string(""));
                        break;
                    case DataType::NONE:
                        write_err_log(std::runtime_error("DLL: <file_data::async_combine_data> Invalid type received."));
                        return false;
                    }
                }
            }

            set_n_cols(col_types().size());
            return true;
        }
        catch ( const std::exception& err ) {
            write_err_log(err, "DLL: <file_data::async_combine_data>");
            return false;
        }
    }
} // namespace burn_in_data_report
