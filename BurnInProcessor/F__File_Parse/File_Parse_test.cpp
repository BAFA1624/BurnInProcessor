#define JSON_DIAGNOSTICS 1
#ifdef _DEBUG
#define DEBUG 4
#endif

#include "../S__Spreadsheet_Classes/BIDR_Spreadsheet.h"

#include <random>
#include <filesystem>
#include <string>
#include <iostream>

#include <windows.h>
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

using namespace burn_in_data_report;

std::vector<uint64_t> rand_vec( const uint64_t& _n,
                                const uint64_t& _low = 0,
                                const uint64_t& _range = 100 ) {
    std::vector<uint64_t> vec( _n );
    for ( uint64_t i = 0; i < _n; ++i )
        vec[i] = rand() % _range + _low;
    return vec;
}

bool WriteCSV(
    const DMap& data,
    const std::string& _filepath ) {
    try {
        if ( data.empty() )
            return false;

        const auto titles = get_keys(data);
        const auto n_lines = data.at(titles[0]).size() + 1;

        std::string content{ "" };

        // Write titles:
        for ( uint64_t i{ 0 }; i < titles.size() - 2; ++i ) {
            content += (titles[i] + ",");
        }
        content += (titles.back() + "\n");

        // Write data:
        for ( uint64_t i{ 0 }; i < n_lines - 2; ++i ) {
            std::string new_line{ "" };
            for ( const auto& [key, vec] : data ) {
                new_line.append(std::to_string(vec[i]) + ",");
            }
            assert(new_line.ends_with(','));
            new_line.pop_back();
            content.append(new_line + "\n");
        }

        // Open file
        std::fstream fp;
        fp.open( _filepath, std::ios_base::out | std::ios_base::trunc );
        assert(fp.is_open());

        // Write to file
        fp << content;

        // close file
        fp.close();

        return true;
    }
    catch ( const std::exception& err ) {
        std::cerr << "ERROR: <WriteCSV> " << err.what() << std::endl;
        return false;
    }
}

std::unordered_map<std::string, std::vector<std::string>>
read_csv(const std::string& file_path, const bool header) {
	const std::filesystem::directory_entry file( file_path );
	if ( file.exists() ) {
		std::fstream stream;
		stream.open( file_path, std::ios::in | std::ios::binary );

		if ( !stream.is_open() ) return {};

		std::filebuf* fbuf = stream.rdbuf();
		auto buf = new char[file.file_size()];
		fbuf->sgetn( buf, static_cast<long long>(file.file_size()) );
		buf[file.file_size() - 1] = '\0';

		std::unordered_map<std::string, std::vector<std::string>> result;
		auto lines = split_str( std::string_view{ buf, file.file_size() }, std::string_view{ "\n" });
		if ( header ) {
			auto headers = split_str( lines[0], std::string_view{ "," });
			std::vector<std::vector<std::string>> data( headers.size() );
			uint64_t count{ 0 };
			for ( uint64_t i{1}; i < lines.size(); ++i ) {
                const auto& line = lines[i];
				auto values = split_str( line, std::string_view{ "," } );
				for ( uint64_t i{ 0 }; i < data.size(); ++i ) {
					data[i].emplace_back( std::string{ values[i] });
				}
			}
			
			for ( uint64_t i{ 0 }; i < data.size(); ++i ) {
				result[std::string{ headers[i] }] = data[i];
			}

			return result;
		}
		else {
			std::vector<std::vector<std::string>> data( lines[0].size() );
			for ( const auto& line : lines ) {
				std::cout << std::string{ line.data(), line.size() } << std::endl;
				auto values = split_str( line, std::string_view{ "," } );
				for ( uint64_t i{ 0 }; i < data.size(); ++i ) {
					data[i].emplace_back( std::string{ values[i] } );
				}
			}
			
			for ( uint64_t i{ 0 }; i < data.size(); ++i ) {
				result[std::to_string(i)] = data[i];
			}
			return result;
		}
	}
	else {
		std::cerr << "Failed to open csv: " + file_path << std::endl;
		exit( -1 );
	}
}

std::string verify( const double& input,
                    const std::string& output,
                    const std::string& answer) {
	return (output == answer) ?
		std::format( "[Success] {} -> {}.", input, answer ) :
		std::format( "[Failure] {} -> {}, output: {}.", input, answer, output );
}

