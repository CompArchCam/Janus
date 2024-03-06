
#ifndef _HASHING_
#define _HASHING_


#define CLOSED_HASHING 0
// Node structure for linked list
struct Node {
    uintptr_t key;
    int value;
    struct Node *next;
};

// HashTable structure
struct HashTable{
    Node **table;
    int size;
    int p;
    // Function to calculate the hash index
};
int hashFunction(HashTable *hashTable, uint32_t key) {
    //return key % size;
    return ((uint32_t)(key * 2654435769) >> (32 - hashTable->p));
}

// Function to initialize the hash table
void initHashTable(HashTable *hashTable, int init_size) {
    hashTable->table = (Node **)malloc(init_size * sizeof(Node *));
    hashTable->size = init_size;
    int p = 0;
    // Initialize each slot to NULL
    for (int i = 0; i < init_size; i++) {
        hashTable->table[i] = NULL;
    }
    int num = init_size;
    while (num > 1) {
        num >>= 1; // Right shift num by 1 bit
        p++;   // Increment the power
    }
    hashTable->p = p;
}
// Function to insert a key-value pair into the hash table
void insert(HashTable *hashTable, uint32_t key, int value) {
    //int index = hashTable->hashFunction(key);
    int index  = ((uint32_t)(key * 2654435769) >> (32 - hashTable->p));
    // Create a new node
    Node *newNode = (Node *)malloc(sizeof(Node));
    newNode->key = key;
    newNode->value = value;
    newNode->next = NULL;

    // Insert at the beginning of the linked list
    newNode->next = hashTable->table[index];
    hashTable->table[index] = newNode;
}
// Function to retrieve the value associated with a key
int lookupAddr(const HashTable *hashTable, uint32_t key) {
    //int index = hashTable->hashFunction(key);
    int index  = ((uint32_t)(key * 2654435769) >> (32 - hashTable->p));
    // Traverse the linked list at the hash index
    Node *current = hashTable->table[index];
    while (current != NULL) {
        if (current->key == key) {
            // Key found, return the associated value
            //cout<<"key found"<<hex<<key<<endl;
            return current->value;
        }
        current = current->next;
    }
    // Key not found
    return 0;
}
// Function to free memory used by the hash table
void freeHashTable(HashTable *hashTable) {
    for (int i = 0; i < hashTable->size; i++) {
        Node *current = hashTable->table[i];
        while (current != NULL) {
            Node *temp = current;
            current = current->next;
            free(temp);
        }
    }

    free(hashTable->table);
}


#if 0
    // Function to initialize the hash table
    void init(int init_size){
        size = init_size;
        if(!size) return;
        table = (Node **)malloc(size * sizeof(Node *));
        // Initialize each slot to NULL
        for (int i = 0; i < size; i++) {
           table[i] = NULL;
        }
        int num = size;
        while (num > 1) {
            num >>= 1; // Right shift num by 1 bit
            p++;   // Increment the power
        }
    }
    int get_size(){
       return size;
    }
    int hashFunction(uintptr_t key) {
        //return key % size;
        //Knuth's multiplicative hash, assuming table size is power of 2
    //    return (key * 2654435769) & (size - 1);
       //assert(p>=0 && p<=32);
        return ((uint32_t)(key * 2654435769) >> (32 - p));
    }
    // Function to insert a key-value pair into the hash table
    void insert(uintptr_t key, int value) {
        int index = hashFunction(key);
        // Create a new node
        Node *newNode = (Node *)malloc(sizeof(Node));
        newNode->key = key;
        newNode->value = value;
        newNode->next = NULL;

        // Insert at the beginning of the linked list
        newNode->next = table[index];
        table[index] = newNode;
    }

    int lookup(uintptr_t key) {
        int index = hashFunction(key);

        // Traverse the linked list at the hash index
        Node *current = (Node*)table[index];
        while (current != NULL) {
            if (current->key == key) {
                // Key found, return the associated value
                return current->value;
            }
            current = current->next;
        }

        // Key not found
        return 0;
    }
    void cleanup(){
    // Function to free memory used by the hash table
        for (int i = 0; i < size; i++) {
            Node *current = table[i];
            while (current != NULL) {
                Node *temp = current;
                current = current->next;
                free(temp);
            }
        }

        free(table);
   }

};
#endif


#if CLOSED_HASHING
//closed-hash (open addressing based hashtable).
// Define the structure for the hash table entry
typedef struct {
    uintptr_t key;
    int value;
} HashEntry;

// Define the structure for the hash table
typedef struct {
    HashEntry *entries;
    int size;
    // Initialize the hash table
    init(int init_size) {
        size = init_size;
        entries = (HashEntry*)malloc(sizeof(HashEntry) * size);
        for (int i = 0; i < size; i++) {
            entries[i].key = 0;
        }
    }

    // Hash function
    int hash(uintptr_t key, int size) {
        return key % size;
    }

    // Insert a key-value pair into the hash table
    void insert(uintptr_t key, int value) {
        int index = hash(key, hashTable->size);
        while (entries[index].key != 0) {
            index = (index + 1) % hashTable->size; // Linear probing
        }
        entries[index].key = key;
        entries[index].value = value;
    }

    // Retrieve the value associated with a key from the hash table
    int lookup(uintptr_t key) {
        int index = hash(key, size);
        while (entries[index].key != key) {
            index = (index + 1) % hashTable->size;
            if (hashTable->entries[index].key == 0) {
                return -1; // Key not found
            }
        }
        return entries[index].value;
    }

    // Free memory allocated for the hash table
    void destroy() {
        free(entries);
        free(hashTable);
    }
} HashTable;
//HashTable *hashTable = (HashTable*)malloc(sizeof(HashTable));
// hashTable->init(TABLE_SIZE);

#endif
#endif
