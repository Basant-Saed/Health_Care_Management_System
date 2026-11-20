#include "queryEngine.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <cctype>
#include <cstring>

using namespace std;

// ----- String Helpers -----

static string trim(const string& s) {
    size_t start = 0;
    while (start < s.length() && isspace((unsigned char)s[start])) start++;
    if (start == s.length()) return "";
    size_t end = s.length() - 1;
    while (end > start && isspace((unsigned char)s[end])) end--;
    return s.substr(start, end - start + 1);
}

static string toUpper(const string& s) {
    string res = s;
    for (char& c : res) c = (char)toupper((unsigned char)c);
    return res;
}

static string cleanFixedField(const char* buf, int maxLen) {
    int len = 0;
    while (len < maxLen && buf[len] != '\0') len++;
    return string(buf, len);
}

static string stripQuotes(const string& s) {
    string t = trim(s);
    if (t.length() >= 2) {
        if ((t.front() == '\'' && t.back() == '\'') || (t.front() == '"' && t.back() == '"'))
            return t.substr(1, t.length() - 2);
    }
    return t;
}

static string normalizeFieldName(QueryTable table, const string& raw) {
    string f = toUpper(trim(raw));
    string clean = "";
    for (char c : f) {
        if (isalnum((unsigned char)c)) clean += c;
    }
    if (table == QueryTable::DOCTORS) {
        if (clean == "DOCTORID" || clean == "ID" || clean == "DOCID") return "Doctor ID";
        if (clean == "DOCTORNAME" || clean == "NAME") return "Doctor Name";
        if (clean == "DOCTORSPECIALTY" || clean == "DOCTORSPECIALIZATION" || clean == "SPECIALTY" || clean == "SPECIALIZATION")
            return "Doctor Specialty";
        if (clean == "DOCTORADDRESS" || clean == "ADDRESS") return "Doctor Address";
    } else if (table == QueryTable::APPOINTMENTS) {
        if (clean == "APPOINTMENTID" || clean == "APPID" || clean == "ID") return "Appointment ID";
        if (clean == "APPOINTMENTDATE" || clean == "APPDATE" || clean == "DATE") return "Appointment Date";
        if (clean == "DOCTORID" || clean == "DOCID") return "Doctor ID";
    }
    return "";
}

static size_t findCharOutsideQuotes(const string& s, char target) {
    bool inQuote = false;
    char quoteChar = '\0';
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        if ((c == '\'' || c == '"') && (!inQuote || quoteChar == c)) {
            inQuote = !inQuote;
            quoteChar = inQuote ? c : '\0';
        } else if (!inQuote && c == target) {
            return i;
        }
    }
    return string::npos;
}

static size_t findKeywordOutsideQuotes(const string& s, const string& keyword, size_t startPos = 0) {
    string kwUpper = toUpper(keyword);
    size_t kwLen = keyword.length();
    bool inQuote = false;
    char quoteChar = '\0';

    for (size_t i = startPos; i < s.length(); i++) {
        char c = s[i];
        if ((c == '\'' || c == '"') && (!inQuote || quoteChar == c)) {
            inQuote = !inQuote;
            quoteChar = inQuote ? c : '\0';
        } else if (!inQuote && i + kwLen <= s.length()) {
            if (toUpper(s.substr(i, kwLen)) == kwUpper) {
                bool before = (i == 0 || isspace((unsigned char)s[i - 1]));
                bool after = (i + kwLen == s.length() || isspace((unsigned char)s[i + kwLen]));
                if (before && after) return i;
            }
        }
    }
    return string::npos;
}

static vector<string> splitConditionsByAnd(const string& condStr) {
    vector<string> conditions;
    size_t pos = 0;
    while (pos < condStr.length()) {
        size_t andPos = findKeywordOutsideQuotes(condStr, "AND", pos);
        if (andPos == string::npos) {
            string token = trim(condStr.substr(pos));
            if (!token.empty()) conditions.push_back(token);
            break;
        }
        string token = trim(condStr.substr(pos, andPos - pos));
        if (!token.empty()) conditions.push_back(token);
        pos = andPos + 3;
    }
    return conditions;
}

// ----- Record Lookup Helpers -----

