#include "doctorManager.h"
#include "appointmentManager.h"
#include "structs.h"
#include "indexManager.h"
#include "secondaryIndex.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <algorithm>

using namespace std;

void initializeDoctorFile() {
    ifstream check("doctor.dat", ios::binary); 
    if (!check) {
        ofstream doctor("doctor.dat", ios::binary);
        int availHead = -1;
        doctor.write(reinterpret_cast<char*>(&availHead), sizeof(int));
    }
}

void add_doctor() {
    fstream doctor("doctor.dat", ios::binary | ios::in | ios::out);
    if (!doctor) {
        cout << "Error opening doctor.dat file." << endl;
        return;
    }
    
    doctor.seekp(0, ios::end);
    char choice;

    do {
        Doctor d;

        fill(d.ID, d.ID + 15, '\0'); 
        cout << "Enter Doctor ID: ";
        cin >> ws >> d.ID;

        fill(d.name, d.name + 30, '\0');
        cout << "Enter Doctor Name: ";
        cin >> ws;
        cin.getline(d.name, 30);

        fill(d.specialization, d.specialization + 20, '\0');
        cout << "Enter Doctor Specialization: ";
        cin >> ws;
        cin.getline(d.specialization, 20);
        
        fill(d.address, d.address + 30, '\0');
        cout << "Enter Doctor Address: ";
        cin >> ws;
        cin.getline(d.address, 30);

        if (searchPrimaryIndex(doctorPrimaryIndex, d.ID) != -1) {
            cout << "Error: Doctor ID already exists. Record not added." << endl;       
        }
        else if (doctorPrimaryIndex.count >= MAX_INDEX_SIZE) {                          
            cout << "Error: Doctor index is full. Record not added." << endl;
        }
        else {
            int availhead = -1;
            doctor.seekg(0, ios::beg);
            doctor.read(reinterpret_cast<char*>(&availhead), sizeof(int));

            if (availhead != -1) {
                doctor.seekg(availhead + 1, ios::beg);
                int nextavail = -1;
                doctor.read(reinterpret_cast<char*>(&nextavail), sizeof(int));

                doctor.seekp(availhead, ios::beg);
                doctor.write(reinterpret_cast<char*>(&d), sizeof(Doctor));

                doctor.seekp(0, ios::beg);
                doctor.write(reinterpret_cast<char*>(&nextavail), sizeof(int));
                insertPrimaryIndex(doctorPrimaryIndex, d.ID, availhead);
                insertDoctorNameIndex(d.name, d.ID);
                cout << "Doctor added to reused slot at offset: " << availhead << endl;

            }
            else {
                doctor.seekp(0, ios::end);
                int newOffset = static_cast<int>(doctor.tellp());
                doctor.write(reinterpret_cast<char*>(&d), sizeof(Doctor));
                insertPrimaryIndex(doctorPrimaryIndex, d.ID, newOffset);
                insertDoctorNameIndex(d.name, d.ID);
                cout << " Doctor added at end of file." << endl;

            }
        }
        cout << "Do you want to add more doctors? (y/n): ";
        cin >> choice;

    } while (choice == 'y' || choice == 'Y');

    doctor.close();
}

bool delete_doctor(fstream& doctor, const char* doctorID) {
    if (!doctor) return false;
    doctor.seekg(4, ios::beg);                             
    Doctor d;
    streampos offset = doctor.tellg();
    bool found = false;

    while (doctor.read(reinterpret_cast<char*>(&d), sizeof(Doctor))) {
        if (d.ID[0] != '*' && strcmp(d.ID, doctorID) == 0) {
            found = true;
            break;
        }

        offset = doctor.tellg();
    }
    
    if (!found) {
		doctor.clear(); 
        cout << "Error: Doctor ID not found!" << endl;
        return false;
    }

    int avail = -1;
    doctor.seekg(0, ios::beg);
    doctor.read(reinterpret_cast<char*>(&avail), sizeof(int));
    doctor.seekp(offset, ios::beg);
    char mark = '*';
    doctor.write(&mark, sizeof(char));
    doctor.write(reinterpret_cast<char*>(&avail), sizeof(int));

    int newavail = static_cast<int>(offset);
    doctor.seekp(0, ios::beg);
    doctor.write(reinterpret_cast<char*>(&newavail), sizeof(int));
	deletePrimaryIndex(doctorPrimaryIndex, doctorID);
	deleteDoctorNameIndex(d.name, d.ID);
    cout << "Doctor deleted successfully! Avail List updated." << endl;
    return true;
}

