#include <iostream>
#include <fstream>
#include "structs.h"
#include "doctorManager.h"
#include "indexManager.h"
#include "appointmentManager.h"
#include "secondaryIndex.h"

using namespace std;

int main() {

    initializeDoctorFile();
    initializeAppointment();

    buildDoctorPrimaryIndex(doctors.dart);
    buildDoctorNameIndex("doctor.dat")
    buildAppointmentPrimaryIndex(appointments.dart);

    cout << "=== Healthcare System ===" << endl;

    return 0;
}