static bool getDoctorRecord(const char* id, Doctor& d) {
    int offset = searchPrimaryIndex(doctorPrimaryIndex, id);
    if (offset == -1) return false;
    ifstream file("doctor.dat", ios::binary);
    if (!file) return false;
    file.seekg(offset, ios::beg);
    return file.read((char*)&d, sizeof(Doctor)) && d.ID[0] != '*';
}

static bool getAppointmentRecord(const char* id, Appointment& a) {
    int offset = searchPrimaryIndex(appointmentPrimaryIndex, id);
    if (offset == -1) return false;
    ifstream file("app.dat", ios::binary);
    if (!file) return false;
    file.seekg(offset, ios::beg);
    return file.read((char*)&a, sizeof(Appointment)) && a.ID[0] != '*';
}

// ----- Table Output Formatters -----

static void printDivider(const vector<int>& widths) {
    cout << "+";
    for (int w : widths) cout << string(w + 2, '-') << "+";
    cout << "\n";
}

static void printHeader(const vector<string>& headers, const vector<int>& widths) {
    printDivider(widths);
    cout << "|";
    for (size_t i = 0; i < headers.size(); i++)
        cout << " " << left << setw(widths[i]) << headers[i] << " |";
    cout << "\n";
    printDivider(widths);
}

static void printRow(const vector<string>& cells, const vector<int>& widths) {
    cout << "|";
    for (size_t i = 0; i < cells.size(); i++)
        cout << " " << left << setw(widths[i]) << cells[i] << " |";
    cout << "\n";
}

static void displayDoctorResults(const vector<Doctor>& results, const ParsedQuery& pq) {
    if (results.empty()) {
        cout << "No records found matching the specified query criteria.\n";
        return;
    }
    vector<string> headers;
    vector<int> widths;
    if (pq.isSelectAll) {
        headers = {"Doctor ID", "Doctor Name", "Specialization", "Address"};
        widths = {15, 30, 20, 30};
    } else {
        for (const string& f : pq.selectedFields) {
            headers.push_back(f);
            if (f == "Doctor ID") widths.push_back(15);
            else if (f == "Doctor Name") widths.push_back(30);
            else if (f == "Doctor Specialty") widths.push_back(20);
            else if (f == "Doctor Address") widths.push_back(30);
        }
    }

    printHeader(headers, widths);
    for (const auto& d : results) {
        vector<string> cells;
        if (pq.isSelectAll) {
            cells = {cleanFixedField(d.ID, 15), cleanFixedField(d.name, 30),
                     cleanFixedField(d.specialization, 20), cleanFixedField(d.address, 30)};
        } else {
            for (const string& f : pq.selectedFields) {
                if (f == "Doctor ID") cells.push_back(cleanFixedField(d.ID, 15));
                else if (f == "Doctor Name") cells.push_back(cleanFixedField(d.name, 30));
                else if (f == "Doctor Specialty") cells.push_back(cleanFixedField(d.specialization, 20));
                else if (f == "Doctor Address") cells.push_back(cleanFixedField(d.address, 30));
            }
        }
        printRow(cells, widths);
    }
    printDivider(widths);
    cout << "(" << results.size() << " record(s) returned)\n";
}

static void displayAppointmentResults(const vector<Appointment>& results, const ParsedQuery& pq) {
    if (results.empty()) {
        cout << "No records found matching the specified query criteria.\n";
        return;
    }
    vector<string> headers;
    vector<int> widths;
    if (pq.isSelectAll) {
        headers = {"App ID", "Appointment Date", "Doctor ID"};
        widths = {15, 30, 15};
    } else {
        for (const string& f : pq.selectedFields) {
            headers.push_back(f);
            if (f == "Appointment ID") widths.push_back(15);
            else if (f == "Appointment Date") widths.push_back(30);
            else if (f == "Doctor ID") widths.push_back(15);
        }
    }

    printHeader(headers, widths);
    for (const auto& a : results) {
        vector<string> cells;
        if (pq.isSelectAll) {
            cells = {cleanFixedField(a.ID, 15), cleanFixedField(a.date, 30), cleanFixedField(a.doctor_id, 15)};
        } else {
            for (const string& f : pq.selectedFields) {
                if (f == "Appointment ID") cells.push_back(cleanFixedField(a.ID, 15));
                else if (f == "Appointment Date") cells.push_back(cleanFixedField(a.date, 30));
                else if (f == "Doctor ID") cells.push_back(cleanFixedField(a.doctor_id, 15));
            }
        }
        printRow(cells, widths);
    }
    printDivider(widths);
    cout << "(" << results.size() << " record(s) returned)\n";
}

