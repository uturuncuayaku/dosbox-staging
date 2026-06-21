#include "dos_append.h"
#include <sstream>
#include <algorithm>
#include <cstdarg>
#include <cstdio>

namespace DOS_Append {
    static std::vector<std::string> search_paths;

    void SetPaths(const std::string& path_string) {
        Clear();
        
        std::stringstream ss(path_string);
        std::string item;
        
        while (std::getline(ss, item, ';')) {
            size_t start = item.find_first_not_of(" \t\r\n");
            size_t end = item.find_last_not_of(" \t\r\n");
            
            if (start != std::string::npos && end != std::string::npos) {
                std::string trimmed_path = item.substr(start, end - start + 1);
                
                if (!trimmed_path.empty() && trimmed_path.back() == '\\') {
                    trimmed_path.pop_back();
                }
                
                search_paths.push_back(trimmed_path);
            }
        }
    }

    void Clear() {
        search_paths.clear();
    }

    bool IsActive() {
        return !search_paths.empty();
    }

    const std::vector<std::string>& GetPaths() {
        return search_paths;
    }

    void LogToFile(const char* format, ...) {
        va_list args;
        va_start(args, format);
        std::vfprintf(stderr, format, args);
        std::fprintf(stderr, "\n");
        std::fflush(stderr);
        va_end(args);

        va_start(args, format);
        std::FILE* f = std::fopen("c:\\Users\\andtr\\Documents\\GitHub\\antigravity_V2_dosbos-staging\\dosbox_run.log", "a");
        if (f) {
            std::vfprintf(f, format, args);
            std::fprintf(f, "\n");
            std::fclose(f);
        }
        va_end(args);
    }
}
