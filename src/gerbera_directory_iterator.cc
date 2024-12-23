/*GRB*

    Gerbera - https://gerbera.io/

    gerbera_directory_iterator.cc - this file is part of Gerbera.

    Copyright (C) 2024 Gerbera Contributors

    Gerbera is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2
    as published by the Free Software Foundation.

    Gerbera is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Gerbera.  If not, see <http://www.gnu.org/licenses/>.

    $Id$
*/

/// \file gerbera_directory_iterator.cc

#include "gerbera_directory_iterator.h"
#include <regex>

std::vector<std::filesystem::directory_entry>::iterator begin(gerbera_directory_iterator& it)
{
    return it.begin();
}

std::vector<std::filesystem::directory_entry>::iterator end(gerbera_directory_iterator& it)
{
    return it.end();
}

gerbera_directory_iterator::gerbera_directory_iterator(const std::filesystem::directory_entry& de, std::error_code& ec)
{
    try {
        for (const auto& entry : std::filesystem::directory_iterator(de, ec))
            sortedContents.push_back(entry);
    } catch (...) {
    }

    process_sort();
}

gerbera_directory_iterator::gerbera_directory_iterator(const std::string& path, std::error_code& ec)
{
    try {
        for (const auto& entry : std::filesystem::directory_iterator(path, ec))
            sortedContents.push_back(entry);
    } catch (...) {
    }

    process_sort();
}

void gerbera_directory_iterator::process_sort()
{
    if (sortedContents.empty())
        return;

    namespace fs = std::filesystem;
    typedef fs::directory_entry DirEntry;

    // standard sorting
    std::sort(sortedContents.begin(), sortedContents.end(), [](const DirEntry& lhs, const DirEntry& rhs) {
        return fs::path(lhs.path()).filename() < fs::path(rhs.path()).filename();
    });

#ifdef HAVE_HUMAN_SORTING
    // custom sorting

    std::regex stemRe("^([0-9]*)[^0-9]*([0-9]*)$");

    typedef std::vector<fs::directory_entry> DirVec;
    DirVec sortedFolders, sortedFiles;

    // folders must be first in order
    for (const DirEntry& item : sortedContents) {
        if (item.is_directory())
            sortedFolders.push_back(item);
        else
            sortedFiles.push_back(item);
    }
    sortedContents.clear();

    // sorting both folders and files
    for (DirVec* vec : std::vector<DirVec*> { &sortedFolders, &sortedFiles }) {
        if (vec->empty())
            continue;

        std::map<int, DirEntry> frontMap;
        std::map<int, DirEntry> backMap;

        for (size_t i = 0; i < vec->size();) {
            std::smatch stemMatch;
            fs::path filePath = vec->at(i).path();
            std::string fileStem = filePath.stem();

            if (std::regex_match(fileStem, stemMatch, stemRe)) {
                std::string frontStr = stemMatch[1].str();
                std::string backStr = stemMatch[2].str();

                if (!frontStr.empty())
                    frontMap.insert({ std::stoi(frontStr), vec->at(i) });

                if (!backStr.empty())
                    backMap.insert({ std::stoi(backStr), vec->at(i) });
            }

            i++;

            // in current version, if some of entries don't match regex, falling back to standard sort
            if (frontMap.size() < i && backMap.size() < i)
                break;
        }

        if (frontMap.size() == vec->size()) // all entries have integer prefix
        {
            for (auto it = frontMap.cbegin(); it != frontMap.cend(); ++it)
                sortedContents.push_back(it->second);
        } else {
            if (backMap.size() == vec->size()) // all entries have integer suffix
            {
                for (auto it = backMap.cbegin(); it != backMap.cend(); ++it)
                    sortedContents.push_back(it->second);
            } else {
                for (const DirEntry& item : *vec) // falling back to standard sort
                    sortedContents.push_back(item);
            }
        }
    }
#endif

    for (size_t id = 0; id < sortedContents.size(); ++id)
        sortedContentsMap.insert({ sortedContents.at(id), id });
}

size_t gerbera_directory_iterator::distance(const std::filesystem::directory_entry& path) const
{
    const auto it = sortedContentsMap.find(path);
    if (it != sortedContentsMap.cend())
        return it->second;

    return 0;
}