// ----- Query Parser -----

ParsedQuery parseQuery(const string& queryString) {
    ParsedQuery pq;
    pq.isValid = false;
    pq.table = QueryTable::UNKNOWN;
    pq.isSelectAll = false;

    string q = trim(queryString);
    if (!q.empty() && q.back() == ';') {
        q.pop_back();
        q = trim(q);
    }
    if (q.empty()) {
        pq.errorMessage = "Error: Query is empty.";
        return pq;
    }
    if (toUpper(q.substr(0, 7)) != "SELECT " && toUpper(q.substr(0, 7)) != "SELECT\t") {
        pq.errorMessage = "Syntax Error: Query must begin with keyword 'SELECT'.";
        return pq;
    }

    size_t fromPos = findKeywordOutsideQuotes(q, "FROM", 6);
    if (fromPos == string::npos) {
        pq.errorMessage = "Syntax Error: Missing 'FROM' clause in query.";
        return pq;
    }

    string fieldsPart = trim(q.substr(6, fromPos - 6));
    if (fieldsPart.empty()) {
        pq.errorMessage = "Syntax Error: No fields specified in SELECT clause.";
        return pq;
    }

    size_t wherePos = findKeywordOutsideQuotes(q, "WHERE", fromPos + 4);
    if (wherePos == string::npos) {
        pq.errorMessage = "Syntax Error: Missing 'WHERE' clause. Queries without WHERE require full sequential scans, which are prohibited.";
        return pq;
    }

    string tablePart = trim(q.substr(fromPos + 4, wherePos - (fromPos + 4)));
    string tableUpper = toUpper(tablePart);
    if (tableUpper == "DOCTORS" || tableUpper == "DOCTOR") {
        pq.table = QueryTable::DOCTORS;
    } else if (tableUpper == "APPOINTMENTS" || tableUpper == "APPOINTMENT") {
        pq.table = QueryTable::APPOINTMENTS;
    } else {
        pq.errorMessage = "Syntax Error: Unknown table '" + tablePart + "'. Supported tables are Doctors and Appointments.";
        return pq;
    }

    if (toUpper(fieldsPart) == "ALL" || fieldsPart == "*") {
        pq.isSelectAll = true;
        pq.selectedFields.push_back("ALL");
    } else {
        stringstream ss(fieldsPart);
        string item;
        while (getline(ss, item, ',')) {
            string rawF = trim(item);
            if (rawF.empty()) continue;
            string canonical = normalizeFieldName(pq.table, rawF);
            if (canonical.empty()) {
                pq.errorMessage = "Syntax Error: Unknown field '" + rawF + "' for table " + tablePart + ".";
                return pq;
            }
            pq.selectedFields.push_back(canonical);
        }
        if (pq.selectedFields.empty()) {
            pq.errorMessage = "Syntax Error: Invalid field list in SELECT clause.";
            return pq;
        }
    }

    string condPart = trim(q.substr(wherePos + 5));
    if (condPart.empty()) {
        pq.errorMessage = "Syntax Error: WHERE clause cannot be empty.";
        return pq;
    }

    vector<string> rawConditions = splitConditionsByAnd(condPart);
    if (rawConditions.empty()) {
        pq.errorMessage = "Syntax Error: No valid conditions found in WHERE clause.";
        return pq;
    }

    for (const string& rawCond : rawConditions) {
        size_t eqPos = findCharOutsideQuotes(rawCond, '=');
        if (eqPos == string::npos) {
            pq.errorMessage = "Syntax Error: Invalid condition '" + rawCond + "'. Expected format: Field='Value'.";
            return pq;
        }

        string rawField = trim(rawCond.substr(0, eqPos));
        string rawVal = trim(rawCond.substr(eqPos + 1));
        string canonicalField = normalizeFieldName(pq.table, rawField);
        if (canonicalField.empty()) {
            pq.errorMessage = "Syntax Error: Invalid condition field '" + rawField + "' for table " + tablePart + ".";
            return pq;
        }
        string cleanVal = stripQuotes(rawVal);
        if (cleanVal.empty() && rawVal != "''" && rawVal != "\"\"") {
            pq.errorMessage = "Syntax Error: Missing value in condition '" + rawCond + "'.";
            return pq;
        }
        pq.conditions.push_back({canonicalField, cleanVal});
    }

    pq.isValid = true;
    return pq;
}

