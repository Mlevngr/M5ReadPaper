#include "file_reader.h"
#include <fstream>
#include <sstream>

std::string FileReader::readFile(const std::string& filePath) {
    std::ifstream file(filePath);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}