#pragma once

#include <iostream> // Remove after testing
#include <fstream>
#include <thread>

#include "../S__Datastructures/BIDR_FileTree.h"

/*----------------------------------------------------------------

Functions TODO:
    - std::vector<std::fstream> --> std::vector<data>: Combine contents of multiple files according to custom function.
    - Write to file, either through default mechanic or through specified custom function.

*/

namespace BurnInDataReport {
    // TODO: Need to finish implementing some folder search stuff to test this
    void FUNC_TO_HANDLE_FILES( FileData& file ) {}

    // Need to revise return type
    void ConcatenateFiles( std::vector<FileData>& files, std::vector<std::string>& properties ) {
        std::vector<double> conc_data;
    }
    /*template <typename T>
    void ConcatenateFiles(FileTree<double>& folderStructure)
    {
    }*/
}