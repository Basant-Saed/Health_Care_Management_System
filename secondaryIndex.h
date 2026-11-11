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





// ================= SPECIALTY SECONDARY INDEX =================

const int SPECIALTY_SIZE = 20;
const int MAX_SPECIALTY_INDEX_SIZE = 1000;

#pragma pack(push, 1)

struct SpecialtyIndexEntry {
    char specialty[SPECIALTY_SIZE];
    int head;
};

struct SpecialtyListNode {
    char doctorID[KEY_SIZE];
    int next;
};

#pragma pack(pop)

struct SpecialtyIndex {
    SpecialtyIndexEntry entries[MAX_SPECIALTY_INDEX_SIZE];
    int count;
};

extern SpecialtyIndex doctorSpecialtyIndex;

void initSpecialtyIndex();
int binarySearchSpecialty(const char* specialty);

bool insertDoctorSpecialtyIndex(const char* specialty, const char* doctorID);
bool deleteDoctorSpecialtyIndex(const char* specialty, const char* doctorID);

bool updateDoctorSpecialtyIndex(const char* oldSpecialty,const char* newSpecialty,const char* doctorID);

bool renameDoctorIDInSpecialtyIndex(const char* specialty,const char* oldID,const char* newID);

int searchDoctorSpecialtyIndex(const char* specialty, char outIDs[][KEY_SIZE],int maxIDs);

void buildDoctorSpecialtyIndex(const char* doctorFile);
void saveSpecialtyIndex();
void printSpecialtyIndex();



// ================= APPOINTMENT DOCTOR ID SECONDARY INDEX =================

const int MAX_APPOINTMENT_DOCTOR_INDEX_SIZE = 1000;

#pragma pack(push, 1)

struct AppointmentDoctorIndexEntry {
    char doctorID[KEY_SIZE];
    int head;
};

struct AppointmentDoctorListNode {
    char appointmentID[KEY_SIZE];
    int next;
};

#pragma pack(pop)

struct AppointmentDoctorIndex {
    AppointmentDoctorIndexEntry entries[MAX_APPOINTMENT_DOCTOR_INDEX_SIZE];
    int count;
};

extern AppointmentDoctorIndex appointmentDoctorIndex;

void initAppointmentDoctorIndex();
int binarySearchAppointmentDoctor(const char* doctorID);

bool insertAppointmentDoctorIndex(const char* doctorID, const char* appointmentID);
bool deleteAppointmentDoctorIndex(const char* doctorID, const char* appointmentID);

int searchAppointmentsByDoctorID(const char* doctorID,char outIDs[][KEY_SIZE],int maxIDs);

void buildAppointmentDoctorIndex(const char* appointmentFile);
void saveAppointmentDoctorIndex();
void printAppointmentDoctorIndex();

#endif
