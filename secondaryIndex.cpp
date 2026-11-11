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



// ================= SPECIALTY SECONDARY INDEX =================

SpecialtyIndex doctorSpecialtyIndex;

static const char* SPECIALTY_LIST_FILE = "doctor_specialty_list.dat";
static const char* SPECIALTY_INDEX_FILE = "doctor_specialty_index.dat";

void initSpecialtyIndex() {
    doctorSpecialtyIndex.count = 0;
}

static int specialtyNodeOffset(int rrn) {
    return static_cast<int>(sizeof(int) + rrn * sizeof(SpecialtyListNode));
}

static void createSpecialtyListFile() {
    ifstream check(SPECIALTY_LIST_FILE, ios::binary);

    if (check) {
        check.close();
        return;
    }

    check.close();

    ofstream f(SPECIALTY_LIST_FILE, ios::binary);
    int freeHead = -1;

    f.write(reinterpret_cast<char*>(&freeHead), sizeof(int));
}

static void resetSpecialtyListFile() {
    ofstream f(SPECIALTY_LIST_FILE, ios::binary | ios::trunc);

    int freeHead = -1;

    f.write(reinterpret_cast<char*>(&freeHead), sizeof(int));
}

static void readSpecialtyNode(fstream& f, int rrn, SpecialtyListNode& n) {
    f.clear();
    f.seekg(specialtyNodeOffset(rrn), ios::beg);
    f.read(reinterpret_cast<char*>(&n), sizeof(SpecialtyListNode));
}

static void writeSpecialtyNode(fstream& f, int rrn, const SpecialtyListNode& n) {
    f.clear();
    f.seekp(specialtyNodeOffset(rrn), ios::beg);
    f.write(reinterpret_cast<const char*>(&n), sizeof(SpecialtyListNode));
}

static int allocateSpecialtyNode(fstream& f, const char* id, int next) {
    SpecialtyListNode n;
    memset(&n, 0, sizeof(SpecialtyListNode));
    strncpy(n.doctorID, id, KEY_SIZE);
    n.next = next;

    int freeHead = -1;
    f.clear();
    f.seekg(0, ios::beg);
    f.read(reinterpret_cast<char*>(&freeHead), sizeof(int));

    int rrn;

    if (freeHead != -1) {
        SpecialtyListNode deleted;
        readSpecialtyNode(f, freeHead, deleted);

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

        rrn = (endOffset - static_cast<int>(sizeof(int))) /
              static_cast<int>(sizeof(SpecialtyListNode));
    }

    writeSpecialtyNode(f, rrn, n);
    return rrn;
}

static void freeSpecialtyNode(fstream& f, int rrn) {
    int freeHead = -1;

    f.clear();
    f.seekg(0, ios::beg);
    f.read(reinterpret_cast<char*>(&freeHead), sizeof(int));

    SpecialtyListNode n;
    memset(&n, 0, sizeof(SpecialtyListNode));

    n.doctorID[0] = '*';
    n.next = freeHead;

    writeSpecialtyNode(f, rrn, n);

    f.clear();
    f.seekp(0, ios::beg);
    f.write(reinterpret_cast<char*>(&rrn), sizeof(int));
}

static int findSpecialtyPosition(const char* specialty) {
    int low = 0;
    int high = doctorSpecialtyIndex.count;

    while (low < high) {
        int mid = (low + high) / 2;

        if (strncmp(doctorSpecialtyIndex.entries[mid].specialty,
                    specialty, SPECIALTY_SIZE) < 0)
            low = mid + 1;
        else
            high = mid;
    }

    return low;
}

int binarySearchSpecialty(const char* specialty) {
    int pos = findSpecialtyPosition(specialty);

    if (pos < doctorSpecialtyIndex.count &&
        strncmp(doctorSpecialtyIndex.entries[pos].specialty,
                specialty, SPECIALTY_SIZE) == 0)
        return pos;

    return -1;
}



