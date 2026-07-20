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
const int HASH_SIZE = 200003;  // larger prime number
const string DATA_FILE = "data.bin";
const string INDEX_FILE = "index.bin";

struct Record {
    char key[MAX_KEY_LEN + 1];
    int32_t value;
    int32_t next;  // next position in file (-1 if none)
    
    Record() : value(0), next(-1) {
        memset(key, 0, sizeof(key));
    }
    
    Record(const string& k, int32_t v) : value(v), next(-1) {
        strncpy(key, k.c_str(), MAX_KEY_LEN);
        key[MAX_KEY_LEN] = '\0';
    }
};

class DiskStorage {
private:
    int dataFd = -1;
    int indexFd = -1;
    size_t dataSize = 0;
    
    // Memory-mapped index
    int32_t* index = nullptr;
    
    // Simple cache for last accessed key
    string cachedKey;
    vector<int32_t> cachedValues;
    bool cacheValid = false;
    
    size_t hashKey(const string& key) const {
        size_t hash = 0;
        for (char c : key) {
            hash = hash * 31 + c;
        }
        return hash % HASH_SIZE;
    }
    
    streampos appendRecord(const Record& rec) {
        lseek(dataFd, 0, SEEK_END);
        streampos pos = lseek(dataFd, 0, SEEK_CUR);
        
        write(dataFd, &rec, sizeof(rec));
        dataSize += sizeof(rec);
        
        return pos;
    }
    
    void readRecord(streampos pos, Record& rec) {
        lseek(dataFd, pos, SEEK_SET);
        read(dataFd, &rec, sizeof(rec));
    }
    
    void writeRecord(streampos pos, const Record& rec) {
        lseek(dataFd, pos, SEEK_SET);
        write(dataFd, &rec, sizeof(rec));
    }
    
public:
    DiskStorage() {
        // Open data file
        dataFd = open(DATA_FILE.c_str(), O_RDWR | O_CREAT, 0644);
        if (dataFd < 0) {
            cerr << "Failed to open data file" << endl;
            exit(1);
        }
        
        // Get data file size
        struct stat st;
        fstat(dataFd, &st);
        dataSize = st.st_size;
        
        // Open and map index file
        indexFd = open(INDEX_FILE.c_str(), O_RDWR | O_CREAT, 0644);
        if (indexFd < 0) {
            cerr << "Failed to open index file" << endl;
            exit(1);
        }
        
        // Ensure index file is correct size
        fstat(indexFd, &st);
        if (st.st_size < HASH_SIZE * sizeof(int32_t)) {
            // Resize file
            ftruncate(indexFd, HASH_SIZE * sizeof(int32_t));
        }
        
        // Memory map index file
        index = (int32_t*)mmap(nullptr, HASH_SIZE * sizeof(int32_t),
                              PROT_READ | PROT_WRITE, MAP_SHARED, indexFd, 0);
        if (index == MAP_FAILED) {
            cerr << "Failed to mmap index file" << endl;
            exit(1);
        }
        
        // Initialize if file was newly created or resized
        if (st.st_size < HASH_SIZE * sizeof(int32_t)) {
            for (int i = 0; i < HASH_SIZE; i++) {
                index[i] = -1;
            }
        }
    }
    
    ~DiskStorage() {
        if (dataFd >= 0) close(dataFd);
        if (indexFd >= 0) {
            munmap(index, HASH_SIZE * sizeof(int32_t));
            close(indexFd);
        }
    }
    
    void insert(const string& key, int32_t value) {
        // Check if exists first
        size_t hash = hashKey(key);
        int32_t pos = index[hash];
        
        while (pos != -1) {
            Record rec;
            readRecord(pos, rec);
            
            if (strcmp(rec.key, key.c_str()) == 0 && rec.value == value) {
                return;  // Already exists
            }
            
            pos = rec.next;
        }
        
        // Append new record
        Record newRec(key, value);
        newRec.next = index[hash];  // point to current head
        
        streampos newPos = appendRecord(newRec);
        index[hash] = newPos;  // update head
        
        // Invalidate cache for this key
        if (cachedKey == key) {
            cacheValid = false;
        }
    }
    
    void remove(const string& key, int32_t value) {
        size_t hash = hashKey(key);
        int32_t pos = index[hash];
        int32_t prevPos = -1;
        
        while (pos != -1) {
            Record rec;
            readRecord(pos, rec);
            
            if (strcmp(rec.key, key.c_str()) == 0 && rec.value == value) {
                // Found record to delete
                if (prevPos == -1) {
                    // Update head
                    index[hash] = rec.next;
                } else {
                    // Update previous record's next pointer
                    Record prevRec;
                    readRecord(prevPos, prevRec);
                    prevRec.next = rec.next;
                    writeRecord(prevPos, prevRec);
                }
                // Invalidate cache for this key
                if (cachedKey == key) {
                    cacheValid = false;
                }
                return;
            }
            
            prevPos = pos;
            pos = rec.next;
        }
    }
    
    vector<int32_t> find(const string& key) {
        // Check cache first
        if (cacheValid && cachedKey == key) {
            return cachedValues;
        }
        
        vector<int32_t> result;
        
        size_t hash = hashKey(key);
        int32_t pos = index[hash];
        
        while (pos != -1) {
            Record rec;
            readRecord(pos, rec);
            
            if (strcmp(rec.key, key.c_str()) == 0) {
                result.push_back(rec.value);
            }
            
            pos = rec.next;
        }
        
        sort(result.begin(), result.end());
        
        // Update cache
        cachedKey = key;
        cachedValues = result;
        cacheValid = true;
        
        return result;
    }
};

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);
    
    DiskStorage storage;
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