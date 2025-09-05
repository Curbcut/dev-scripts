#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <map>
#include <set>

namespace fs = std::filesystem;

struct FileMatch {
    std::string repoName;
    std::string relativePath;
    std::string fullPath;
};

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

std::string getRepoNameFromPath(const std::string& path) {
    fs::path p(path);
    return p.filename().string();
}

std::vector<FileMatch> findFiles(const std::string& searchPath, const std::string& fileName) {
    std::vector<FileMatch> matches;
    std::string lowerFileName = fileName;
    std::transform(lowerFileName.begin(), lowerFileName.end(), lowerFileName.begin(), ::tolower);
    
    std::string command = "find \"" + searchPath + "\" -type f -name \"*\" 2>/dev/null | grep -v node_modules | grep -v dist | grep -v .git";
    
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return matches;
    
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        std::string filePath(buffer);
        filePath.erase(filePath.find_last_not_of("\n\r") + 1);
        
        std::string baseName = fs::path(filePath).filename().string();
        std::string lowerBaseName = baseName;
        std::transform(lowerBaseName.begin(), lowerBaseName.end(), lowerBaseName.begin(), ::tolower);
        
        // Remove extension for comparison
        size_t dotPos = lowerBaseName.find_last_of('.');
        if (dotPos != std::string::npos) {
            lowerBaseName = lowerBaseName.substr(0, dotPos);
        }
        
        if (lowerBaseName == lowerFileName || lowerBaseName.find(lowerFileName) != std::string::npos) {
            FileMatch match;
            match.fullPath = filePath;
            
            // Determine repo name from path
            std::string baseDir = std::string(std::getenv("HOME")) + "/Curbcut/cc_app/";
            if (filePath.find(baseDir) == 0) {
                std::string relativeFromBase = filePath.substr(baseDir.length());
                size_t firstSlash = relativeFromBase.find('/');
                if (firstSlash != std::string::npos) {
                    match.repoName = relativeFromBase.substr(0, firstSlash);
                    match.relativePath = relativeFromBase.substr(firstSlash + 1);
                }
            }
            
            if (!match.repoName.empty()) {
                matches.push_back(match);
            }
        }
    }
    
    pclose(pipe);
    return matches;
}

std::string constructGitHubUrl(const std::string& repoName, const std::string& relativePath) {
    // Map of local folder names to GitHub repo names
    std::map<std::string, std::string> repoMap = {
        {"cc.v3", "cc.v3"},
        {"curbcut-api", "curbcut-api"},
        {"npm-map", "npm-map"},
        {"npm-ui", "npm-ui"},
        {"npm-types", "npm-types"},
        {"cc.pipe", "cc.pipe"},
        {"cho", "cho"},
        {"queries", "queries"}
    };
    
    std::string githubRepo = repoMap.count(repoName) ? repoMap[repoName] : repoName;
    return "https://github.com/Curbcut/" + githubRepo + "/blob/main/" + relativePath;
}

void openInFirefox(const std::string& url) {
    std::string command = "firefox --new-window \"" + url + "\" &";
    std::system(command.c_str());
}

int promptUserChoice(const std::vector<FileMatch>& matches) {
    std::cout << "\nMultiple files found. Please choose one:\n\n";
    
    for (size_t i = 0; i < matches.size(); i++) {
        std::cout << i + 1 << ". " << matches[i].repoName << ": " << matches[i].relativePath << "\n";
    }
    
    std::cout << "\nEnter number (1-" << matches.size() << "): ";
    
    int choice;
    std::cin >> choice;
    
    if (choice < 1 || choice > static_cast<int>(matches.size())) {
        std::cerr << "Invalid choice\n";
        return -1;
    }
    
    return choice - 1;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: ccgho [-f FOLDERSUBSTRING] FILENAME\n";
        std::cerr << "Example: ccgho -f v3 MapContainer\n";
        std::cerr << "Example: ccgho UserService\n";
        return 1;
    }
    
    std::string fileName;
    std::string folderSubstring;
    std::string searchPath;
    
    // Parse arguments
    int i = 1;
    while (i < argc) {
        if (std::string(argv[i]) == "-f" && i + 1 < argc) {
            folderSubstring = argv[i + 1];
            i += 2;
        } else {
            fileName = argv[i];
            i++;
        }
    }
    
    if (fileName.empty()) {
        std::cerr << "Error: No file name provided\n";
        return 1;
    }
    
    // Determine base directory
    const char* homeDir = std::getenv("HOME");
    if (!homeDir) {
        std::cerr << "Error: Could not determine home directory\n";
        return 1;
    }
    
    std::string baseDir = std::string(homeDir) + "/Curbcut/cc_app";
    
    // Determine search path
    if (!folderSubstring.empty()) {
        searchPath = findMatchingSubfolder(baseDir, folderSubstring);
        if (searchPath.empty()) {
            std::cerr << "Error: No subfolder matching '" << folderSubstring << "' found in " << baseDir << "\n";
            return 1;
        }
    } else {
        searchPath = baseDir;
    }
    
    // Find matching files
    std::vector<FileMatch> matches = findFiles(searchPath, fileName);
    
    if (matches.empty()) {
        std::cerr << "No files matching '" << fileName << "' found\n";
        return 1;
    }
    
    FileMatch selectedMatch;
    
    if (matches.size() == 1) {
        selectedMatch = matches[0];
    } else if (!folderSubstring.empty()) {
        // If folder was specified but multiple matches found within that folder, take the first one
        selectedMatch = matches[0];
    } else {
        // Multiple matches and no folder specified - prompt user
        int choice = promptUserChoice(matches);
        if (choice < 0) {
            return 1;
        }
        selectedMatch = matches[choice];
    }
    
    // Construct GitHub URL and open
    std::string githubUrl = constructGitHubUrl(selectedMatch.repoName, selectedMatch.relativePath);
    std::cout << "Opening: " << githubUrl << "\n";
    openInFirefox(githubUrl);
    
    return 0;
}