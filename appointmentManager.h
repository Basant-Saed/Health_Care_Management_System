#ifndef appointmentManager_h
#define appointmentManager_h

#include "structs.h"
#include <fstream>
using namespace std;

void initializeAppointment();
void add_appointment();
bool delete_appointment(fstream& app, const char* appID );
bool update_appointment(fstream& app, const char* oldID, const Appointment& updatedApp);
void update_appointment_date();

#endif