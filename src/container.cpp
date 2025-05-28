#include "container.h"
#include "config.h"
#include <fstream>


std::string container_type_to_string(CONTAINER_TYPE type) {
    switch (type) {
        case HOT_CONTAINER: return "hot_container";
        case COLD_CONTAINER: return "cold_container";
        case CONTAINER:      return "container";
        default:
            throw std::invalid_argument("Unknown CONTAINER_TYPE value");
    }
}

CONTAINER_TYPE string_to_container_type(const std::string& str) {
    if (str == "hot_container") return HOT_CONTAINER;
    if (str == "cold_container") return COLD_CONTAINER;
    if (str == "container") return CONTAINER;
    throw std::invalid_argument("Unknown CONTAINER_TYPE string");
}

void saveContainerIndex(std::vector<ContainerKey> refContainers, int current_version){
    if(refContainers.empty()) return ;
    std::string container_index_path = Config::getInstance().getContainerIndexPath();
    std::string container_index_name(container_index_path);
    container_index_name.append("/container");
    container_index_name.append(to_string(current_version));

    ofstream outFile(container_index_name);
    if (!outFile) {
        cerr << "Unable to open file: " << container_index_name << "\n";
        return;
    }
    for (ContainerKey value : refContainers) {
        outFile << container_type_to_string(value.type) << "\t";
        outFile << value.containerId << "\n";
    }

    outFile.close();
}

std::vector<ContainerKey> loadContainerIds(int current_version) {
    std::vector<ContainerKey> refContainers;

    std::string container_index_path = Config::getInstance().getContainerIndexPath();
    std::string container_index_name = container_index_path + "/container" + to_string(current_version);

    std::ifstream inFile(container_index_name);
    if (!inFile) {
        cerr << "Unable to open file: " << container_index_name << "\n";
        return refContainers; // 返回空 vector
    }

    string line;
    string type;
    ContainerKey value;
    while (getline(inFile, line)) {
        if (line.empty()) continue;
        stringstream ss(line);
        ss >> type >> value.containerId;
        value.type = string_to_container_type(type);
        refContainers.push_back(value);
    }

    inFile.close();
    return refContainers;
}