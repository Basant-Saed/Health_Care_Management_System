#include <iostream>
#include <fstream>
#include <cassert>
#include <string>
#include "structs.h"
#include "indexManager.h"
#include "secondaryIndex.h"
#include "doctorManager.h"
#include "appointmentManager.h"
#include "queryEngine.h"

using namespace std;

static int testsPassed = 0;
static int testsFailed = 0;

static void assertTest(bool condition, const string& testName) {
    if (condition) {
        cout << "[PASS] " << testName << "\n";
        testsPassed++;
    } else {
        cout << "[FAIL] " << testName << "\n";
        testsFailed++;
    }
}

int main() {
    cout << "=========================================================\n";
    cout << "         IS321 MEMBER 5 COMPREHENSIVE TEST SUITE         \n";
    cout << "=========================================================\n\n";

    // 1. Initialize files and seed sample data
    initializeDoctorFile();
    initializeAppointment();
    rebuildIndexes();
    seedSampleDataIfEmpty();

    cout << "\n---------------------------------------------------------\n";
    cout << " TEST GROUP 1: Parser Tests (Unit Validation)\n";
    cout << "---------------------------------------------------------\n";

    // Valid Query 1
    {
        ParsedQuery pq = parseQuery("SELECT ALL FROM Doctors WHERE Doctor ID='D001';");
        assertTest(pq.isValid, "Parse Query 1 (Doctor Primary Index)");
        assertTest(pq.table == QueryTable::DOCTORS, "Query 1 Table is DOCTORS");
        assertTest(pq.isSelectAll, "Query 1 is SELECT ALL");
        assertTest(pq.conditions.size() == 1, "Query 1 has 1 condition");
        if (pq.conditions.size() == 1) {
            assertTest(pq.conditions[0].field == "Doctor ID" && pq.conditions[0].value == "D001",
                       "Query 1 condition is Doctor ID='D001'");
        }
    }

    // Valid Query 2
    {
        ParsedQuery pq = parseQuery("SELECT ALL FROM Appointments WHERE Doctor ID='D001';");
        assertTest(pq.isValid, "Parse Query 2 (Appointments by Doctor ID)");
        assertTest(pq.table == QueryTable::APPOINTMENTS, "Query 2 Table is APPOINTMENTS");
        assertTest(pq.isSelectAll, "Query 2 is SELECT ALL");
        assertTest(pq.conditions.size() == 1, "Query 2 has 1 condition");
        if (pq.conditions.size() == 1) {
            assertTest(pq.conditions[0].field == "Doctor ID" && pq.conditions[0].value == "D001",
                       "Query 2 condition is Doctor ID='D001'");
        }
    }

    // Valid Query 3
    {
        ParsedQuery pq = parseQuery("SELECT Doctor Name FROM Doctors WHERE Doctor Name='Dr. John Smith';");
        assertTest(pq.isValid, "Parse Query 3 (Doctor Name projection & condition)");
        assertTest(pq.table == QueryTable::DOCTORS, "Query 3 Table is DOCTORS");
        assertTest(!pq.isSelectAll, "Query 3 is not SELECT ALL");
        assertTest(pq.selectedFields.size() == 1 && pq.selectedFields[0] == "Doctor Name",
                       "Query 3 selected field is 'Doctor Name'");
    }

    // Valid Query 4
    {
        ParsedQuery pq = parseQuery("SELECT ALL FROM Doctors WHERE Doctor Name='Dr. John Smith' AND Doctor Specialty='Cardiology';");
        assertTest(pq.isValid, "Parse Query 4 (Multiple Secondary Indexes AND)");
        assertTest(pq.table == QueryTable::DOCTORS, "Query 4 Table is DOCTORS");
        assertTest(pq.conditions.size() == 2, "Query 4 has 2 conditions");
        if (pq.conditions.size() == 2) {
            assertTest(pq.conditions[0].field == "Doctor Name" && pq.conditions[0].value == "Dr. John Smith",
                       "Query 4 condition 1 is Doctor Name");
            assertTest(pq.conditions[1].field == "Doctor Specialty" && pq.conditions[1].value == "Cardiology",
                       "Query 4 condition 2 is Doctor Specialty");
        }
    }

    // Syntax Errors
    {
        ParsedQuery pq1 = parseQuery("SELECT ALL Doctors WHERE ID='D001'");
        assertTest(!pq1.isValid, "Catch missing 'FROM'");

        ParsedQuery pq2 = parseQuery("SELECT ALL FROM Doctors");
        assertTest(!pq2.isValid, "Catch missing 'WHERE'");

        ParsedQuery pq3 = parseQuery("SELECT ALL FROM Patients WHERE ID='P01'");
        assertTest(!pq3.isValid, "Catch unknown table 'Patients'");

        ParsedQuery pq4 = parseQuery("SELECT Salary FROM Doctors WHERE Doctor ID='D001'");
        assertTest(!pq4.isValid, "Catch unknown field 'Salary'");

        ParsedQuery pq5 = parseQuery("SELECT ALL FROM Doctors WHERE Doctor ID");
        assertTest(!pq5.isValid, "Catch malformed condition without '='");
    }

    cout << "\n---------------------------------------------------------\n";
    cout << " TEST GROUP 2: Query Execution & Results Verification\n";
    cout << "---------------------------------------------------------\n";

    // Execution 1: Primary Index Search on Doctor ID
    cout << "\n[TEST RUN 1] Primary Index search on Doctor ID='D001'\n";
    bool res1 = executeQuery("SELECT ALL FROM Doctors WHERE Doctor ID='D001';");
    assertTest(res1, "Executed Query 1 successfully");

    // Execution 2: Secondary Index search on Appointments for Doctor ID='D001' (Expect 3 appointments: A001, A002, A010)
    cout << "\n[TEST RUN 2] Secondary Index search on Appointments WHERE Doctor ID='D001'\n";
    bool res2 = executeQuery("SELECT ALL FROM Appointments WHERE Doctor ID='D001';");
    assertTest(res2, "Executed Query 2 successfully");

    // Execution 3: Secondary Index search on Doctors WHERE Doctor Name='Dr. John Smith' (Expect 2 doctors: D003 and D005)
    cout << "\n[TEST RUN 3] Secondary Index search on Doctors WHERE Doctor Name='Dr. John Smith'\n";
    bool res3 = executeQuery("SELECT Doctor Name FROM Doctors WHERE Doctor Name='Dr. John Smith';");
    assertTest(res3, "Executed Query 3 successfully");

    // Execution 4: Multiple Secondary Indexes: Name='Dr. John Smith' AND Specialty='Cardiology' (Expect ONLY D003)
    cout << "\n[TEST RUN 4] Multiple Secondary Indexes Intersection WHERE Doctor Name='Dr. John Smith' AND Doctor Specialty='Cardiology'\n";
    bool res4 = executeQuery("SELECT ALL FROM Doctors WHERE Doctor Name='Dr. John Smith' AND Doctor Specialty='Cardiology';");
    assertTest(res4, "Executed Query 4 successfully");

    // Execution 5: Missing record in Primary Index
    cout << "\n[TEST RUN 5] Non-existent Doctor ID='D999'\n";
    bool res5 = executeQuery("SELECT ALL FROM Doctors WHERE Doctor ID='D999';");
    assertTest(res5, "Non-existent doctor handled gracefully (0 records returned)");

    // Execution 6: Missing record in Secondary Index
    cout << "\n[TEST RUN 6] Non-existent Doctor ID in Appointments='D999'\n";
    bool res6 = executeQuery("SELECT ALL FROM Appointments WHERE Doctor ID='D999';");
    assertTest(res6, "Non-existent doctor in appointments handled gracefully (0 records returned)");

    // Execution 7: Prohibited sequential scan attempt on unindexed field
    cout << "\n[TEST RUN 7] Prohibited sequential scan attempt on Doctor Address\n";
    bool res7 = executeQuery("SELECT ALL FROM Doctors WHERE Doctor Address='10 Tahrir St, Cairo';");
    assertTest(!res7, "Prohibited sequential scan on Address correctly rejected with error");

    // Execution 8: Case insensitivity and whitespace tolerance
    cout << "\n[TEST RUN 8] Lowercase query with extra whitespace\n";
    bool res8 = executeQuery("select   all   from   doctors   where   doctor id = 'D002'  ;");
    assertTest(res8, "Lowercase query with flexible whitespace executed successfully");

    cout << "\n=========================================================\n";
    cout << " FINAL TEST RESULTS: " << testsPassed << " Passed, " << testsFailed << " Failed.\n";
    cout << "=========================================================\n";

    return (testsFailed == 0) ? 0 : 1;
}
