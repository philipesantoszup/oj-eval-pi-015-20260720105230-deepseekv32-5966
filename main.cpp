#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <set>
#include <fstream>

using namespace std;

const string STATE_FILE = "state.bin";

// Simple file-based storage that persists state
class PersistentStorage {
private:
    unordered_map<string, set<int>> data;
    bool loaded = false;
    
    void loadState() {
        if (loaded) return;
        
        ifstream file(STATE_FILE, ios::binary);
        if (!file) {
            loaded = true;
            return;
        }
        
        int size;
        file.read(reinterpret_cast<char*>(&size), sizeof(size));
        
        for (int i = 0; i < size; i++) {
            // Read key length
            int keyLen;
            file.read(reinterpret_cast<char*>(&keyLen), sizeof(keyLen));
            
            // Read key
            string key(keyLen, ' ');
            file.read(&key[0], keyLen);
            
            // Read values count
            int valCount;
            file.read(reinterpret_cast<char*>(&valCount), sizeof(valCount));
            
            // Read values
            for (int j = 0; j < valCount; j++) {
                int value;
                file.read(reinterpret_cast<char*>(&value), sizeof(value));
                data[key].insert(value);
            }
        }
        
        loaded = true;
    }
    
    void saveState() {
        ofstream file(STATE_FILE, ios::binary);
        
        int size = data.size();
        file.write(reinterpret_cast<char*>(&size), sizeof(size));
        
        for (const auto& [key, values] : data) {
            int keyLen = key.size();
            file.write(reinterpret_cast<char*>(&keyLen), sizeof(keyLen));
            file.write(key.c_str(), keyLen);
            
            int valCount = values.size();
            file.write(reinterpret_cast<char*>(&valCount), sizeof(valCount));
            
            for (int val : values) {
                file.write(reinterpret_cast<char*>(&val), sizeof(val));
            }
        }
    }
    
public:
    PersistentStorage() {
        loadState();
    }
    
    ~PersistentStorage() {
        saveState();
    }
    
    void insert(const string& key, int value) {
        data[key].insert(value);
    }
    
    void remove(const string& key, int value) {
        auto it = data.find(key);
        if (it != data.end()) {
            it->second.erase(value);
            if (it->second.empty()) {
                data.erase(it);
            }
        }
    }
    
    vector<int> find(const string& key) {
        auto it = data.find(key);
        if (it == data.end()) {
            return {};
        }
        
        vector<int> result(it->second.begin(), it->second.end());
        return result;
    }
};

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);
    
    PersistentStorage storage;
    int n;
    cin >> n;
    
    for (int i = 0; i < n; i++) {
        string command;
        cin >> command;
        
        if (command == "insert") {
            string key;
            int value;
            cin >> key >> value;
            storage.insert(key, value);
            
        } else if (command == "delete") {
            string key;
            int value;
            cin >> key >> value;
            storage.remove(key, value);
            
        } else if (command == "find") {
            string key;
            cin >> key;
            vector<int> values = storage.find(key);
            
            if (values.empty()) {
                cout << "null\n";
            } else {
                for (size_t j = 0; j < values.size(); j++) {
                    if (j > 0) cout << " ";
                    cout << values[j];
                }
                cout << "\n";
            }
        }
    }
    
    return 0;
}