// ----- Query Execution Engine -----

bool executeParsedQuery(const ParsedQuery& pq) {
    if (!pq.isValid) {
        cout << "[Query Error] " << pq.errorMessage << "\n";
        return false;
    }

    if (pq.table == QueryTable::DOCTORS) {
        string idCond = "", nameCond = "", specCond = "", addrCond = "";
        for (const auto& c : pq.conditions) {
            if (c.field == "Doctor ID") idCond = c.value;
            else if (c.field == "Doctor Name") nameCond = c.value;
            else if (c.field == "Doctor Specialty") specCond = c.value;
            else if (c.field == "Doctor Address") addrCond = c.value;
        }

        vector<Doctor> results;

        if (!idCond.empty()) {
            cout << "[Query Engine] Strategy: Binary Search on Doctor Primary Index (O(log N))...\n";
            Doctor d;
            if (getDoctorRecord(idCond.c_str(), d)) {
                if ((nameCond.empty() || cleanFixedField(d.name, 30) == nameCond) &&
                    (specCond.empty() || cleanFixedField(d.specialization, 20) == specCond) &&
                    (addrCond.empty() || cleanFixedField(d.address, 30) == addrCond)) {
                    results.push_back(d);
                }
            }
        } else if (!nameCond.empty() && !specCond.empty()) {
            cout << "[Query Engine] Strategy: Multiple Secondary Indexes Intersection (Doctor Name + Specialty)...\n";
            char nameIDs[MAX_NAME_INDEX_SIZE][KEY_SIZE];
            int nameCount = searchDoctorNameIndex(nameCond.c_str(), nameIDs, MAX_NAME_INDEX_SIZE);
            char specIDs[MAX_SPECIALTY_INDEX_SIZE][KEY_SIZE];
            int specCount = searchDoctorSpecialtyIndex(specCond.c_str(), specIDs, MAX_SPECIALTY_INDEX_SIZE);

            cout << "               -> Found " << nameCount << " doctor(s) matching Name '" << nameCond << "'\n";
            cout << "               -> Found " << specCount << " doctor(s) matching Specialty '" << specCond << "'\n";

            int matchedCount = 0;
            for (int i = 0; i < nameCount; i++) {
                string k1 = cleanFixedField(nameIDs[i], KEY_SIZE);
                for (int j = 0; j < specCount; j++) {
                    if (k1 == cleanFixedField(specIDs[j], KEY_SIZE)) {
                        matchedCount++;
                        Doctor d;
                        if (getDoctorRecord(k1.c_str(), d)) {
                            if (addrCond.empty() || cleanFixedField(d.address, 30) == addrCond)
                                results.push_back(d);
                        }
                        break;
                    }
                }
            }
            cout << "               -> Intersection size: " << matchedCount << " doctor(s).\n";
        } else if (!nameCond.empty()) {
            cout << "[Query Engine] Strategy: Secondary Index on Doctor Name (Linked List on File)...\n";
            char ids[MAX_NAME_INDEX_SIZE][KEY_SIZE];
            int count = searchDoctorNameIndex(nameCond.c_str(), ids, MAX_NAME_INDEX_SIZE);
            cout << "               -> Found " << count << " doctor(s) in Doctor Name index.\n";
            for (int i = 0; i < count; i++) {
                Doctor d;
                if (getDoctorRecord(cleanFixedField(ids[i], KEY_SIZE).c_str(), d)) {
                    if (addrCond.empty() || cleanFixedField(d.address, 30) == addrCond)
                        results.push_back(d);
                }
            }
        } else if (!specCond.empty()) {
            cout << "[Query Engine] Strategy: Secondary Index on Doctor Specialty (Linked List on File)...\n";
            char ids[MAX_SPECIALTY_INDEX_SIZE][KEY_SIZE];
            int count = searchDoctorSpecialtyIndex(specCond.c_str(), ids, MAX_SPECIALTY_INDEX_SIZE);
            cout << "               -> Found " << count << " doctor(s) in Doctor Specialty index.\n";
            for (int i = 0; i < count; i++) {
                Doctor d;
                if (getDoctorRecord(cleanFixedField(ids[i], KEY_SIZE).c_str(), d)) {
                    if (addrCond.empty() || cleanFixedField(d.address, 30) == addrCond)
                        results.push_back(d);
                }
            }
        } else {
            cout << "[Query Error] Full sequential file scan is strictly prohibited.\n";
            cout << "              WHERE condition must filter on an indexed field:\n";
            cout << "              - Doctor ID (Primary Index)\n";
            cout << "              - Doctor Name (Secondary Index)\n";
            cout << "              - Doctor Specialty (Secondary Index)\n";
            return false;
        }

        displayDoctorResults(results, pq);
        return true;
    }

    else if (pq.table == QueryTable::APPOINTMENTS) {
        string appIdCond = "", docIdCond = "", dateCond = "";
        for (const auto& c : pq.conditions) {
            if (c.field == "Appointment ID") appIdCond = c.value;
            else if (c.field == "Doctor ID") docIdCond = c.value;
            else if (c.field == "Appointment Date") dateCond = c.value;
        }

        vector<Appointment> results;

        if (!appIdCond.empty()) {
            cout << "[Query Engine] Strategy: Binary Search on Appointment Primary Index (O(log N))...\n";
            Appointment a;
            if (getAppointmentRecord(appIdCond.c_str(), a)) {
                if ((docIdCond.empty() || cleanFixedField(a.doctor_id, 15) == docIdCond) &&
                    (dateCond.empty() || cleanFixedField(a.date, 30) == dateCond)) {
                    results.push_back(a);
                }
            }
        } else if (!docIdCond.empty()) {
            cout << "[Query Engine] Strategy: Secondary Index on Doctor ID in Appointments (Linked List on File)...\n";
            char ids[MAX_APPOINTMENT_DOCTOR_INDEX_SIZE][KEY_SIZE];
            int count = searchAppointmentsByDoctorID(docIdCond.c_str(), ids, MAX_APPOINTMENT_DOCTOR_INDEX_SIZE);
            cout << "               -> Found " << count << " appointment(s) for Doctor ID '" << docIdCond << "'.\n";
            for (int i = 0; i < count; i++) {
                Appointment a;
                if (getAppointmentRecord(cleanFixedField(ids[i], KEY_SIZE).c_str(), a)) {
                    if (dateCond.empty() || cleanFixedField(a.date, 30) == dateCond)
                        results.push_back(a);
                }
            }
        } else {
            cout << "[Query Error] Full sequential file scan is strictly prohibited.\n";
            cout << "              WHERE condition must filter on an indexed field:\n";
            cout << "              - Appointment ID (Primary Index)\n";
            cout << "              - Doctor ID (Secondary Index)\n";
            return false;
        }

        displayAppointmentResults(results, pq);
        return true;
    }

    return false;
}

