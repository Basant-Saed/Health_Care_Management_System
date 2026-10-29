#include "doctorManager.h"
#include "appointmentManager.h"
#include "structs.h"
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

        fill(d.ID, d.ID + 15, '\0'); // first we fill the array with null characters to avoid garbage values
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

        int availhead = -1;
        doctor.seekg(0, ios::beg);
        doctor.read(reinterpret_cast<char*>(&availhead), sizeof(int));

        if (availhead != -1) {
            doctor.seekg(availhead + 1, ios::beg);      // availhead contains the flag so we access the byte after it
            int nextavail = -1;
            doctor.read(reinterpret_cast<char*>(&nextavail), sizeof(int));

            doctor.seekp(availhead, ios::beg);
            doctor.write(reinterpret_cast<char*>(&d), sizeof(Doctor));

            doctor.seekp(0, ios::beg);
            doctor.write(reinterpret_cast<char*>(&nextavail), sizeof(int));

            cout << "Doctor added to reused slot at offset: " << availhead << endl;

        } else {
            doctor.seekp(0, ios::end);
            doctor.write(reinterpret_cast<char*>(&d), sizeof(Doctor));
            cout << " Doctor added at end of file." << endl;

        }

        cout << "Do you want to add more doctors? (y/n): ";
        cin >> choice;

    } while (choice == 'y' || choice == 'Y');

    doctor.close();
}

bool delete_doctor(fstream& doctor, const char* doctorID) {
    if (!doctor) return false;
    doctor.seekg(4, ios::beg);                              // skip header 
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
    cout << "Doctor deleted successfully! Avail List updated." << endl;
    return true;
}

bool update_doctor(fstream& doctor, const char* oldID, const Doctor& updatedDoctor) { 
   
}