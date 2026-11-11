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

    rebuildIndexes(); 

    cout << "=== Healthcare System ===" << endl; 

    return 0; 

} 