#include "secondaryIndex.h"
#include "structs.h"
#include <cstring>
#include <fstream>
#include <iostream>
using namespace std;

NameIndex doctorNameIndex;

static const char* LIST_FILE = "doctor_name_list.dat";
static const char* INDEX_FILE = "doctor_name_index.dat";

void initNameIndex() {
    doctorNameIndex.count = 0;
}

static int nodeOffset(int rrn) {
    return static_cast<int>(sizeof(int) + rrn * sizeof(NameListNode));
}

static void createListFile() {
    ofstream f(LIST_FILE, ios::binary | ios::trunc);
    int freeHead = -1;
    f.write(reinterpret_cast<char*>(&freeHead), sizeof(int));
}

static void readNode(fstream& f, int rrn, NameListNode& n) {
    f.clear();
    f.seekg(nodeOffset(rrn), ios::beg);
    f.read(reinterpret_cast<char*>(&n), sizeof(NameListNode));
}

static void writeNode(fstream& f, int rrn, const NameListNode& n) {
    f.clear();
    f.seekp(nodeOffset(rrn), ios::beg);
    f.write(reinterpret_cast<const char*>(&n), sizeof(NameListNode));
}

static int allocateNode(fstream& f, const char* id, int next) {
    NameListNode n;
    memset(&n, 0, sizeof(NameListNode));
    strncpy(n.doctorID, id, KEY_SIZE);
    n.next = next;
    int freeHead = -1;
    f.clear();
    f.seekg(0, ios::beg);
    f.read(reinterpret_cast<char*>(&freeHead), sizeof(int));

    int rrn;
    if (freeHead != -1) {
        NameListNode deleted;
        readNode(f, freeHead, deleted);
        rrn = freeHead;
        int newFreeHead = deleted.next;
        f.clear();
        f.seekp(0, ios::beg);
        f.write(reinterpret_cast<char*>(&newFreeHead), sizeof(int));
    }
    else {
        f.clear();
        f.seekp(0, ios::end);
        int endOffset = static_cast<int>(f.tellp());
        rrn = (endOffset - static_cast<int>(sizeof(int))) / static_cast<int>(sizeof(NameListNode));
    }
    writeNode(f, rrn, n);
    return rrn;
}

static void freeNode(fstream& f, int rrn) {
    int freeHead = -1;
    f.clear();
    f.seekg(0, ios::beg);
    f.read(reinterpret_cast<char*>(&freeHead), sizeof(int));
    NameListNode n;
    memset(&n, 0, sizeof(NameListNode));
    n.doctorID[0] = '*';
    n.next = freeHead;
    writeNode(f, rrn, n);
    f.clear();
    f.seekp(0, ios::beg);
    f.write(reinterpret_cast<char*>(&rrn), sizeof(int));
}

static int findNamePosition(const char* name) {
    int low = 0;
    int high = doctorNameIndex.count;
    while (low < high) {
        int mid = (low + high) / 2;
        if (strncmp(doctorNameIndex.entries[mid].name, name, NAME_SIZE) < 0)
            low = mid + 1;
        else
            high = mid;
    }
    return low;
}

int binarySearchName(const char* name) {
    int pos = findNamePosition(name);
    if (pos < doctorNameIndex.count && strncmp(doctorNameIndex.entries[pos].name, name, NAME_SIZE) == 0)
        return pos;
    return -1;
}

static bool insertImpl(const char* name, const char* doctorID) {
    fstream f(LIST_FILE, ios::binary | ios::in | ios::out);
    if (!f) {
        createListFile();
        f.open(LIST_FILE, ios::binary | ios::in | ios::out);
        if (!f) return false;
    }

    int pos = findNamePosition(name);
    bool exists = pos < doctorNameIndex.count &&
        strncmp(doctorNameIndex.entries[pos].name, name, NAME_SIZE) == 0;

    if (exists) {
        int rrn = allocateNode(f, doctorID, doctorNameIndex.entries[pos].head);
        doctorNameIndex.entries[pos].head = rrn;
    }
    else {
        if (doctorNameIndex.count >= MAX_NAME_INDEX_SIZE)
            return false;
        int rrn = allocateNode(f, doctorID, -1);

        for (int i = doctorNameIndex.count; i > pos; i--)
            doctorNameIndex.entries[i] = doctorNameIndex.entries[i - 1];

        memset(doctorNameIndex.entries[pos].name, '\0', NAME_SIZE);
        strncpy(doctorNameIndex.entries[pos].name, name, NAME_SIZE);
        doctorNameIndex.entries[pos].head = rrn;
        doctorNameIndex.count++;
    }
    return true;
}

bool insertDoctorNameIndex(const char* name, const char* doctorID) {
    if (!insertImpl(name, doctorID))
        return false;
    saveDoctorNameIndex();
    return true;
}