static bool insertSpecialtyImpl(const char* specialty, const char* doctorID) {
    fstream f(SPECIALTY_LIST_FILE, ios::binary | ios::in | ios::out);

    if (!f) {
        createSpecialtyListFile();
        f.open(SPECIALTY_LIST_FILE, ios::binary | ios::in | ios::out);

        if (!f)
            return false;
    }

    int pos = findSpecialtyPosition(specialty);

    bool exists = pos < doctorSpecialtyIndex.count &&
        strncmp(doctorSpecialtyIndex.entries[pos].specialty,
                specialty, SPECIALTY_SIZE) == 0;

    if (exists) {
        int rrn = allocateSpecialtyNode(
            f, doctorID, doctorSpecialtyIndex.entries[pos].head
        );

        doctorSpecialtyIndex.entries[pos].head = rrn;
    }
    else {
        if (doctorSpecialtyIndex.count >= MAX_SPECIALTY_INDEX_SIZE) {
            f.close();
            return false;
        }

        int rrn = allocateSpecialtyNode(f, doctorID, -1);

        for (int i = doctorSpecialtyIndex.count; i > pos; i--)
            doctorSpecialtyIndex.entries[i] =
                doctorSpecialtyIndex.entries[i - 1];

        memset(doctorSpecialtyIndex.entries[pos].specialty,
               '\0', SPECIALTY_SIZE);

        strncpy(doctorSpecialtyIndex.entries[pos].specialty,
                specialty, SPECIALTY_SIZE);

        doctorSpecialtyIndex.entries[pos].head = rrn;
        doctorSpecialtyIndex.count++;
    }

    f.close();
    return true;
}



bool insertDoctorSpecialtyIndex(const char* specialty, const char* doctorID) {
    if (!insertSpecialtyImpl(specialty, doctorID))
        return false;

    saveSpecialtyIndex();
    return true;
}



bool deleteDoctorSpecialtyIndex(const char* specialty, const char* doctorID) {
    int pos = binarySearchSpecialty(specialty);

    if (pos == -1)
        return false;

    fstream f(SPECIALTY_LIST_FILE, ios::binary | ios::in | ios::out);

    if (!f)
        return false;

    int prev = -1;
    int cur = doctorSpecialtyIndex.entries[pos].head;
    SpecialtyListNode n;

    while (cur != -1) {
        readSpecialtyNode(f, cur, n);

        if (strncmp(n.doctorID, doctorID, KEY_SIZE) == 0)
            break;

        prev = cur;
        cur = n.next;
    }

    if (cur == -1) {
        f.close();
        return false;
    }

    if (prev == -1) {
        doctorSpecialtyIndex.entries[pos].head = n.next;
    }
    else {
        SpecialtyListNode p;
        readSpecialtyNode(f, prev, p);

        p.next = n.next;
        writeSpecialtyNode(f, prev, p);
    }

    freeSpecialtyNode(f, cur);

    if (doctorSpecialtyIndex.entries[pos].head == -1) {
        for (int i = pos; i < doctorSpecialtyIndex.count - 1; i++)
            doctorSpecialtyIndex.entries[i] =
                doctorSpecialtyIndex.entries[i + 1];

        doctorSpecialtyIndex.count--;
    }

    f.close();
    saveSpecialtyIndex();
    return true;
}


bool updateDoctorSpecialtyIndex(const char* oldSpecialty,const char* newSpecialty,const char* doctorID) {
    if (strncmp(oldSpecialty, newSpecialty, SPECIALTY_SIZE) == 0)
        return true;

    if (!insertSpecialtyImpl(newSpecialty, doctorID))
        return false;

    if (!deleteDoctorSpecialtyIndex(oldSpecialty, doctorID))
        return false;

    saveSpecialtyIndex();
    return true;
}



