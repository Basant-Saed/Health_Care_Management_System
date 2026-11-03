#include <iostream>
#include <fstream>
#include <cstring>
#include "structs.h"
#include "doctorManager.h"
#include "appointmentManager.h"
#include "indexManager.h"

using namespace std;

static int passed = 0;
static int failed = 0;

// Prints PASS/FAIL for one check and counts it
static void check(bool condition, const char* description) {
    if (condition) {
        cout << "  [PASS] " << description << endl;
        passed++;
    }
    else {
        cout << "  [FAIL] " << description << endl;
        failed++;
    }
}

// Prints a char array without reading past its size (keys are not guaranteed to end with '\0')
static void printField(const char* s, int size) {
    for (int i = 0; i < size && s[i] != '\0'; i++)
        cout << s[i];
}

// Reads a key (max 14 characters) from the keyboard
static void readKey(const char* prompt, char* key) {
    memset(key, 0, KEY_SIZE);
    cout << prompt;
    cin >> ws;
    cin.getline(key, KEY_SIZE);
    if (cin.fail()) {                 // input was longer than the key size
        cin.clear();
        cin.ignore(1000, '\n');
    }
}

static bool isSortedAscending(const PrimaryIndex& idx) {
    for (int i = 1; i < idx.count; i++)
        if (strncmp(idx.entries[i - 1].key, idx.entries[i].key, KEY_SIZE) >= 0)   // >= also catches duplicates
            return false;
    return true;
}

static void printIndex(const PrimaryIndex& idx, const char* title) {
    cout << "--- " << title << " (" << idx.count << " entries) ---" << endl;
    for (int i = 0; i < idx.count; i++) {
        cout << "  " << i << ": [";
        printField(idx.entries[i].key, KEY_SIZE);
        cout << "] -> offset " << idx.entries[i].offset << endl;
    }
}

// ---------- Search ----------
static void searchDoctorTest() {
    char id[KEY_SIZE];
    readKey("Doctor ID to search: ", id);

    int pos = binarySearchPrimary(doctorPrimaryIndex, id);
    if (pos == -1) {
        cout << "Not found in the doctor index." << endl;
        return;
    }
    int offset = doctorPrimaryIndex.entries[pos].offset;
    cout << "Found at index position " << pos << ", file offset " << offset << endl;

    ifstream f("doctor.dat", ios::binary);         // read the record straight from the file using the offset
    Doctor d;
    f.seekg(offset, ios::beg);
    if (f.read(reinterpret_cast<char*>(&d), sizeof(Doctor))) {
        cout << "  ID: ";             printField(d.ID, 15);
        cout << " | Name: ";          printField(d.name, 30);
        cout << " | Specialty: ";     printField(d.specialization, 20);
        cout << " | Address: ";       printField(d.address, 30);
        cout << endl;
    }
}

static void searchAppointmentTest() {
    char id[KEY_SIZE];
    readKey("Appointment ID to search: ", id);

    int pos = binarySearchPrimary(appointmentPrimaryIndex, id);
    if (pos == -1) {
        cout << "Not found in the appointment index." << endl;
        return;
    }
    int offset = appointmentPrimaryIndex.entries[pos].offset;
    cout << "Found at index position " << pos << ", file offset " << offset << endl;

    ifstream f("app.dat", ios::binary);
    Appointment a;
    f.seekg(offset, ios::beg);
    if (f.read(reinterpret_cast<char*>(&a), sizeof(Appointment))) {
        cout << "  ID: ";          printField(a.ID, 15);
        cout << " | Date: ";       printField(a.date, 30);
        cout << " | Doctor ID: ";  printField(a.doctor_id, 15);
        cout << endl;
    }
}

// ---------- Delete ----------
static void deleteDoctorTest() {
    char id[KEY_SIZE];
    readKey("Doctor ID to delete: ", id);
    fstream f("doctor.dat", ios::binary | ios::in | ios::out);
    delete_doctor(f, id);                          // prints its own success / not-found message
}

static void deleteAppointmentTest() {
    char id[KEY_SIZE];
    readKey("Appointment ID to delete: ", id);
    fstream f("app.dat", ios::binary | ios::in | ios::out);
    delete_appointment(f, id);
}

// ---------- Verify RAM index against the file ----------
static bool verifyIndex(const PrimaryIndex& idx, const char* filename, int recordSize, const char* label) {
    cout << "Verifying " << label << "..." << endl;
    bool ok = true;

    if (!isSortedAscending(idx)) {
        cout << "  [FAIL] index is not strictly ascending (unsorted or duplicate keys)" << endl;
        ok = false;
    }

    ifstream f(filename, ios::binary);
    if (!f) {
        cout << "  [FAIL] cannot open " << filename << endl;
        return false;
    }

    char buf[128];                                 // big enough for both record types

    // every index entry must point to a live record with the same key
    for (int i = 0; i < idx.count; i++) {
        f.clear();
        f.seekg(idx.entries[i].offset, ios::beg);
        if (!f.read(buf, recordSize) || buf[0] == '*' ||
            strncmp(buf, idx.entries[i].key, KEY_SIZE) != 0) {
            cout << "  [FAIL] entry " << i << " does not match the file record at offset "
                << idx.entries[i].offset << endl;
            ok = false;
        }
    }

    // the number of live records in the file must equal the number of index entries
    f.clear();
    f.seekg(sizeof(int), ios::beg);
    int live = 0;
    while (f.read(buf, recordSize))
        if (buf[0] != '*')
            live++;

    if (live != idx.count) {
        cout << "  [FAIL] file has " << live << " live records but index has " << idx.count << " entries" << endl;
        ok = false;
    }

    if (ok)
        cout << "  [PASS] " << label << " is sorted and in sync with the file (" << live << " live records)" << endl;
    return ok;
}

