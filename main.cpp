#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

using namespace std;

const int MAX_KEY_LEN = 64;
const int HASH_SIZE = 200003;
const string DATA_FILE = "data.bin";
const string INDEX_FILE = "index.bin";

struct ValueRecord {
    char key[MAX_KEY_LEN + 1];
    int32_t count;
    int32_t values[0];  // flexible array
    
    ValueRecord() : count(0) {
        memset(key, 0, sizeof(key));
    }
    
    ValueRecord(const string& k) : count(0) {
        strncpy(key, k.c_str(), MAX_KEY_LEN);
        key[MAX_KEY_LEN] = '\0';
    }
    
    size_t size() const {
        return sizeof(ValueRecord) + count * sizeof(int32_t);
    }
};

class FastStorage {
private:
    int dataFd = -1;
    int indexFd = -1;
    
    // Memory-mapped index
    int32_t* index = nullptr;
    
    size_t hashKey(const string& key) const {
        size_t hash = 0;
        for (char c : key) {
            hash = hash * 31 + c;
        }
        return hash % HASH_SIZE;
    }
    
    streampos appendData(const void* data, size_t size) {
        lseek(dataFd, 0, SEEK_END);
        streampos pos = lseek(dataFd, 0, SEEK_CUR);
        write(dataFd, data, size);
        return pos;
    }
    
    void readData(streampos pos, void* data, size_t size) {
        lseek(dataFd, pos, SEEK_SET);
        read(dataFd, data, size);
    }
    
    void writeData(streampos pos, const void* data, size_t size) {
        lseek(dataFd, pos, SEEK_SET);
        write(dataFd, data, size);
    }
    
    void deleteRecord(streampos pos) {
        // Mark as deleted
        int32_t marker = -1;
        writeData(pos + offsetof(ValueRecord, count), &marker, sizeof(marker));
    }
    
    vector<int32_t> getValues(const string& key, streampos pos) {
        // Read count
        int32_t count;
        readData(pos + offsetof(ValueRecord, count), &count, sizeof(count));
        
        if (count <= 0) {
            return {};
        }
        
        // Read values
        vector<int32_t> values(count);
        readData(pos + (streampos)sizeof(ValueRecord), values.data(), count * sizeof(int32_t));
        
        return values;
    }
    
public:
    FastStorage() {
        dataFd = open(DATA_FILE.c_str(), O_RDWR | O_CREAT, 0644);
        if (dataFd < 0) exit(1);
        
        indexFd = open(INDEX_FILE.c_str(), O_RDWR | O_CREAT, 0644);
        if (indexFd < 0) exit(1);
        
        struct stat st;
        fstat(indexFd, &st);
        if (st.st_size < HASH_SIZE * sizeof(int32_t)) {
            ftruncate(indexFd, HASH_SIZE * sizeof(int32_t));
        }
        
        index = (int32_t*)mmap(nullptr, HASH_SIZE * sizeof(int32_t),
                              PROT_READ | PROT_WRITE, MAP_SHARED, indexFd, 0);
        if (index == MAP_FAILED) exit(1);
        
        if (st.st_size < HASH_SIZE * sizeof(int32_t)) {
            for (int i = 0; i < HASH_SIZE; i++) {
                index[i] = -1;
            }
        }
    }
    
    ~FastStorage() {
        if (dataFd >= 0) close(dataFd);
        if (indexFd >= 0) {
            munmap(index, HASH_SIZE * sizeof(int32_t));
            close(indexFd);
        }
    }
    
    void insert(const string& key, int32_t value) {
        size_t hash = hashKey(key);
        streampos pos = index[hash];
        
        if (pos != -1) {
            char storedKey[MAX_KEY_LEN + 1];
            readData(pos, storedKey, sizeof(storedKey));
            
            if (strcmp(storedKey, key.c_str()) == 0) {
                vector<int32_t> values = getValues(key, pos);
                if (std::find(values.begin(), values.end(), value) != values.end()) {
                    return;
                }
                
                values.push_back(value);
                sort(values.begin(), values.end());
                
                deleteRecord(pos);
                
                ValueRecord rec(key);
                rec.count = values.size();
                
                streampos newPos = appendData(&rec, sizeof(rec));
                appendData(values.data(), values.size() * sizeof(int32_t));
                
                index[hash] = newPos;
                return;
            }
        }
        
        vector<int32_t> values = {value};
        ValueRecord rec(key);
        rec.count = 1;
        
        streampos newPos = appendData(&rec, sizeof(rec));
        appendData(values.data(), sizeof(int32_t));
        
        index[hash] = newPos;
    }
    
    void remove(const string& key, int32_t value) {
        size_t hash = hashKey(key);
        streampos pos = index[hash];
        
        if (pos == -1) return;
        
        char storedKey[MAX_KEY_LEN + 1];
        readData(pos, storedKey, sizeof(storedKey));
        
        if (strcmp(storedKey, key.c_str()) != 0) return;
        
        vector<int32_t> values = getValues(key, pos);
        auto it = std::find(values.begin(), values.end(), value);
        if (it == values.end()) return;
        
        values.erase(it);
        
        if (values.empty()) {
            deleteRecord(pos);
            index[hash] = -1;
        } else {
            deleteRecord(pos);
            
            ValueRecord rec(key);
            rec.count = values.size();
            
            streampos newPos = appendData(&rec, sizeof(rec));
            appendData(values.data(), values.size() * sizeof(int32_t));
            
            index[hash] = newPos;
        }
    }
    
    vector<int32_t> find(const string& key) {
        size_t hash = hashKey(key);
        streampos pos = index[hash];
        
        if (pos == -1) return {};
        
        char storedKey[MAX_KEY_LEN + 1];
        readData(pos, storedKey, sizeof(storedKey));
        
        if (strcmp(storedKey, key.c_str()) != 0) return {};
        
        return getValues(key, pos);
    }
};

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);
    
    FastStorage storage;
    int n;
    cin >> n;
    
    for (int i = 0; i < n; i++) {
        string command;
        cin >> command;
        
        if (command == "insert") {
            string key;
            int32_t value;
            cin >> key >> value;
            storage.insert(key, value);
            
        } else if (command == "delete") {
            string key;
            int32_t value;
            cin >> key >> value;
            storage.remove(key, value);
            
        } else if (command == "find") {
            string key;
            cin >> key;
            vector<int32_t> values = storage.find(key);
            
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