bool renameDoctorIDInSpecialtyIndex(const char* specialty,const char* oldID,const char* newID) {
    int pos = binarySearchSpecialty(specialty);

    if (pos == -1)
        return false;

    fstream f(SPECIALTY_LIST_FILE, ios::binary | ios::in | ios::out);

    if (!f)
        return false;

    int cur = doctorSpecialtyIndex.entries[pos].head;
    SpecialtyListNode n;

    while (cur != -1) {
        readSpecialtyNode(f, cur, n);

        if (strncmp(n.doctorID, oldID, KEY_SIZE) == 0) {
            memset(n.doctorID, '\0', KEY_SIZE);
            strncpy(n.doctorID, newID, KEY_SIZE);

            writeSpecialtyNode(f, cur, n);
            f.close();
            return true;
        }

        cur = n.next;
    }

    f.close();
    return false;
}



int searchDoctorSpecialtyIndex(const char* specialty,char outIDs[][KEY_SIZE],int maxIDs) {
    int pos = binarySearchSpecialty(specialty);

    if (pos == -1)
        return 0;

    fstream f(SPECIALTY_LIST_FILE, ios::binary | ios::in);

    if (!f)
        return 0;

    int found = 0;
    int cur = doctorSpecialtyIndex.entries[pos].head;
    SpecialtyListNode n;

    while (cur != -1 && found < maxIDs) {
        readSpecialtyNode(f, cur, n);

        memcpy(outIDs[found], n.doctorID, KEY_SIZE);

        found++;
        cur = n.next;
    }

    f.close();
    return found;
}



void buildDoctorSpecialtyIndex(const char* doctorFile) {
    initSpecialtyIndex();
    resetSpecialtyListFile();

    ifstream file(doctorFile, ios::binary);

    if (file) {
        file.seekg(sizeof(int), ios::beg);

        Doctor d;

        while (file.read(reinterpret_cast<char*>(&d), sizeof(Doctor))) {
            if (d.ID[0] != '*')
                insertSpecialtyImpl(d.specialization, d.ID);
        }

        file.close();
    }

    saveSpecialtyIndex();
}



void saveSpecialtyIndex() {
    ofstream f(SPECIALTY_INDEX_FILE, ios::binary | ios::trunc);

    if (!f)
        return;

    f.write(reinterpret_cast<char*>(&doctorSpecialtyIndex.count),
            sizeof(int));

    f.write(reinterpret_cast<char*>(doctorSpecialtyIndex.entries),
            doctorSpecialtyIndex.count * sizeof(SpecialtyIndexEntry));

    f.close();
}



void printSpecialtyIndex() {
    cout << "--- Doctor Specialty secondary index ("
         << doctorSpecialtyIndex.count << " specialties) ---" << endl;

    for (int i = 0; i < doctorSpecialtyIndex.count; i++) {
        cout << "  [";

        for (int k = 0;
             k < SPECIALTY_SIZE &&
             doctorSpecialtyIndex.entries[i].specialty[k] != '\0'; k++)
            cout << doctorSpecialtyIndex.entries[i].specialty[k];

        cout << "] -> head node "
             << doctorSpecialtyIndex.entries[i].head << " : ";

        char ids[MAX_SPECIALTY_INDEX_SIZE][KEY_SIZE];

        int n = searchDoctorSpecialtyIndex(
            doctorSpecialtyIndex.entries[i].specialty,
            ids,
            MAX_SPECIALTY_INDEX_SIZE
        );

        for (int j = 0; j < n; j++) {
            for (int k = 0; k < KEY_SIZE && ids[j][k] != '\0'; k++)
                cout << ids[j][k];

            cout << (j + 1 < n ? ", " : "");
        }

        cout << endl;
    }
}



// ================= APPOINTMENT DOCTOR ID SECONDARY INDEX =================

AppointmentDoctorIndex appointmentDoctorIndex;

static const char* APPOINTMENT_DOCTOR_LIST_FILE =
    "appointment_doctor_list.dat";