bool executeQuery(const string& queryString) {
    ParsedQuery pq = parseQuery(queryString);
    if (!pq.isValid) {
        cout << "[Query Error] " << pq.errorMessage << "\n";
        return false;
    }
    return executeParsedQuery(pq);
}

// ----- UI & Main Menu Functions -----

void showWelcomeScreen() {
    cout << "\n=========================================================\n";
    cout << "          HEALTHCARE MANAGEMENT SYSTEM (IS321)          \n";
    cout << "              File Management & Processing              \n";
    cout << "=========================================================\n";
    cout << " Team Responsibilities:\n";
    cout << "  - Member 1: File Init, Record Insert/Delete, Avail List\n";
    cout << "  - Member 2: Primary Indexes, Binary Search\n";
    cout << "  - Member 3: Update Operations, Doctor Name Sec Index\n";
    cout << "  - Member 4: Doctor Specialty & App Sec Indexes, Rebuild\n";
    cout << "  - Member 5: SQL Query Engine, UI & Integration\n";
    cout << "=========================================================\n\n";
}

void displayMainMenu() {
    cout << "\n----------------------- MAIN MENU -----------------------\n";
    cout << " 1. Add New Doctor\n";
    cout << " 2. Add New Appointment\n";
    cout << " 3. Update Doctor Name (Doctor ID)\n";
    cout << " 4. Update Doctor ID (Key Update Test)\n";
    cout << " 5. Update Appointment Date (Appointment ID)\n";
    cout << " 6. Delete Appointment (Appointment ID)\n";
    cout << " 7. Delete Doctor (Doctor ID)\n";
    cout << " 8. Print Doctor Info (Doctor ID)\n";
    cout << " 9. Print Appointment Info (Appointment ID)\n";
    cout << "10. Write Query\n";
    cout << "11. Exit\n";
    cout << "---------------------------------------------------------\n";
    cout << "Enter your choice (1-11): ";
}

