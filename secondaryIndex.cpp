#include "secondaryIndex.h"
#include <cstring>
#include <cstddef>
#include <fstream>

using namespace std;

SecondaryIndex doctorNameIndex        = {"doctor_name_list.dat",        NAME_SIZE,      0, {}};
SecondaryIndex doctorSpecialtyIndex   = {"doctor_specialty_list.dat",   SPECIALTY_SIZE, 0, {}};
SecondaryIndex appointmentDoctorIndex = {"appointment_doctor_list.dat", KEY_SIZE,       0, {}};

static void readNode(fstream& f, int rrn, SecondaryNode& n) {
    f.clear();
    f.seekg(static_cast<streamoff>(rrn) * sizeof(SecondaryNode), ios::beg);
    f.read(reinterpret_cast<char*>(&n), sizeof(SecondaryNode));
}

static void writeNode(fstream& f, int rrn, const SecondaryNode& n) {
    f.clear();
    f.seekp(static_cast<streamoff>(rrn) * sizeof(SecondaryNode), ios::beg);
    f.write(reinterpret_cast<const char*>(&n), sizeof(SecondaryNode));
}

static int findPosition(const SecondaryIndex& x, const char* key) {
    int low = 0, high = x.count;
    while (low < high) {
        int mid = (low + high) / 2;
        if (strncmp(x.entries[mid].key, key, x.keySize) < 0)
            low = mid + 1;
        else
            high = mid;
    }
    return low;
}

static int binarySearch(const SecondaryIndex& x, const char* key) {
    int pos = findPosition(x, key);
    if (pos < x.count && strncmp(x.entries[pos].key, key, x.keySize) == 0)
        return pos;
    return -1;
}

static bool insertKey(SecondaryIndex& x, const char* key, const char* id) {
    fstream f(x.listFile, ios::binary | ios::in | ios::out);
    if (!f) {
        { ofstream create(x.listFile, ios::binary | ios::app); }
        f.open(x.listFile, ios::binary | ios::in | ios::out);
        if (!f) return false;
    }

    int pos = findPosition(x, key);
    bool exists = (pos < x.count && strncmp(x.entries[pos].key, key, x.keySize) == 0);
    if (!exists && x.count >= MAX_SECONDARY_SIZE)
        return false;

    f.clear();
    f.seekp(0, ios::end);
    int rrn = static_cast<int>(f.tellp() / static_cast<streamoff>(sizeof(SecondaryNode)));

    SecondaryNode n;
    memset(&n, 0, sizeof(SecondaryNode));
    strncpy(n.id, id, KEY_SIZE);
    n.next = exists ? x.entries[pos].head : -1;
    writeNode(f, rrn, n);

    if (exists) {
        x.entries[pos].head = rrn;
    } else {
        for (int i = x.count; i > pos; i--)
            x.entries[i] = x.entries[i - 1];
        memset(x.entries[pos].key, 0, NAME_SIZE);
        strncpy(x.entries[pos].key, key, x.keySize);
        x.entries[pos].head = rrn;
        x.count++;
    }
    return true;
}

static bool deleteKey(SecondaryIndex& x, const char* key, const char* id) {
    int pos = binarySearch(x, key);
    if (pos == -1) return false;

    fstream f(x.listFile, ios::binary | ios::in | ios::out);
    if (!f) return false;

    int prev = -1, cur = x.entries[pos].head;
    SecondaryNode n;
    while (cur != -1) {
        readNode(f, cur, n);
        if (strncmp(n.id, id, KEY_SIZE) == 0)
            break;
        prev = cur;
        cur = n.next;
    }
    if (cur == -1) return false;

    if (prev == -1) {
        x.entries[pos].head = n.next;
    } else {
        SecondaryNode p;
        readNode(f, prev, p);
        p.next = n.next;
        writeNode(f, prev, p);
    }

    if (x.entries[pos].head == -1) {
        for (int i = pos; i < x.count - 1; i++)
            x.entries[i] = x.entries[i + 1];
        x.count--;
    }
    return true;
}

static bool updateKey(SecondaryIndex& x, const char* oldKey, const char* newKey, const char* id) {
    if (strncmp(oldKey, newKey, x.keySize) == 0)
        return true;
    return insertKey(x, newKey, id) && deleteKey(x, oldKey, id);
}

static bool renameId(SecondaryIndex& x, const char* key, const char* oldID, const char* newID) {
    int pos = binarySearch(x, key);
    if (pos == -1) return false;

    fstream f(x.listFile, ios::binary | ios::in | ios::out);
    if (!f) return false;

    SecondaryNode n;
    for (int cur = x.entries[pos].head; cur != -1; cur = n.next) {
        readNode(f, cur, n);
        if (strncmp(n.id, oldID, KEY_SIZE) == 0) {
            memset(n.id, 0, KEY_SIZE);
            strncpy(n.id, newID, KEY_SIZE);
            writeNode(f, cur, n);
            return true;
        }
    }
    return false;
}

