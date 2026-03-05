#include "utils/CSVUtil.hpp"

#include <fstream>
namespace CSVUtil {
    bool parseCsvLine(const std::string& line, std::vector<std::string>& out) {
        out.clear();
        std::string field;
        bool inQuotes = false;
        for (std::size_t i = 0; i < line.size(); ++i) {
            char c = line[i];
            if (c == '"') {
                if (inQuotes && i + 1 < line.size() && line[i + 1] == '"') {
                    field.push_back('"');
                    ++i;
                } else {
                    inQuotes = !inQuotes;
                }
                continue;
            }
            if (c == ',' && !inQuotes) {
                out.push_back(field);
                field.clear();
                continue;
            }
            field.push_back(c);
        }
        out.push_back(field);
        return true;
    }

    bool tryOpenCsv(const std::string& path, std::ifstream& stream) {
        stream.open(path);
        return stream.is_open();
    }
}