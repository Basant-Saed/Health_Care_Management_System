#ifndef doctorManager_h
#define doctorManager_h


#include "structs.h"
#include <fstream>
using namespace std;

void initializeDoctorFile();
void add_doctor();
bool delete_doctor(fstream& doctor, const char* doctorID );
bool update_doctor(fstream& doctor, const char* oldID, const Doctor& updatedDoctor);
void update_doctor_name();
void update_doctor_id();

#endif