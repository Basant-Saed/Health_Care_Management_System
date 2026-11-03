#include <iostream>
#include <fstream>
#include "structs.h"
#include "doctorManager.h"
#include "indexManager.h"
#include "appointmentManager.h"


using namespace std;

int main() {

    initializeDoctorFile();
    initializeAppointment();

    buildDoctorPrimaryIndex(doctors.dart);
    buildAppointmentPrimaryIndex(appointments.dart);

    cout << "=== Healthcare System ===" << endl;

    return 0;
}