void handleDeleteAppointmentMenu() {
    char id[KEY_SIZE];
    cout << "\n--- Delete Appointment ---\nEnter Appointment ID to delete: ";
    cin >> ws;
    cin.getline(id, KEY_SIZE);
    fstream f("app.dat", ios::binary | ios::in | ios::out);
    if (!f) {
        cout << "Error: Could not open app.dat file.\n";
        return;
    }
    delete_appointment(f, id);
    f.close();
}

void handleDeleteDoctorMenu() {
    char id[KEY_SIZE];
    cout << "\n--- Delete Doctor ---\nEnter Doctor ID to delete: ";
    cin >> ws;
    cin.getline(id, KEY_SIZE);
    fstream f("doctor.dat", ios::binary | ios::in | ios::out);
    if (!f) {
        cout << "Error: Could not open doctor.dat file.\n";
        return;
    }
    delete_doctor(f, id);
    f.close();
}

void handlePrintDoctorInfoMenu() {
    char id[KEY_SIZE];
    cout << "\n--- Print Doctor Info ---\nEnter Doctor ID: ";
    cin >> ws;
    cin.getline(id, KEY_SIZE);

    Doctor d;
    if (getDoctorRecord(id, d)) {
        cout << "\n========== Doctor Record ==========\n";
        cout << "  Doctor ID      : " << cleanFixedField(d.ID, 15) << "\n";
        cout << "  Doctor Name    : " << cleanFixedField(d.name, 30) << "\n";
        cout << "  Specialization : " << cleanFixedField(d.specialization, 20) << "\n";
        cout << "  Address        : " << cleanFixedField(d.address, 30) << "\n";
        cout << "  File Offset    : " << searchPrimaryIndex(doctorPrimaryIndex, id) << " bytes\n";
        cout << "===================================\n";
    } else {
        cout << "Error: Doctor with ID '" << id << "' not found in Primary Index.\n";
    }
}

void handlePrintAppointmentInfoMenu() {
    char id[KEY_SIZE];
    cout << "\n--- Print Appointment Info ---\nEnter Appointment ID: ";
    cin >> ws;
    cin.getline(id, KEY_SIZE);

    Appointment a;
    if (getAppointmentRecord(id, a)) {
        cout << "\n======== Appointment Record ========\n";
        cout << "  Appointment ID : " << cleanFixedField(a.ID, 15) << "\n";
        cout << "  Date           : " << cleanFixedField(a.date, 30) << "\n";
        cout << "  Doctor ID      : " << cleanFixedField(a.doctor_id, 15) << "\n";
        cout << "  File Offset    : " << searchPrimaryIndex(appointmentPrimaryIndex, id) << " bytes\n";
        cout << "====================================\n";
    } else {
        cout << "Error: Appointment with ID '" << id << "' not found in Primary Index.\n";
    }
}