bool deleteDoctorNameIndex(const char* name, const char* doctorID) {
    int pos = binarySearchName(name);
    if (pos == -1)
        return false;

    fstream f(LIST_FILE, ios::binary | ios::in | ios::out);
    if (!f)
        return false;

    int prev = -1;
    int cur = doctorNameIndex.entries[pos].head;
    NameListNode n;
    while (cur != -1) {
        readNode(f, cur, n);
        if (strncmp(n.doctorID, doctorID, KEY_SIZE) == 0)
            break;
        prev = cur;
        cur = n.next;
    }
    if (cur == -1)
        return false;

    if (prev == -1) {
        doctorNameIndex.entries[pos].head = n.next;
    }
    else {
        NameListNode p;
        readNode(f, prev, p);
        p.next = n.next;
        writeNode(f, prev, p);
    }
    freeNode(f, cur);

    if (doctorNameIndex.entries[pos].head == -1) {
        for (int i = pos; i < doctorNameIndex.count - 1; i++)
            doctorNameIndex.entries[i] = doctorNameIndex.entries[i + 1];
        doctorNameIndex.count--;
    }
    f.close();
    saveDoctorNameIndex();
    return true;
}

bool updateDoctorNameIndex(const char* oldName, const char* newName, const char* doctorID) {
    if (strncmp(oldName, newName, NAME_SIZE) == 0)
        return true;
    if (!insertDoctorNameIndex(newName, doctorID))
        return false;
    return deleteDoctorNameIndex(oldName, doctorID);
}

bool renameDoctorIDInNameIndex(const char* name, const char* oldID, const char* newID) {
    int pos = binarySearchName(name);
    if (pos == -1)
        return false;

    fstream f(LIST_FILE, ios::binary | ios::in | ios::out);
    if (!f)
        return false;

    int cur = doctorNameIndex.entries[pos].head;
    NameListNode n;
    while (cur != -1) {
        readNode(f, cur, n);
        if (strncmp(n.doctorID, oldID, KEY_SIZE) == 0) {
            memset(n.doctorID, '\0', KEY_SIZE);
            strncpy(n.doctorID, newID, KEY_SIZE);
            writeNode(f, cur, n);
            return true;
        }
        cur = n.next;
    }
    return false;
}

int searchDoctorNameIndex(const char* name, char outIDs[][KEY_SIZE], int maxIDs) {
    int pos = binarySearchName(name);
    if (pos == -1)
        return 0;

    fstream f(LIST_FILE, ios::binary | ios::in);
    if (!f)
        return 0;

    int found = 0;
    int cur = doctorNameIndex.entries[pos].head;
    NameListNode n;
    while (cur != -1 && found < maxIDs) {
        f.clear();
        f.seekg(nodeOffset(cur), ios::beg);
        f.read(reinterpret_cast<char*>(&n), sizeof(NameListNode));
        memcpy(outIDs[found], n.doctorID, KEY_SIZE);
        found++;
        cur = n.next;
    }
    return found;
}

void buildDoctorNameIndex(const char* doctorFile) {
    initNameIndex();
    createListFile();
    ifstream file(doctorFile, ios::binary);
    if (file) {
        file.seekg(sizeof(int), ios::beg);
        Doctor d;
        while (file.read(reinterpret_cast<char*>(&d), sizeof(Doctor))) {
            if (d.ID[0] != '*')
                insertImpl(d.name, d.ID);
        }
    }
    saveDoctorNameIndex();
}

void saveDoctorNameIndex() {
    ofstream f(INDEX_FILE, ios::binary | ios::trunc);
    if (!f)
        return;
    f.write(reinterpret_cast<char*>(&doctorNameIndex.count), sizeof(int));
    f.write(reinterpret_cast<char*>(doctorNameIndex.entries), doctorNameIndex.count * sizeof(NameIndexEntry));
}

void printDoctorNameIndex() {
    cout << "--- Doctor Name secondary index (" << doctorNameIndex.count << " names) ---" << endl;
    for (int i = 0; i < doctorNameIndex.count; i++) {
        cout << "  [";
        for (int k = 0; k < NAME_SIZE && doctorNameIndex.entries[i].name[k] != '\0'; k++)
            cout << doctorNameIndex.entries[i].name[k];
        cout << "] -> head node " << doctorNameIndex.entries[i].head << " : ";
        char ids[MAX_INDEX_SIZE][KEY_SIZE];
        int n = searchDoctorNameIndex(doctorNameIndex.entries[i].name, ids, MAX_INDEX_SIZE);
        for (int j = 0; j < n; j++) {
            for (int k = 0; k < KEY_SIZE && ids[j][k] != '\0'; k++)
                cout << ids[j][k];
            cout << (j + 1 < n ? ", " : "");
        }
        cout << endl;
    }
}