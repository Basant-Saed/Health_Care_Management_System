#ifndef secondaryIndex_h
#define secondaryIndex_h
#include "structs.h"
#include "indexManager.h"
using namespace std;

const int NAME_SIZE = 30;
const int SPECIALTY_SIZE = 20;
const int MAX_SECONDARY_SIZE = 1000;
const int MAX_NAME_INDEX_SIZE = MAX_SECONDARY_SIZE;
const int MAX_SPECIALTY_INDEX_SIZE = MAX_SECONDARY_SIZE;
const int MAX_APPOINTMENT_DOCTOR_INDEX_SIZE = MAX_SECONDARY_SIZE;

#pragma pack(push, 1)
struct SecondaryEntry {
    char key[NAME_SIZE];
    int head;
};
struct SecondaryNode {
    char id[KEY_SIZE];
    int next;
};
#pragma pack(pop)

struct SecondaryIndex {
    const char* listFile;
    int keySize;
    int count;
    SecondaryEntry entries[MAX_SECONDARY_SIZE];
};

extern SecondaryIndex doctorNameIndex;
extern SecondaryIndex doctorSpecialtyIndex;
extern SecondaryIndex appointmentDoctorIndex;

// ----- Doctor Name (Doctors file) -----
int  binarySearchName(const char* name);
bool insertDoctorNameIndex(const char* name, const char* doctorID);
bool deleteDoctorNameIndex(const char* name, const char* doctorID);
bool updateDoctorNameIndex(const char* oldName, const char* newName, const char* doctorID);
bool renameDoctorIDInNameIndex(const char* name, const char* oldID, const char* newID);
int  searchDoctorNameIndex(const char* name, char outIDs[][KEY_SIZE], int maxIDs);
void buildDoctorNameIndex(const char* doctorFile);

// ----- Doctor Specialty (Doctors file) -----
int  binarySearchSpecialty(const char* specialty);
bool insertDoctorSpecialtyIndex(const char* specialty, const char* doctorID);
bool deleteDoctorSpecialtyIndex(const char* specialty, const char* doctorID);
bool updateDoctorSpecialtyIndex(const char* oldSpecialty, const char* newSpecialty, const char* doctorID);
bool renameDoctorIDInSpecialtyIndex(const char* specialty, const char* oldID, const char* newID);
int  searchDoctorSpecialtyIndex(const char* specialty, char outIDs[][KEY_SIZE], int maxIDs);
void buildDoctorSpecialtyIndex(const char* doctorFile);

// ----- Doctor ID (Appointments file) -----
int  binarySearchAppointmentDoctor(const char* doctorID);
bool insertAppointmentDoctorIndex(const char* doctorID, const char* appointmentID);
bool deleteAppointmentDoctorIndex(const char* doctorID, const char* appointmentID);
int  searchAppointmentsByDoctorID(const char* doctorID, char outIDs[][KEY_SIZE], int maxIDs);
void buildAppointmentDoctorIndex(const char* appointmentFile);

#endif