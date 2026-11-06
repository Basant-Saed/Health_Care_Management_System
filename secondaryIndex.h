#ifndef secondaryIndex_h
#define secondaryIndex_h
#include "structs.h"
#include "indexManager.h"
using namespace std;

const int NAME_SIZE = 30;
const int MAX_NAME_INDEX_SIZE = 1000;

#pragma pack(push, 1)
struct NameIndexEntry {
    char name[NAME_SIZE];
    int head;
};

struct NameListNode {
    char doctorID[KEY_SIZE];
    int next;
};
#pragma pack(pop)

struct NameIndex {
    NameIndexEntry entries[MAX_NAME_INDEX_SIZE];
    int count;
};

extern NameIndex doctorNameIndex;

void initNameIndex();
int binarySearchName(const char* name);
bool insertDoctorNameIndex(const char* name, const char* doctorID);
bool deleteDoctorNameIndex(const char* name, const char* doctorID);
bool updateDoctorNameIndex(const char* oldName, const char* newName, const char* doctorID);
bool renameDoctorIDInNameIndex(const char* name, const char* oldID, const char* newID);
int searchDoctorNameIndex(const char* name, char outIDs[][KEY_SIZE], int maxIDs);
void buildDoctorNameIndex(const char* doctorFile);
void saveDoctorNameIndex();
void printDoctorNameIndex();

#endif