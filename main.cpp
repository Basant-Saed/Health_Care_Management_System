#include <iostream>
#include <fstream>
#include "structs.h"
#include "doctorManager.h"
#include "appointmentManager.h"


using namespace std;

int main() {

    initializeDoctorFile();
    initializeAppointment();

    cout << "=== Healthcare System ===" << endl;

    return 0;
}