static const char* APPOINTMENT_DOCTOR_INDEX_FILE =
    "appointment_doctor_index.dat";

void initAppointmentDoctorIndex() {
    appointmentDoctorIndex.count = 0;
}

// Calculate the byte offset of a node using its RRN
static int appointmentDoctorNodeOffset(int rrn) {
    return static_cast<int>(sizeof(int) + rrn * sizeof(AppointmentDoctorListNode));
}

// Create the list file only if it does not exist
static void createAppointmentDoctorListFile() { ifstream check(APPOINTMENT_DOCTOR_LIST_FILE, ios::binary);

    if (check) {
        check.close();
        return;
    }
    check.close();
    ofstream f(APPOINTMENT_DOCTOR_LIST_FILE, ios::binary);
    int freeHead = -1;
    f.write(reinterpret_cast<char*>(&freeHead), sizeof(int));
}

// Reset the list file when rebuilding the index
static void resetAppointmentDoctorListFile() {
    ofstream f(APPOINTMENT_DOCTOR_LIST_FILE,ios::binary | ios::trunc);
    int freeHead = -1;
    f.write(reinterpret_cast<char*>(&freeHead), sizeof(int));
}

// Read a node using its RRN
static void readAppointmentDoctorNode(fstream& f, int rrn, AppointmentDoctorListNode& n) {
    f.clear();
    f.seekg(appointmentDoctorNodeOffset(rrn), ios::beg);

    f.read(reinterpret_cast<char*>(&n), sizeof(AppointmentDoctorListNode));
}

// Write a node using its RRN
static void writeAppointmentDoctorNode(fstream& f,int rrn,const AppointmentDoctorListNode& n) {
    f.clear();
    f.seekp(appointmentDoctorNodeOffset(rrn), ios::beg);

    f.write(reinterpret_cast<const char*>(&n), sizeof(AppointmentDoctorListNode));
}

// Allocate a new node or reuse a deleted node
static int allocateAppointmentDoctorNode(fstream& f,const char* appointmentID,int next) {
    AppointmentDoctorListNode n;

    memset(&n, 0, sizeof(AppointmentDoctorListNode));

    strncpy(n.appointmentID, appointmentID, KEY_SIZE);
    n.next = next;

    int freeHead = -1;

    f.clear();
    f.seekg(0, ios::beg);
    f.read(reinterpret_cast<char*>(&freeHead), sizeof(int));

    int rrn;

    if (freeHead != -1) {

        AppointmentDoctorListNode deleted;
        readAppointmentDoctorNode(f, freeHead, deleted);
        rrn = freeHead;
        int newFreeHead = deleted.next;
        f.clear();
        f.seekp(0, ios::beg);

        f.write(reinterpret_cast<char*>(&newFreeHead),sizeof(int));
    }
    else {

        f.clear();
        f.seekp(0, ios::end);

        int endOffset = static_cast<int>(f.tellp());

        rrn = (endOffset - static_cast<int>(sizeof(int))) /
              static_cast<int>(sizeof(AppointmentDoctorListNode));
    }

    writeAppointmentDoctorNode(f, rrn, n);

    return rrn;
}

// Return a deleted node to the AVAIL LIST
static void freeAppointmentDoctorNode(fstream& f, int rrn) {

    int freeHead = -1;

    f.clear();
    f.seekg(0, ios::beg);

    f.read(reinterpret_cast<char*>(&freeHead), sizeof(int));

    AppointmentDoctorListNode n;

    memset(&n, 0, sizeof(AppointmentDoctorListNode));

    n.appointmentID[0] = '*';
    n.next = freeHead;

    writeAppointmentDoctorNode(f, rrn, n);

    f.clear();
    f.seekp(0, ios::beg);

    f.write(reinterpret_cast<char*>(&rrn), sizeof(int));
}

