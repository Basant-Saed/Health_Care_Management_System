#ifndef indexManager_h
#define indexManager_h

#include "structs.h"

using namespace std;

const int KEY_SIZE = 15;
const int MAX_INDEX_SIZE = 1000;

struct PrimaryIndexEntry {
    char key[KEY_SIZE];
    int offset;
};

struct PrimaryIndex {
    PrimaryIndexEntry entries[MAX_INDEX_SIZE];
    int count;
};

extern PrimaryIndex doctorPrimaryIndex;
extern PrimaryIndex appointmentPrimaryIndex;

void initPrimaryIndex(PrimaryIndex& index);
int searchPrimaryIndex(const PrimaryIndex& index, const char* key);
int binarySearchPrimary(const PrimaryIndex& index, const char* key);
bool insertPrimaryIndex(PrimaryIndex& index, const char* key, int offset);
bool deletePrimaryIndex(PrimaryIndex& index, const char* key);
bool updatePrimaryKey(PrimaryIndex& index, const char* oldKey, const char* newKey);

void buildDoctorPrimaryIndex(const char* filename);
void buildAppointmentPrimaryIndex(const char* filename);
void rebuildIndexes();

#endif
