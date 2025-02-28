
#ifndef _HASHING_
#define _HASHING_



// Node structure for linked list
typedef struct Node {
    uint64_t key;
    int value;
    struct Node *next;
} Node;

// HashTable structure
typedef struct {
    Node **table;
    int size;
} HashTable;

// Node structure for the outer hash table for nested
typedef struct OuterNode {
    uint64_t key;
    HashTable innerTable; // Inner hash table for each outer key
    struct OuterNode *next;
} OuterNode;
typedef struct {
    OuterNode **table;
    int size;
} OuterHashTable;
#define DEFAULT_INNER_SIZE 256


// Function to initialize the hash table
void initHashTable(HashTable *hashTable, int size) {
    hashTable->table = (Node **)malloc(size * sizeof(Node *));
    hashTable->size = size;

    // Initialize each slot to NULL
    for (int i = 0; i < size; i++) {
        hashTable->table[i] = NULL;
    }
}

// Function to calculate the hash index
int hashFunction(uint64_t key, int size) {
    return key % size;
}
// Function to insert a key-value pair into the hash table
void insert(HashTable *hashTable, uint64_t key, int value) {
    int index = hashFunction(key, hashTable->size);

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
int lookup(const HashTable *hashTable, uint64_t key) {
    int index = hashFunction(key, hashTable->size);

    // Traverse the linked list at the hash index
    Node *current = hashTable->table[index];
    while (current != NULL) {
        if (current->key == key) {
            // Key found, return the associated value
            return current->value;
        }
        current = current->next;
    }

    // Key not found
    return -1;
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


/*--------- Outer (Nested) HashTable ------- */
void initIJHashTable(OuterHashTable *outerHashTable, int size) {
    outerHashTable->table = (OuterNode **)malloc(size * sizeof(OuterNode *));
    outerHashTable->size = size;

    // Initialize each slot to NULL
    for (int i = 0; i < size; i++) {
        outerHashTable->table[i] = NULL;
    }
}
void insertIJ(OuterHashTable *outerHashTable, uint64_t key) {
    int index = key % outerHashTable->size;

    // Create a new node for the outer hash table
    OuterNode *newNode = (OuterNode *)malloc(sizeof(OuterNode));
    newNode->key = key;
    initHashTable(&(newNode->innerTable), DEFAULT_INNER_SIZE); // Initialize the inner hash table
    newNode->next = NULL;

    // Insert at the beginning of the linked list
    newNode->next = outerHashTable->table[index];
    outerHashTable->table[index] = newNode;
}
// Function to free memory used by the outer hash table
void freeIJTable(OuterHashTable *outerHashTable) {
    for (int i = 0; i < outerHashTable->size; i++) {
        OuterNode *current = outerHashTable->table[i];
        while (current != NULL) {
            OuterNode *temp = current;
            current = current->next;
            freeHashTable(&(temp->innerTable)); // Free memory used by inner hash table
            free(temp);
        }
    }

    free(outerHashTable->table);
}
//for the function address (outer key) provided, return if the target address (inner key) is within the range of that function.
int lookupIJTable(const OuterHashTable *outerHashTable, uint64_t outerKey, uint64_t innerKey) {
    int outerIndex = outerKey % outerHashTable->size;

    // Traverse the linked list at the outer hash index
    OuterNode *outerCurrent = outerHashTable->table[outerIndex];
    while (outerCurrent != NULL) {
        if (outerCurrent->key == outerKey) {
            // Outer key found, now look up in the inner hash table
            int innerIndex = innerKey % outerCurrent->innerTable.size;
            Node *innerCurrent = outerCurrent->innerTable.table[innerIndex];
            while (innerCurrent != NULL) {
                if (innerCurrent->key == innerKey) {
                    // Inner key found, return the associated value
                    return innerCurrent->value;
                }
                innerCurrent = innerCurrent->next;
            }
            // Inner key not found
            return -1;
        }
        outerCurrent = outerCurrent->next;
    }

    // Outer key not found
    return -1;
}

HashTable* getInnerTableForKey(const OuterHashTable *outerHashTable, uint64_t outerKey) {
    int outerIndex = outerKey % outerHashTable->size;

    // Traverse the linked list at the hash index
    OuterNode *current = outerHashTable->table[outerIndex];
    while (current != NULL) {
        if (current->key == outerKey) {
            // Outer key found, return its inner table
            return &current->innerTable;
        }
        current = current->next;
    }
    // Outer key not found, return NULL
    return NULL;
}
// for a 
void insertIJTableWithList(OuterHashTable *outerHashTable, uint64_t outerKey, uint64_t innerKey, int value) {
    int outerIndex = outerKey % outerHashTable->size;
    
    OuterNode *current = outerHashTable->table[outerIndex];
    HashTable* innerTable =  getInnerTableForKey(outerHashTable, outerKey); 
    if(innerTable != NULL) {
           insert(innerTable, innerKey, 1);
    }
    else{
        // Create a new node for the outer hash table
        OuterNode *newNode = (OuterNode *)malloc(sizeof(OuterNode));
        newNode->key = outerKey;
        initHashTable(&(newNode->innerTable), DEFAULT_INNER_SIZE); // Initialize the inner hash table
        newNode->next = NULL;

        // Insert at the beginning of the linked list
        newNode->next = outerHashTable->table[outerIndex];
        outerHashTable->table[outerIndex] = newNode;

        // Insert inner key-value pair into the inner hash table
        insert(&(newNode->innerTable), innerKey, 1);
    }


}
#endif