// Find the correct sorted position of a Doctor ID
static int findAppointmentDoctorPosition(const char* doctorID) {

    int low = 0;
    int high = appointmentDoctorIndex.count;

    while (low < high) {

        int mid = (low + high) / 2;

        if (strncmp(
                appointmentDoctorIndex.entries[mid].doctorID,
                doctorID,
                KEY_SIZE
            ) < 0) {

            low = mid + 1;
        }
        else {
            high = mid;
        }
    }

    return low;
}


// Binary search for a Doctor ID
int binarySearchAppointmentDoctor(const char* doctorID) {

    int pos = findAppointmentDoctorPosition(doctorID);

    if (pos < appointmentDoctorIndex.count &&strncmp(appointmentDoctorIndex.entries[pos].doctorID,doctorID,KEY_SIZE) == 0)
    {
        return pos;
    }

    return -1;
}

// Internal insertion function
static bool insertAppointmentDoctorImpl(const char* doctorID,const char* appointmentID) {

    fstream f(
        APPOINTMENT_DOCTOR_LIST_FILE,
        ios::binary | ios::in | ios::out
    );

    if (!f) {

        createAppointmentDoctorListFile();

        f.open(
            APPOINTMENT_DOCTOR_LIST_FILE,
            ios::binary | ios::in | ios::out
        );

        if (!f)
            return false;
    }

    int pos = findAppointmentDoctorPosition(doctorID);

    bool exists =
        pos < appointmentDoctorIndex.count &&
        strncmp(
            appointmentDoctorIndex.entries[pos].doctorID,
            doctorID,
            KEY_SIZE
        ) == 0;

    if (exists) {

        int rrn = allocateAppointmentDoctorNode(
            f,
            appointmentID,
            appointmentDoctorIndex.entries[pos].head
        );

        appointmentDoctorIndex.entries[pos].head = rrn;
    }
    else {

        if (appointmentDoctorIndex.count >=
            MAX_APPOINTMENT_DOCTOR_INDEX_SIZE) {

            f.close();
            return false;
        }

        int rrn = allocateAppointmentDoctorNode(
            f,
            appointmentID,
            -1
        );

        for (int i = appointmentDoctorIndex.count; i > pos; i--) {

            appointmentDoctorIndex.entries[i] =
                appointmentDoctorIndex.entries[i - 1];
        }

        memset(
            appointmentDoctorIndex.entries[pos].doctorID,
            '\0',
            KEY_SIZE
        );

        strncpy(
            appointmentDoctorIndex.entries[pos].doctorID,
            doctorID,
            KEY_SIZE
        );

        appointmentDoctorIndex.entries[pos].head = rrn;

        appointmentDoctorIndex.count++;
    }

    f.close();

    return true;
}


// Public insertion function
bool insertAppointmentDoctorIndex(const char* doctorID,const char* appointmentID) {

    if (!insertAppointmentDoctorImpl(doctorID, appointmentID))
        return false;

    saveAppointmentDoctorIndex();

    return true;
}


// Delete an appointment from a Doctor ID linked list
bool deleteAppointmentDoctorIndex(const char* doctorID,const char* appointmentID) {

    int pos = binarySearchAppointmentDoctor(doctorID);

    if (pos == -1)
        return false;

    fstream f(
        APPOINTMENT_DOCTOR_LIST_FILE,
        ios::binary | ios::in | ios::out
    );

    if (!f)
        return false;

    int prev = -1;

    int cur = appointmentDoctorIndex.entries[pos].head;

    AppointmentDoctorListNode n;

    while (cur != -1) {

        readAppointmentDoctorNode(f, cur, n);

        if (strncmp(
                n.appointmentID,
                appointmentID,
                KEY_SIZE
            ) == 0) {

            break;
        }

        prev = cur;
        cur = n.next;
    }

    if (cur == -1) {
        f.close();
        return false;
    }

    if (prev == -1) {

        appointmentDoctorIndex.entries[pos].head = n.next;
    }
    else {

        AppointmentDoctorListNode p;

        readAppointmentDoctorNode(f, prev, p);

        p.next = n.next;

        writeAppointmentDoctorNode(f, prev, p);
    }

    freeAppointmentDoctorNode(f, cur);

    // Remove the Doctor ID entry if its list becomes empty
    if (appointmentDoctorIndex.entries[pos].head == -1) {

        for (int i = pos;
             i < appointmentDoctorIndex.count - 1;
             i++) {

            appointmentDoctorIndex.entries[i] =
                appointmentDoctorIndex.entries[i + 1];
        }

        appointmentDoctorIndex.count--;
    }

    f.close();

    saveAppointmentDoctorIndex();

    return true;
}


