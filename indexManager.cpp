#include "indexManager.h"
#include "structs.h"
#include <cstring>
#include <fstream>
using namespace std;

PrimaryIndex doctorPrimaryIndex;
PrimaryIndex appointmentPrimaryIndex;

void initPrimaryIndex(PrimaryIndex& index) {
    index.count = 0;
}
static int findPosition(const PrimaryIndex& index, const char* key) {
    int low = 0;
    int high = index.count;

    while (low < high) {
        int mid = (low + high) / 2;
        if (strncmp(index.entries[mid].key, key, KEY_SIZE) < 0)
            low = mid + 1;
        else
            high = mid;
    }
    return low;
}

int binarySearchPrimary(const PrimaryIndex& index, const char* key) {
    int pos = findPosition(index, key);

    if (pos < index.count && strncmp(index.entries[pos].key, key, KEY_SIZE) == 0)
        return pos;

    return -1;
}

int searchPrimaryIndex(const PrimaryIndex& index, const char* key) {
    int pos = binarySearchPrimary(index, key);
    if (pos == -1)
        return -1;
    return index.entries[pos].offset;
}

bool insertPrimaryIndex(PrimaryIndex& index, const char* key, int offset) {
    if (index.count >= MAX_INDEX_SIZE)
        return false;    

    int pos = findPosition(index, key);
    if (pos < index.count&& strncmp(index.entries[pos].key, key, KEY_SIZE) == 0)
        return false;

    for (int i = index.count; i > pos; i--)
        index.entries[i] = index.entries[i - 1];

    memset(index.entries[pos].key, '\0', KEY_SIZE);
    strncpy(index.entries[pos].key, key, KEY_SIZE);
    index.entries[pos].offset = offset;
    index.count++;
    return true;
}

bool deletePrimaryIndex(PrimaryIndex& index, const char* key) {
    int pos = binarySearchPrimary(index, key);
    if (pos == -1)
        return false;              
    for (int i = pos; i < index.count - 1; i++)
        index.entries[i] = index.entries[i + 1];

    index.count--;
    return true;
}

static void buildFromFile(PrimaryIndex& index, const char* filename, int recordSize) {
    initPrimaryIndex(index);

    ifstream file(filename, ios::binary);
    if (!file)
        return;

    file.seekg(sizeof(int), ios::beg);     
    char buffer[128];              
    int offset = sizeof(int);

    while (file.read(buffer, recordSize)) {
        if (buffer[0] != '*')           
            insertPrimaryIndex(index, buffer, offset);
        offset += recordSize;
    }
}

void buildDoctorPrimaryIndex(const char* filename) {
    buildFromFile(doctorPrimaryIndex, filename, sizeof(Doctor));
}

void buildAppointmentPrimaryIndex(const char* filename) {
    buildFromFile(appointmentPrimaryIndex, filename, sizeof(Appointment));
}