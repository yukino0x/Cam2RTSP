#include "ReadParams.h"

string ReadParams::trim(const string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    if (start == std::string::npos || end == std::string::npos) {
        return "";
    }
    return s.substr(start, end - start + 1);
}

ReadParams::ReadParams(const string& filename)
{
    ifstream fin(filename);
    if (!fin) {
        cerr << "Cannot open file: " << filename << "\n";
        return;
    }
    
    string line;
    while (getline(fin, line)) {
        // Skip empty lines or lines starting with "//"
        if (line.empty() || line.substr(0, 2) == "//") {
            continue;
        }
        // Split the line by "=" and trim the key and value
        size_t pos = line.find("=");
        if (pos == string::npos) {
            cerr << "Invalid line: " << line << "\n";
            continue;
        }
        string key = trim(line.substr(0, pos));
        string value = trim(line.substr(pos + 1));
        // Insert the key-value pair into the map
        data[key] = value;
    }
    fin.close();
}

string ReadParams::get(const string& key) const {
    auto it = data.find(key);
    if (it == data.end()) {
        return "";
    }
    return it->second;
}


void ReadParams::print() const {
    for (const auto& pair : data) {
        cout << pair.first << " = " << pair.second << "\n";
    }
}