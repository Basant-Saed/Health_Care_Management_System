#include "appointmentManager.h"
#include "doctorManager.h"
#include "indexManager.h"
#include "structs.h"
#include "secondaryIndex.h"
#include <iostream>
#include <cstring>
#include <fstream>

using namespace std;

void initializeAppointment() {
    ifstream check("app.dat", ios::binary);
    if (!check) {
        ofstream app("app.dat", ios::binary);
        int availHead = -1;
        app.write(reinterpret_cast<char*>(&availHead), sizeof(int));
    }
}

static void readKey(const char* prompt, char* key, int size) {
    memset(key, 0, size);
    cout << prompt;
    cin >> ws;
    cin.getline(key, size);
    if (cin.fail()) {
        cin.clear();
        cin.ignore(1000, '\n');
    }
}

void add_appointment() {
    fstream app("app.dat", ios::binary | ios::in | ios::out);
    if (!app) {
        cout << "Error opening app.dat file." << endl;
        return;
    }

    char choice;
    do {
        Appointment a;
        memset(&a, 0, sizeof(Appointment));

        cout << "Enter Appointment ID: ";
        cin >> ws >> a.ID;
        readKey("Enter Appointment Date: ", a.date, 30);
        readKey("Enter Doctor ID for the Appointment: ", a.doctor_id, 15);

        if (searchPrimaryIndex(appointmentPrimaryIndex, a.ID) != -1) {
            cout << "Error: Appointment ID already exists. Record not added." << endl;
        } else if (appointmentPrimaryIndex.count >= MAX_INDEX_SIZE) {
            cout << "Error: Appointment index is full. Record not added." << endl;
        } else {
            int availhead = -1;
            app.seekg(0, ios::beg);
            app.read(reinterpret_cast<char*>(&availhead), sizeof(int));

            if (availhead != -1) {
                app.seekg(availhead + 1, ios::beg);
                int nextavail = -1;
                app.read(reinterpret_cast<char*>(&nextavail), sizeof(int));

                app.seekp(availhead, ios::beg);
                app.write(reinterpret_cast<char*>(&a), sizeof(Appointment));

                app.seekp(0, ios::beg);
                app.write(reinterpret_cast<char*>(&nextavail), sizeof(int));

                insertPrimaryIndex(appointmentPrimaryIndex, a.ID, availhead);
                insertAppointmentDoctorIndex(a.doctor_id, a.ID);
                cout << "Appointment added to reused slot at offset: " << availhead << endl;
            } else {
                app.seekp(0, ios::end);
                int newOffset = static_cast<int>(app.tellp());
                app.write(reinterpret_cast<char*>(&a), sizeof(Appointment));

                insertPrimaryIndex(appointmentPrimaryIndex, a.ID, newOffset);
                insertAppointmentDoctorIndex(a.doctor_id, a.ID);
                cout << " Appointment added at end of file." << endl;
            }
        }
        cout << "Do you want to add more appointments? (y/n): ";
        cin >> choice;
    } while (choice == 'y' || choice == 'Y');

    app.close();
}

bool delete_appointment(fstream& app, const char* appID) {
    if (!app) return false;
    app.seekg(4, ios::beg);
    Appointment a;
    streampos offset = app.tellg();
    bool found = false;

    while (app.read(reinterpret_cast<char*>(&a), sizeof(Appointment))) {
        if (a.ID[0] != '*' && strcmp(a.ID, appID) == 0) {
            found = true;
            break;
        }
        offset = app.tellg();
    }

    if (!found) {
        app.clear();
        cout << "Error: Appointment ID not found!" << endl;
        return false;
    }

    int avail = -1;
    app.seekg(0, ios::beg);
    app.read(reinterpret_cast<char*>(&avail), sizeof(int));

    app.seekp(offset, ios::beg);
    char mark = '*';
    app.write(&mark, sizeof(char));
    app.write(reinterpret_cast<char*>(&avail), sizeof(int));

    int newavail = static_cast<int>(offset);
    app.seekp(0, ios::beg);
    app.write(reinterpret_cast<char*>(&newavail), sizeof(int));

    deletePrimaryIndex(appointmentPrimaryIndex, appID);
    deleteAppointmentDoctorIndex(a.doctor_id, a.ID);

    cout << "Appointment deleted successfully! Avail List updated." << endl;
    return true;
}

