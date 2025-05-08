#include <iostream>
#include <unordered_set>
#include <vector>

struct Entry {
    int container_number;
};

int computeSomething() {
    static int x = 100;
    return x++;
}

int main() {
    std::vector<Entry> entries = {{1}, {2}, {3}};
    std::unordered_set<int> usedContainers;

    for (auto& entry : entries) {
        int originalNumber = entry.container_number;
        usedContainers.insert(originalNumber);
        entry.container_number = computeSomething();
    }

    // 打印 usedContainers
    for (int x : usedContainers) {
        std::cout << x << " ";
    }
    std::cout << std::endl;
}