// Search for all appointments belonging to a Doctor ID
int searchAppointmentsByDoctorID(const char* doctorID,char outIDs[][KEY_SIZE],int maxIDs) {

    int pos = binarySearchAppointmentDoctor(doctorID);

    if (pos == -1)
        return 0;

    fstream f(
        APPOINTMENT_DOCTOR_LIST_FILE,
        ios::binary | ios::in
    );

    if (!f)
        return 0;

    int found = 0;

    int cur = appointmentDoctorIndex.entries[pos].head;

    AppointmentDoctorListNode n;

    while (cur != -1 && found < maxIDs) {

        readAppointmentDoctorNode(f, cur, n);

        memcpy(outIDs[found], n.appointmentID, KEY_SIZE);

        found++;

        cur = n.next;
    }

    f.close();

    return found;
}


// Build the Appointment Secondary Index from app.dat
void buildAppointmentDoctorIndex(const char* appointmentFile) {

    initAppointmentDoctorIndex();

    resetAppointmentDoctorListFile();

    ifstream file(appointmentFile, ios::binary);

    if (file) {

        file.seekg(sizeof(int), ios::beg);

        Appointment a;

        while (file.read(
            reinterpret_cast<char*>(&a),
            sizeof(Appointment)
        )) {

            if (a.ID[0] != '*') {

                insertAppointmentDoctorImpl(
                    a.doctor_id,
                    a.ID
                );
            }
        }

        file.close();
    }

    saveAppointmentDoctorIndex();
}

// Save the sorted index to a binary file
void saveAppointmentDoctorIndex() {

    ofstream f(
        APPOINTMENT_DOCTOR_INDEX_FILE,
        ios::binary | ios::trunc
    );

    if (!f)
        return;

    f.write(
        reinterpret_cast<char*>(&appointmentDoctorIndex.count),
        sizeof(int)
    );

    f.write(
        reinterpret_cast<char*>(appointmentDoctorIndex.entries),
        appointmentDoctorIndex.count *
        sizeof(AppointmentDoctorIndexEntry)
    );

    f.close();
}

// Print the Appointment Secondary Index
void printAppointmentDoctorIndex() {

    cout << "--- Appointment Doctor ID secondary index ("<< appointmentDoctorIndex.count << " doctors) ---" << endl;

    for (int i = 0;i < appointmentDoctorIndex.count; i++) {
        cout << "  [";
        for (int k = 0; k < KEY_SIZE && appointmentDoctorIndex.entries[i].doctorID[k] != '\0';k++) {

            cout << appointmentDoctorIndex.entries[i].doctorID[k];
        }

        cout << "] -> head node "
             << appointmentDoctorIndex.entries[i].head
             << " : ";

        char ids[MAX_APPOINTMENT_DOCTOR_INDEX_SIZE][KEY_SIZE];

        int n = searchAppointmentsByDoctorID(
            appointmentDoctorIndex.entries[i].doctorID,
            ids,
            MAX_APPOINTMENT_DOCTOR_INDEX_SIZE
        );

        for (int j = 0; j < n; j++) {

            for (int k = 0;
                 k < KEY_SIZE && ids[j][k] != '\0';
                 k++) {

                cout << ids[j][k];
            }

            cout << (j + 1 < n ? ", " : "");
        }

        cout << endl;
    }
}

