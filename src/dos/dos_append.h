#pragma once

#include <vector>
#include <string>

namespace DOS_Append {
    // Parse and store semicolon-separated paths (e.g., "C:\GAMES;D:\DATA")
    void SetPaths(const std::string& path_string);
    
    // Clear the currently active append paths (used when user types "APPEND ;")
    void Clear();
    
    // Fast check for the kernel to see if it should bother looping
    bool IsActive();
    
    // Retrieve the active paths for the file resolution loop
    const std::vector<std::string>& GetPaths();

    // Log message to file and stderr
    void LogToFile(const char* format, ...);
}