void handleWriteQueryMenu() {
    cout << "\n=========================================================\n";
    cout << "                    SQL QUERY ENGINE                     \n";
    cout << "=========================================================\n";
    cout << "Supported Query Examples:\n";
    cout << "  1. SELECT ALL FROM Doctors WHERE Doctor ID='xxx';\n";
    cout << "  2. SELECT ALL FROM Appointments WHERE Doctor ID='xxx';\n";
    cout << "  3. SELECT Doctor Name FROM Doctors WHERE Doctor Name='xxx';\n";
    cout << "  4. SELECT ALL FROM Doctors WHERE Doctor Name='xxx' AND Doctor Specialty='yyy';\n";
    cout << "---------------------------------------------------------\n";
    cout << "Enter query (or type 'back' to return to menu):\n> ";

    string query;
    cin >> ws;
    getline(cin, query);
    if (query.empty() || toUpper(query) == "BACK") return;

    cout << "\nExecuting Query: " << query << "\n";
    executeQuery(query);
}

void runMainMenu() {
    int choice = -1;
    while (choice != 11) {
        displayMainMenu();
        if (!(cin >> choice)) {
            cin.clear();
            cin.ignore(1000, '\n');
            cout << "Invalid input. Please enter a valid number (1-11).\n";
            continue;
        }

        switch (choice) {
            case 1: add_doctor(); break;
            case 2: add_appointment(); break;
            case 3: update_doctor_name(); break;
            case 4: update_doctor_id(); break;
            case 5: update_appointment_date(); break;
            case 6: handleDeleteAppointmentMenu(); break;
            case 7: handleDeleteDoctorMenu(); break;
            case 8: handlePrintDoctorInfoMenu(); break;
            case 9: handlePrintAppointmentInfoMenu(); break;
            case 10: handleWriteQueryMenu(); break;
            case 11: cout << "\nThank you for using the Healthcare Management System. Goodbye!\n"; break;
            default: cout << "Invalid choice. Please select an option between 1 and 11.\n"; break;
        }
    }
}

// ----- Sample Data Seeder -----

void seedSampleDataIfEmpty() {
    if (doctorPrimaryIndex.count > 0 || appointmentPrimaryIndex.count > 0)
        return;

    cout << "[Initialization] Database is currently empty.\n";
    cout << "[Initialization] Seeding initial 5 Doctors and 10 Appointments...\n";

    Doctor sampleDocs[5] = {
        {"D001", "Dr. Ahmed Hassan", "Cardiology", "10 Tahrir St, Cairo"},
        {"D002", "Dr. Sara Mohamed", "Pediatrics", "25 El-Nasr Rd, Giza"},
        {"D003", "Dr. John Smith", "Cardiology", "14 Horreya Ave, Alex"},
        {"D004", "Dr. Mona Ali", "Dermatology", "8 El-Galaa St, Cairo"},
        {"D005", "Dr. John Smith", "Orthopedics", "30 Abbasia, Cairo"}
    };

    ofstream docFile("doctor.dat", ios::binary | ios::trunc);
    int availHead = -1;
    docFile.write((char*)&availHead, sizeof(int));
    for (int i = 0; i < 5; i++)
        docFile.write((char*)&sampleDocs[i], sizeof(Doctor));
    docFile.close();

    Appointment sampleApps[10] = {
        {"A001", "2026-11-20 09:00", "D001"},
        {"A002", "2026-11-20 10:30", "D001"},
        {"A003", "2026-11-21 11:00", "D002"},
        {"A004", "2026-11-21 14:00", "D002"},
        {"A005", "2026-11-22 09:30", "D003"},
        {"A006", "2026-11-22 15:00", "D003"},
        {"A007", "2026-11-23 10:00", "D004"},
        {"A008", "2026-11-23 12:30", "D004"},
        {"A009", "2026-11-24 11:00", "D005"},
        {"A010", "2026-11-24 16:00", "D001"}
    };

    ofstream appFile("app.dat", ios::binary | ios::trunc);
    appFile.write((char*)&availHead, sizeof(int));
    for (int i = 0; i < 10; i++)
        appFile.write((char*)&sampleApps[i], sizeof(Appointment));
    appFile.close();

    rebuildIndexes();
    cout << "[Initialization] Seeding complete: 5 Doctors and 10 Appointments loaded.\n";
}