template <typename T>
std::vector<std::pair<uint64_t, uint64_t>>
ExtractRanges( const std::vector<T>&_data,
               const T & _threshold,
               const uint64_t & _n = 1,
               const uint64_t & _max_range_sz = 0 ) {
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
    std::vector<std::pair<uint64_t, uint64_t>> results;
    results.reserve( _data.size() / 10 );
    bool continuous_range{ false };
    uint64_t r_start{ 0 }, r_end{ 0 }, count{ 0 };
    for ( uint64_t i = 0; i < _data.size(); ++i ) {
        uint64_t branch{ 0 };
        branch += ((_data[i] >= _threshold) ? 2 : 1);
        branch += ((continuous_range) ? 1 : -1);

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
                const auto subranges = sub_range_split( _data, r_start, r_end, _max_range_sz );

                results.insert( results.cend(), subranges.cbegin(), subranges.cend() );

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
        }
    }
    if ( continuous_range && r_start > r_end ) {
        /*
        * Handle case where a valid range reaches to the end of the data
        * without transitioning back below the threshold.
        */
        r_end = _data.size() - 1;
        const auto subranges = sub_range_split( _data, r_start, r_end, _max_range_sz );
        results.insert( results.cend(), subranges.cbegin(), subranges.cend() );
    }
    results.shrink_to_fit();
    return results;
};

using range = std::pair<uint64_t, uint64_t>;
template <typename T> [[nodiscard]] DMap
Reduce( const TMap<T>& data, const std::vector<range>& filter = {} ) {
    try {
        DMap means, stdevs, n;
        for ( const auto& [key, vec] : data ) {
            if ( means.find( key ) != means.end() ) {
                std::runtime_error err("Duplicate key provided.");
                throw err;
            }

            means[key] = {};
            stdevs[key] = {};
            n[key] = {};
            means[key].reserve(filter.size());
            stdevs[key].reserve(filter.size());
            n[key].reserve(filter.size());

            for ( const auto& [first, last] : filter ) {
                const auto& [n_first, n_last] = stable_period_convert( first, last );
                const auto size = static_cast< double >(n_last - n_first);
                const double _mean = mean( vec, n_first, n_last );
                const double _stdev = stdev( vec, n_first, n_last, _mean, std::vector<double>{}, 0 );
                means[key].emplace_back(_mean);
                stdevs[key].emplace_back(_stdev);
                n[key].emplace_back(size);
            }

            means[key].shrink_to_fit();
            stdevs[key].shrink_to_fit();
            n[key].shrink_to_fit();
        }
        DMap result;
        for ( const auto& [key, vec] : means ) {
            result[key] = vec;
        }
        for ( const auto& [key, vec] : stdevs ) {
            result[key + "_std"] = vec;
        }
        for ( const auto& [key, vec] : n ) {
            result[key + "_n"] = vec;
        }
        return result;
    }
    catch ( const std::exception& err ) {
        std::cerr << "ERROR: <Reduce> " << err.what() << std::endl;
        return {};
    }
};

template <typename T1, typename T2> [[nodiscard]] DMap
Reduce( const TMap<T1>& data1,
        const TMap<T2>& data2,
        const std::vector<range>& filter = {} ) {
    DMap results = Reduce(data1, filter);
    for ( const auto& [key, vec] : Reduce( data2, filter ) ) {
        results[key] = vec;
    }
    return results;
}

