#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

std::string findMatchingSubfolder(const std::string& baseDir, const std::string& pattern) {
    try {
        for (const auto& entry : fs::directory_iterator(baseDir)) {
            if (entry.is_directory()) {
                std::string dirName = entry.path().filename().string();
                std::transform(dirName.begin(), dirName.end(), dirName.begin(), ::tolower);
                std::string lowerPattern = pattern;
                std::transform(lowerPattern.begin(), lowerPattern.end(), lowerPattern.begin(), ::tolower);
                
                if (dirName.find(lowerPattern) != std::string::npos) {
                    return entry.path().string();
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error searching directories: " << e.what() << std::endl;
    }
    return "";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: ccgrep [-f FOLDERSUBSTRING] SEARCHSTRING" << std::endl;
        return 1;
    }
    
    std::string searchString;
    std::string folderSubstring;
    std::string searchPath;
    
    // Parse arguments
    int i = 1;
    while (i < argc) {
        if (std::string(argv[i]) == "-f" && i + 1 < argc) {
            folderSubstring = argv[i + 1];
            i += 2;
        } else {
            searchString = argv[i];
            i++;
        }
    }
    
    if (searchString.empty()) {
        std::cerr << "Error: No search string provided" << std::endl;
        return 1;
    }
    
    // Determine base directory
    const char* homeDir = std::getenv("HOME");
    if (!homeDir) {
        std::cerr << "Error: Could not determine home directory" << std::endl;
        return 1;
    }
    
    std::string baseDir = std::string(homeDir) + "/Curbcut/cc_app";
    
    // Determine search path
    if (!folderSubstring.empty()) {
        searchPath = findMatchingSubfolder(baseDir, folderSubstring);
        if (searchPath.empty()) {
            std::cerr << "Error: No subfolder matching '" << folderSubstring << "' found in " << baseDir << std::endl;
            return 1;
        }
    } else {
        searchPath = baseDir;
    }
    
    // Build grep command
    std::string grepCmd = "grep -r --color=always ";
    grepCmd += "\"" + searchString + "\" ";
    grepCmd += "--exclude-dir={node_modules,dist,.git,coverage} ";
    grepCmd += "--exclude=\"*.tsbuildinfo\" ";
    grepCmd += "\"" + searchPath + "\"";
    
    // Execute grep command
    int result = std::system(grepCmd.c_str());
    
    return WEXITSTATUS(result);
}
