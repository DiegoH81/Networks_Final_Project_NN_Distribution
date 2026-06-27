#include <iostream>
#include <fstream>
#include <vector>
#include <string>

std::vector<std::vector<std::string>> readCSV(const std::string& path, int numPartition) {
    
    std::vector<std::vector<std::string>> res(numPartition);
    std::ifstream file(path);

    if (!file.is_open()) {
        std::cerr << "Error: cannot open file " << path << std::endl;
        return res;
    }
    std::string line;
    getline(file, line);

    int temp = 0;
    while (getline(file, line)) {
        if (!line.empty()) {
            res[temp].push_back(line);
            temp++;

            if (temp == numPartition)
                temp = 0;
        }
    }
    file.close();

    return res;
}

int main() {
    std::vector<std::vector<std::string>> ar = readCSV("Dataset of Diabetes .csv", 10);

    for (int i = 0; i < ar.size(); i++) {
        std::cout << "data: " << i + 1 << " size = " << ar[i].size() << std::endl;
    }
}