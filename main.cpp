#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <set>

using namespace std;

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);
    
    int n;
    cin >> n;
    
    // Using in-memory structures for initial implementation
    // Will convert to file storage later
    unordered_map<string, set<int>> data;
    
    for (int i = 0; i < n; i++) {
        string command;
        cin >> command;
        
        if (command == "insert") {
            string key;
            int value;
            cin >> key >> value;
            data[key].insert(value);
            
        } else if (command == "delete") {
            string key;
            int value;
            cin >> key >> value;
            auto it = data.find(key);
            if (it != data.end()) {
                it->second.erase(value);
                if (it->second.empty()) {
                    data.erase(it);
                }
            }
            
        } else if (command == "find") {
            string key;
            cin >> key;
            auto it = data.find(key);
            if (it == data.end() || it->second.empty()) {
                cout << "null\n";
            } else {
                bool first = true;
                for (int val : it->second) {
                    if (!first) cout << " ";
                    cout << val;
                    first = false;
                }
                cout << "\n";
            }
        }
    }
    
    return 0;
}