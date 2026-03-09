#ifndef CSVUTILS_HPP
#define CSVUTILS_HPP

#include <string>
#include <vector>
namespace CSVUtil {
    bool parseCsvLine(const std::string& line, std::vector<std::string>& out);
    bool tryOpenCsv(const std::string& path, std::ifstream& stream);
}

#endif