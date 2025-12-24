#pragma once

#include <string>

class FileReader {
public:
    static std::string readFile(const std::string& filePath);
};