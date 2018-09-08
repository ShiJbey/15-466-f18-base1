#pragma once

#include "data_path.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <fstream>



// This header provide codes that assists in reading in
// levels from *.map files
namespace TiltEscape
{

    // Returns 2D array of char from file
    std::vector<char[]> parse_level(std::string filename)
    {
        std::vector<char[]> rows;

        std::ifstream mapfile(data_path(filename));
        if (!mapfile.is_open())
        {
            std::cerr << "failed to open " << filename << '\n';
        }
        else
        {
            
            std::string line;
            while (std::getline(mapfile, line))
            {
                char row[line.length];
                std::size_t c = line.copy(row, line.length, 0);
                if (c < line.length) {
                    std::cerr << "Could not copy entire row of level" << std::endl;
                }
                rows.push_back(row);
            }
        }

        return rows;
    }

};