static int searchKey(const SecondaryIndex& x, const char* key, char outIDs[][KEY_SIZE], int maxIDs) {
    int pos = binarySearch(x, key);
    if (pos == -1) return 0;

    fstream f(x.listFile, ios::binary | ios::in);
    if (!f) return 0;

    int found = 0;
    SecondaryNode n;
    for (int cur = x.entries[pos].head; cur != -1 && found < maxIDs; cur = n.next) {
        readNode(f, cur, n);
        memcpy(outIDs[found++], n.id, KEY_SIZE);
    }
    return found;
}

static void rebuild(SecondaryIndex& x, const char* dataFile, size_t recordSize, size_t keyOffset) {
    x.count = 0;
    { ofstream reset(x.listFile, ios::binary | ios::trunc); }
    ifstream in(dataFile, ios::binary);
    if (!in) return;

    in.seekg(sizeof(int), ios::beg);
    char rec[128];
    while (in.read(rec, static_cast<streamsize>(recordSize))) {
        if (rec[0] != '*')
            insertKey(x, rec + keyOffset, rec);
    }
}

// Doctor Name Index
int binarySearchName(const char* name) {
    return binarySearch(doctorNameIndex, name);
}
bool insertDoctorNameIndex(const char* name, const char* doctorID) {
    return insertKey(doctorNameIndex, name, doctorID);
}
bool deleteDoctorNameIndex(const char* name, const char* doctorID) {
    return deleteKey(doctorNameIndex, name, doctorID);
}
bool updateDoctorNameIndex(const char* oldName, const char* newName, const char* doctorID) {
    return updateKey(doctorNameIndex, oldName, newName, doctorID);
}
bool renameDoctorIDInNameIndex(const char* name, const char* oldID, const char* newID) {
    return renameId(doctorNameIndex, name, oldID, newID);
}
int searchDoctorNameIndex(const char* name, char outIDs[][KEY_SIZE], int maxIDs) {
    return searchKey(doctorNameIndex, name, outIDs, maxIDs);
}
void buildDoctorNameIndex(const char* doctorFile) {
    rebuild(doctorNameIndex, doctorFile, sizeof(Doctor), offsetof(Doctor, name));
}

// Doctor Specialty Index
int binarySearchSpecialty(const char* specialty) {
    return binarySearch(doctorSpecialtyIndex, specialty);
}
bool insertDoctorSpecialtyIndex(const char* specialty, const char* doctorID) {
    return insertKey(doctorSpecialtyIndex, specialty, doctorID);
}
bool deleteDoctorSpecialtyIndex(const char* specialty, const char* doctorID) {
    return deleteKey(doctorSpecialtyIndex, specialty, doctorID);
}
bool updateDoctorSpecialtyIndex(const char* oldSpecialty, const char* newSpecialty, const char* doctorID) {
    return updateKey(doctorSpecialtyIndex, oldSpecialty, newSpecialty, doctorID);
}
bool renameDoctorIDInSpecialtyIndex(const char* specialty, const char* oldID, const char* newID) {
    return renameId(doctorSpecialtyIndex, specialty, oldID, newID);
}
int searchDoctorSpecialtyIndex(const char* specialty, char outIDs[][KEY_SIZE], int maxIDs) {
    return searchKey(doctorSpecialtyIndex, specialty, outIDs, maxIDs);
}
void buildDoctorSpecialtyIndex(const char* doctorFile) {
    rebuild(doctorSpecialtyIndex, doctorFile, sizeof(Doctor), offsetof(Doctor, specialization));
}

// Appointment Doctor ID Index
int binarySearchAppointmentDoctor(const char* doctorID) {
    return binarySearch(appointmentDoctorIndex, doctorID);
}
bool insertAppointmentDoctorIndex(const char* doctorID, const char* appointmentID) {
    return insertKey(appointmentDoctorIndex, doctorID, appointmentID);
}
bool deleteAppointmentDoctorIndex(const char* doctorID, const char* appointmentID) {
    return deleteKey(appointmentDoctorIndex, doctorID, appointmentID);
}
int searchAppointmentsByDoctorID(const char* doctorID, char outIDs[][KEY_SIZE], int maxIDs) {
    return searchKey(appointmentDoctorIndex, doctorID, outIDs, maxIDs);
}
void buildAppointmentDoctorIndex(const char* appointmentFile) {
    rebuild(appointmentDoctorIndex, appointmentFile, sizeof(Appointment), offsetof(Appointment, doctor_id));
}
