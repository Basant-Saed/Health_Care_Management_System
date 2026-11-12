#include <iostream>
#include <fstream>
#include "structs.h"
#include "doctorManager.h"
#include "indexManager.h"
#include "appointmentManager.h"
#include "secondaryIndex.h"
#include "queryEngine.h"

using namespace std;

int main() {
    // 1. Initialize binary data files if not present (Member 1)
    initializeDoctorFile();
    initializeAppointment();

    // 2. Rebuild all RAM indexes from data files on program startup (Member 4)
    rebuildIndexes();

    // 3. Seed initial 5 doctors and 10 appointments if database is empty (Member 1 / 5 helper)
    seedSampleDataIfEmpty();

    // 4. Display Welcome Screen (Member 5)
    showWelcomeScreen();

    // 5. Launch Main Menu interactive console loop (Member 5)
    runMainMenu();

    return 0;
}