int main() {
    try {

        /*
        auto csv_data = read_csv("C:\\Users\\AndrewsBe\\Documents\\Data Logging Tool Development\\test_samples\\time_conversion_data.csv", true);
        auto raw_inputs = csv_data.at( "inputs" );
        auto expected_outputs = csv_data.at( "expected_outputs" );

        std::vector<std::chrono::system_clock::time_point> raw_results;
        for ( const auto& d : raw_inputs) {
            raw_results.push_back( datetime_to_time_point( std::stod(d) ) );
        }
        std::vector<std::string> results;
        for (const auto& et : raw_results) {
            results.push_back(std::format("{:%d/%m/%Y %H:%M:%S}", std::chrono::floor<std::chrono::seconds>(et)));
        }

        for ( uint64_t i{ 0 }; i < results.size(); ++i ) {
            std::cout << std::format("{}\n", verify(std::stod(raw_inputs[i]), results[i], expected_outputs[i]));
        }*/
        
        _CrtMemState sOld{}, sNew{}, sDiff{};
        _CrtMemCheckpoint(&sOld);


        std::string user_input{ "" };
        std::filesystem::directory_entry default_config_path{ std::filesystem::current_path().string() + "\\Test Configs" };

        if ( !default_config_path.exists() ) {
            std::cout << std::format("Default config path ({}) does not exist.\n", default_config_path.path().string());
            std::cout << std::format("Please select a new one:\n");
            
            while ( !default_config_path.exists() ) {
                std::getline(std::cin, user_input);
                default_config_path = std::filesystem::directory_entry{ user_input };
            }
        }

        std::cout << "Please enter path to files:" << std::endl;
        user_input = "";
        std::filesystem::directory_entry dir{ user_input };
        std::map<uint64_t, std::filesystem::directory_entry> files;
        while ( !dir.is_directory() ) {
            try {
                std::getline(std::cin, user_input);
                dir = std::filesystem::directory_entry{ user_input };
                files = recursive_explore_path( dir );
            }
            catch (const std::exception& err) { 
                user_input = "";
                std::cout << err.what() << std::endl;
            }
        }
       
        for ( const auto& [i, file] : files ) {
            std::cout << std::format("{}: {}\n", i, short_path( dir, file ));
        }

        std::vector<std::filesystem::directory_entry>
            files_to_load;
        
        std::cout << "Please select which files to use (e.g: 0 1 4 7):" << std::endl;
        do {
            std::getline(std::cin, user_input);
            //std::cin >> std::ws;

            auto values = split_str(user_input, std::string_view{ " \t," });

            for ( const auto& val : values ) {
                try {
                    uint64_t idx = std::stoull( std::string{ val } );
                    files_to_load.push_back(files.at(idx));
                }
                catch ( const std::exception& err ) {
                    user_input = "";
                    std::cerr << err.what() << std::endl;
                }
            }
        }
        while ( files_to_load.empty() );

        std::cout << "Max off time to detect test failure? (minutes)\n";
        user_input = "";
        nano max_off_time{ nano::zero() };
        do {
            try {
                std::getline(std::cin, user_input);
                //std::cin >> std::ws;

                using mins = std::chrono::duration<double, std::chrono::minutes::period>;
                double minutes = std::stod(user_input);

                max_off_time =
                    std::chrono::duration_cast<nano>( mins{minutes} );
            }
            catch ( const std::exception& err ) {
                max_off_time = nano::zero();
                user_input = "";
                std::cerr << err.what() << std::endl;
            }
        }
        while ( max_off_time == nano::zero() );

        std::cout << "Apply trimming to detected test failure? [y, yes, n, no]\n";
        user_input = "";
        bool apply_trimming{ true }, success{ false };
        do {
            try {
                std::getline(std::cin, user_input);
                if ( is_in( tolower( user_input ), {"y", "yes", "n", "no"}) ) {
                    if ( is_in( tolower( user_input ), { "y", "yes" } ) ) {
                        apply_trimming = true;
                    }
                    else {
                        apply_trimming = false;
                    }
                    success = true;
                }
                else {
                    success = false;
                    user_input = "";
                }
            }
            catch ( const std::exception& err ) {
                success = false;
                user_input = "";
                std::cerr << err.what() << std::endl;
            }
        }
        while ( !success );

        success = false;
        std::string err_str{ "" };
        file_data f_data;
        try {
            f_data = file_data{ files_to_load, default_config_path, 256, max_off_time, apply_trimming };
            success = true;
        }
        catch ( const std::exception& err ) {
            err_str = std::format("Failed to create file_data: {}\n", std::string{ err.what() });
            success = false;
        }

        if ( !success ) {
            throw std::runtime_error(err_str);
        }

        std::cout << std::endl;
        std::cout << "Files loaded: \n";
        std::function<void(const std::filesystem::directory_entry&)> lambda =
            [](const std::filesystem::directory_entry& x) { std::cout << x.path().string(); };
        print_vec(f_data.files(), lambda, "\n");
        std::cout << std::endl;

        const auto& keys = f_data.get_columns();
        const auto& key_types = f_data.get_col_types();

        std::cout << std::format("Select filter key:\n");
        std::map<uint64_t, std::string> filter_keys;
        uint64_t count{ 1 };
        for ( const auto& [key, type] : key_types ) {
            if ( type == DataType::INTEGER || type == DataType::DOUBLE ) {
                std::cout << std::format("{}: {}\n", count, key);
                filter_keys[count++] = key;
            }
        }

        std::string filter_key{ "" };
        user_input = "";
        count = 0;
        do {
            try {
                std::getline(std::cin, user_input);
                //std::cin >> std::ws;
                count = std::stoull(user_input);
                filter_key = filter_keys.at(count);
            }
            catch ( const std::exception& err ) {
                count = 0;
                user_input = "";
                std::cerr << err.what() << std::endl;
            }
        }
        while ( filter_keys.count(count) == 0 );

        std::cout << "Provide value (0 <= x <= 1) to calculate filter threshold:\n";
        user_input = "";
        double fraction = -1.0;
        do {
            try {
                std::getline(std::cin, user_input);
                //std::cin >> std::ws;
                fraction = std::stod(user_input);
            }
            catch ( const std::exception& err ) {
                fraction = -1.0;
                user_input = "";
                std::cerr << err.what() << std::endl;
            }
        }
        while ( fraction < 0 || fraction > 1 );

        std::cout << "Select no. values for filter transition:\n";
        user_input = "";
        uint64_t _n{ 0 };
        do {
            try {
                std::getline(std::cin, user_input);
                //std::cin >> std::ws;
                _n = std::stoull(user_input);
            }
            catch ( const std::exception& err ) {
                _n = 0;
                user_input = "";
                std::cerr << err.what() << std::endl;
            }
        }
        while ( _n == 0 );

        std::cout << "Select max. range size for filter:\n";
        user_input = "";
        uint64_t max_range_sz{ 0 };
        do {
            try {
                std::getline(std::cin, user_input);
                //std::cin >> std::ws;
                max_range_sz = std::stoull(user_input);
            }
            catch ( const std::exception& err ) {
                max_range_sz = 0;
                user_input = "";
                std::cerr << err.what() << std::endl;
            }
        }
        while ( max_range_sz == 0 );

        std::vector<std::pair<uint64_t, uint64_t>> filter;
        switch ( key_types.at( filter_key ) ) {
        case DataType::INTEGER:
        {
            const int64_t max_val = f_data.max_ints.at( filter_key );
            filter = ExtractRanges( f_data.get_i( filter_key ),
                                    static_cast<int64_t>(max_val * fraction),
                                    _n, max_range_sz);
            break;
        }
        case DataType::DOUBLE:
        {
            const double max_val = f_data.max_doubles.at( filter_key );
            filter = ExtractRanges( f_data.get_d( filter_key ),
                                    max_val * fraction, _n,
                                    max_range_sz );
            break;
        }
        default:
            break;
        }

        std::cout << "This program currently only works with integer and double data.\n";
        std::cout << "Please select data to write to .csv, available keys: (e.g 1 2 3 4 5)\n";
        for ( const auto& [i, key] : filter_keys ) {
            std::cout << std::format("{}: {}\n", i, key);
        }

        user_input = "";
        std::unordered_map<std::string, DataType> data_keys{};
        do {
            try {
                std::getline(std::cin, user_input);

                auto vals = split_str(user_input, std::string_view{" ,\t"});

                for ( const auto& val : vals ) {
                    int idx = std::stoi(std::string{ val });
                    data_keys[filter_keys.at(idx)] = key_types.at(filter_keys.at(idx));
                }

                break;
            }
            catch ( std::exception& err ) {
                user_input = "";
                std::cerr << err.what() << std::endl;
            }
        }
        while ( true );

        IMap int_data;
        DMap double_data;
        double_data["internal time"] = f_data.get_internal_time_double();
        const auto& x = double_data.at("internal time");
        for ( const auto& [key, type] : data_keys ) {
            switch ( type ) {
            case DataType::INTEGER:
                int_data[key] = f_data.get_i(key);
                break;
            case DataType::DOUBLE:
                double_data[key] = f_data.get_d(key);
            }
        }

        DMap data = Reduce( int_data, double_data, filter );

        std::cout << "Please provide a name for the output:" << std::endl;
        user_input = "";
        std::vector<std::string_view> vals(0);
        std::filesystem::directory_entry file, parent;
        do {
            try {
                std::getline(std::cin, user_input);
                vals = split_str(user_input, std::string_view{ ".<>\"/|?*" });
                file = std::filesystem::directory_entry{ std::string{ vals[0] } + ".csv"};
                parent = std::filesystem::directory_entry{ file.path().parent_path() };
            }
            catch ( const std::exception& err ) {
                std::cerr << err.what() << std::endl;
            }
        }
        while ( vals.size() != 1
                && !file.exists()
                && parent.exists());

        user_input += ".csv";
        std::cout << std::format("Saving: {}\n", user_input);

        WriteCSV( data, user_input);

        std::cout << "End Program..." << std::endl;
        char c;
        std::cin >> c;
        
        
        _CrtMemCheckpoint(&sNew);
        if (_CrtMemDifference(&sDiff, &sOld, &sNew))
        {
            OutputDebugString("-----_CrtMemDumpStatistics-----\n");
            _CrtMemDumpStatistics(&sDiff);
            OutputDebugString("-----_CrtMemDumpAllObjectsSince-----\n");
            _CrtMemDumpAllObjectsSince(&sOld);
            OutputDebugString("-----_CrtDumpMemoryLeaks-----\n");
            _CrtDumpMemoryLeaks();
        }
        else
        {
            OutputDebugString("-----No Leaks Found-----\n");
        }
    }
    catch ( const std::exception& err ) {
        char c;
        std::cerr << "ERROR: <main> " << err.what() << std::endl;
        std::cin >> c;
        return -1;
    }
}