// ---------- Index-only self test (no files used) ----------
static void selfTest() {
    passed = 0;
    failed = 0;
    cout << "Running index-only self test..." << endl;

    PrimaryIndex t;
    initPrimaryIndex(t);

    const char* keys[] = { "D5", "D2", "D9", "D1", "D7" };    // deliberately unsorted
    for (int i = 0; i < 5; i++)
        insertPrimaryIndex(t, keys[i], 4 + i * 95);

    check(t.count == 5, "5 keys inserted");
    check(isSortedAscending(t), "index is sorted ascending after unsorted inserts");
    check(strncmp(t.entries[0].key, "D1", KEY_SIZE) == 0 &&
        strncmp(t.entries[4].key, "D9", KEY_SIZE) == 0, "smallest key first, largest key last");

    check(searchPrimaryIndex(t, "D7") == 4 + 4 * 95, "binary search finds D7 with the correct offset");
    check(searchPrimaryIndex(t, "D3") == -1, "binary search returns -1 for a missing key");

    check(insertPrimaryIndex(t, "D5", 999) == false, "duplicate key D5 is rejected");
    check(t.count == 5, "count unchanged after duplicate insert");
    check(searchPrimaryIndex(t, "D5") == 4, "original offset of D5 is unchanged");

    check(deletePrimaryIndex(t, "D5") == true, "delete middle key D5");
    check(t.count == 4 && searchPrimaryIndex(t, "D5") == -1, "D5 is gone and count decreased");
    check(isSortedAscending(t), "index still sorted after delete (shift left)");

    check(deletePrimaryIndex(t, "D1") == true, "delete first key D1");
    check(deletePrimaryIndex(t, "D9") == true, "delete last key D9");
    check(deletePrimaryIndex(t, "D9") == false, "deleting a missing key returns false");
    check(t.count == 2 && isSortedAscending(t), "2 keys left and still sorted");

    check(insertPrimaryIndex(t, "D3", 500) == true, "re-insert a new key D3");
    check(isSortedAscending(t) && searchPrimaryIndex(t, "D3") == 500, "D3 inserted in the correct sorted position");

    cout << "Result: " << passed << " passed, " << failed << " failed." << endl;
}

int main() {
    initializeDoctorFile();
    initializeAppointment();
    buildDoctorPrimaryIndex("doctor.dat");
    buildAppointmentPrimaryIndex("app.dat");

    int choice = -1;
    while (choice != 0) {
        cout << endl;
        cout << "===== Primary Index Test Menu =====" << endl;
        cout << " 1. Add doctor (try the same ID twice to test duplicates)" << endl;
        cout << " 2. Add appointment (try the same ID twice to test duplicates)" << endl;
        cout << " 3. Delete doctor" << endl;
        cout << " 4. Delete appointment" << endl;
        cout << " 5. Search doctor by ID (index -> file)" << endl;
        cout << " 6. Search appointment by ID (index -> file)" << endl;
        cout << " 7. Print both RAM indexes" << endl;
        cout << " 8. Clear RAM indexes (simulate closing the program)" << endl;
        cout << " 9. Rebuild RAM indexes from the files" << endl;
        cout << "10. Verify RAM indexes against the files" << endl;
        cout << "11. Index-only self test (no files)" << endl;
        cout << " 0. Exit" << endl;
        cout << "Choice: ";

        if (!(cin >> choice)) {                    // not a number
            cin.clear();
            cin.ignore(1000, '\n');
            choice = -1;
            continue;
        }

        switch (choice) {
        case 1:  add_doctor(); break;
        case 2:  add_appointment(); break;
        case 3:  deleteDoctorTest(); break;
        case 4:  deleteAppointmentTest(); break;
        case 5:  searchDoctorTest(); break;
        case 6:  searchAppointmentTest(); break;
        case 7:
            printIndex(doctorPrimaryIndex, "Doctor primary index");
            printIndex(appointmentPrimaryIndex, "Appointment primary index");
            break;
        case 8:
            initPrimaryIndex(doctorPrimaryIndex);
            initPrimaryIndex(appointmentPrimaryIndex);
            cout << "RAM indexes cleared (files untouched). Searches will fail until you rebuild (option 9)." << endl;
            break;
        case 9:
            buildDoctorPrimaryIndex("doctor.dat");
            buildAppointmentPrimaryIndex("app.dat");
            cout << "Rebuilt: " << doctorPrimaryIndex.count << " doctors, "
                << appointmentPrimaryIndex.count << " appointments." << endl;
            break;
        case 10:
            verifyIndex(doctorPrimaryIndex, "doctor.dat", sizeof(Doctor), "Doctor index");
            verifyIndex(appointmentPrimaryIndex, "app.dat", sizeof(Appointment), "Appointment index");
            break;
        case 11: selfTest(); break;
        case 0:  cout << "Bye." << endl; break;
        default: cout << "Invalid choice." << endl;
        }
    }
    return 0;
}