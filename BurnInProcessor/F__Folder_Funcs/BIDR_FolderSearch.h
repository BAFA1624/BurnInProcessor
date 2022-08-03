#pragma once

#include "../BIDR_Defines.h"

#include <filesystem>
#include <map>

namespace burn_in_data_report
{
    inline std::vector<std::filesystem::directory_entry>
    explore_path( const std::filesystem::path& _path ) {
        std::vector<std::filesystem::directory_entry> result(0);
        std::error_code                               ec;
        std::filesystem::directory_iterator           iter(_path, ec);

        if ( ec ) { return result; }

        for ( const auto& file : iter ) { result.push_back(file); }
        return result;
    }

    inline std::map<uinteger, std::filesystem::directory_entry>
    recursive_explore_path( const std::filesystem::path& _f_path ) {
        std::map<uinteger, std::filesystem::directory_entry> idx_dir_map {};

        const auto is_valid = []( const std::filesystem::directory_entry& x ) -> bool {
            return !(x.is_block_file() || x.is_character_file()
                     || x.is_directory() || x.is_fifo()
                     || x.is_socket() || x.is_symlink());
        };

        std::vector<std::filesystem::directory_entry> valid_files;
        const auto rec_iter = std::filesystem::recursive_directory_iterator { _f_path };
        uinteger count { 0 };
        for ( const auto& dir_entry : rec_iter ) { if ( is_valid(dir_entry) ) { idx_dir_map[count++] = dir_entry; } }

        return idx_dir_map;
    }

    inline std::string
    short_path( const std::filesystem::path& root,
                const std::filesystem::path& path ) {
        const uinteger root_len{ static_cast<uinteger>(root.string().size()) };
        const uinteger path_len{ static_cast<uinteger>(path.string().size()) };
        assert(root_len < path_len);
        return path.string().substr(root_len, path_len);
    }
}