bool update_appointment(fstream& app, const char* oldID, const Appointment& updatedApp) {
    if (!app) return false;

    int pos = binarySearchPrimary(appointmentPrimaryIndex, oldID);
    if (pos == -1) {
        cout << "Error: Appointment ID not found!" << endl;
        return false;
    }
    int offset = appointmentPrimaryIndex.entries[pos].offset;

    if (updatedApp.ID[0] == '\0' || updatedApp.ID[0] == '*') {
        cout << "Error: Appointment ID cannot be empty or start with '*'." << endl;
        return false;
    }
    if (updatedApp.date[0] == '\0') {
        cout << "Error: Appointment date cannot be empty." << endl;
        return false;
    }
    if (updatedApp.doctor_id[0] == '\0' || updatedApp.doctor_id[0] == '*') {
        cout << "Error: Doctor ID cannot be empty or start with '*'." << endl;
        return false;
    }

    bool idChanged = strncmp(oldID, updatedApp.ID, KEY_SIZE) != 0;
    if (idChanged && searchPrimaryIndex(appointmentPrimaryIndex, updatedApp.ID) != -1) {
        cout << "Error: new Appointment ID already exists. Nothing updated." << endl;
        return false;
    }

    Appointment oldApp;
    app.clear();
    app.seekg(offset, ios::beg);
    if (!app.read(reinterpret_cast<char*>(&oldApp), sizeof(Appointment))) {
        cout << "Error: could not read the appointment record." << endl;
        return false;
    }

    bool doctorChanged = strncmp(oldApp.doctor_id, updatedApp.doctor_id, KEY_SIZE) != 0;
    bool indexChanged = idChanged || doctorChanged;

    if (indexChanged) {
        if (!deleteAppointmentDoctorIndex(oldApp.doctor_id, oldApp.ID)) {
            cout << "Error: could not delete old Appointment Secondary Index entry." << endl;
            return false;
        }
        if (!insertAppointmentDoctorIndex(updatedApp.doctor_id, updatedApp.ID)) {
            cout << "Error: could not insert new Appointment Secondary Index entry." << endl;
            insertAppointmentDoctorIndex(oldApp.doctor_id, oldApp.ID);
            return false;
        }
    }

    if (idChanged) {
        if (!updatePrimaryKey(appointmentPrimaryIndex, oldID, updatedApp.ID)) {
            cout << "Error: could not update Appointment Primary Index." << endl;
            if (indexChanged) {
                deleteAppointmentDoctorIndex(updatedApp.doctor_id, updatedApp.ID);
                insertAppointmentDoctorIndex(oldApp.doctor_id, oldApp.ID);
            }
            return false;
        }
    }

    app.clear();
    app.seekp(offset, ios::beg);
    app.write(reinterpret_cast<const char*>(&updatedApp), sizeof(Appointment));
    app.flush();

    if (!app) {
        cout << "Error: could not write updated appointment." << endl;
        return false;
    }

    cout << "Appointment updated successfully!" << endl;
    return true;
}

void update_appointment_date() {
    fstream app("app.dat", ios::binary | ios::in | ios::out);
    if (!app) {
        cout << "Error opening app.dat file." << endl;
        return;
    }

    char id[KEY_SIZE];
    readKey("Enter Appointment ID: ", id, KEY_SIZE);

    int offset = searchPrimaryIndex(appointmentPrimaryIndex, id);
    if (offset == -1) {
        cout << "Error: Appointment ID not found!" << endl;
        app.close();
        return;
    }

    Appointment a;
    app.seekg(offset, ios::beg);
    if (!app.read(reinterpret_cast<char*>(&a), sizeof(Appointment))) {
        cout << "Error: could not read the appointment record." << endl;
        app.close();
        return;
    }

    readKey("Enter new Appointment Date: ", a.date, 30);
    update_appointment(app, id, a);
    app.close();
}