bool update_doctor(fstream& doctor, const char* oldID, const Doctor& updatedDoctor) { 
	if (!doctor) return false;

    int pos = binarySearchPrimary(doctorPrimaryIndex, oldID);
    if (pos == -1) {
        cout << "Error: Doctor ID not found!" << endl;
        return false;
    }
    int offset = doctorPrimaryIndex.entries[pos].offset;

    if (updatedDoctor.ID[0] == '\0' || updatedDoctor.ID[0] == '*') {
        cout << "Error: Doctor ID cannot be empty or start with '*'." << endl;
        return false;
    }
    if (updatedDoctor.name[0] == '\0') {
        cout << "Error: Doctor name cannot be empty." << endl;
        return false;
    }

    bool idChanged = strncmp(oldID, updatedDoctor.ID, KEY_SIZE) != 0;
    if (idChanged && searchPrimaryIndex(doctorPrimaryIndex, updatedDoctor.ID) != -1) {
        cout << "Error: new Doctor ID already exists. Nothing updated." << endl;
        return false;
    }

    Doctor oldDoc;
    doctor.clear();
    doctor.seekg(offset, ios::beg);
    if (!doctor.read(reinterpret_cast<char*>(&oldDoc), sizeof(Doctor))) {
        doctor.clear();
        cout << "Error: could not read the doctor record." << endl;
        return false;
    }

    if (strncmp(oldDoc.name, updatedDoctor.name, NAME_SIZE) != 0) {
        if (!updateDoctorNameIndex(oldDoc.name, updatedDoctor.name, oldID)) {
            cout << "Error: could not update the Doctor Name index. Nothing updated." << endl;
            return false;
        }
    }

    if (idChanged) {
        updatePrimaryKey(doctorPrimaryIndex, oldID, updatedDoctor.ID);
        renameDoctorIDInNameIndex(updatedDoctor.name, oldID, updatedDoctor.ID);
    }
    doctor.clear();
    doctor.seekp(offset, ios::beg);
    doctor.write(reinterpret_cast<const char*>(&updatedDoctor), sizeof(Doctor));
    doctor.flush();

    cout << "Doctor updated successfully!" << endl;
    return true;
}

static void readDoctorKey(const char* prompt, char* key) {
    memset(key, 0, KEY_SIZE);
    cout << prompt;
    cin >> ws;
    cin.getline(key, KEY_SIZE);
    if (cin.fail()) {
        cin.clear();
        cin.ignore(1000, '\n');
    }
}

static bool loadDoctor(fstream& doctor, const char* id, Doctor& d) {
    int offset = searchPrimaryIndex(doctorPrimaryIndex, id);
    if (offset == -1) {
        cout << "Error: Doctor ID not found!" << endl;
        return false;
    }
    doctor.clear();
    doctor.seekg(offset, ios::beg);
    return static_cast<bool>(doctor.read(reinterpret_cast<char*>(&d), sizeof(Doctor)));
}

void update_doctor_name() {
    fstream doctor("doctor.dat", ios::binary | ios::in | ios::out);
    if (!doctor) {
        cout << "Error opening doctor.dat file." << endl;
        return;
    }

    char id[KEY_SIZE];
    readDoctorKey("Enter Doctor ID: ", id);

    Doctor d;
    if (!loadDoctor(doctor, id, d)) return;

    fill(d.name, d.name + 30, '\0');
    cout << "Enter new Doctor Name: ";
    cin >> ws;
    cin.getline(d.name, 30);
    if (cin.fail()) {
        cin.clear();
        cin.ignore(1000, '\n');
    }

    update_doctor(doctor, id, d);
    doctor.close();
}

void update_doctor_id() {
    fstream doctor("doctor.dat", ios::binary | ios::in | ios::out);
    if (!doctor) {
        cout << "Error opening doctor.dat file." << endl;
        return;
    }

    char oldID[KEY_SIZE];
    readDoctorKey("Enter current Doctor ID: ", oldID);

    Doctor d;
    if (!loadDoctor(doctor, oldID, d)) return;

    char newID[KEY_SIZE];
    readDoctorKey("Enter new Doctor ID: ", newID);

    fill(d.ID, d.ID + 15, '\0');
    strncpy(d.ID, newID, KEY_SIZE);

    update_doctor(doctor, oldID, d);
    doctor.close();
}