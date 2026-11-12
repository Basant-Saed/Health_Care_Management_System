#include "queryEngine.h"
#include "structs.h"
#include "indexManager.h"
#include "secondaryIndex.h"
#include "doctorManager.h"
#include "appointmentManager.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <cstring>

using namespace std;

// ============================================================================
// STRING UTILITIES & HELPERS
// ============================================================================

// Trims leading and trailing whitespace characters
static string trim(const string& s) {
    size_t start = 0;
    while (start < s.length() && isspace(static_cast<unsigned char>(s[start]))) {
        start++;
    }
    if (start == s.length()) return "";

    size_t end = s.length() - 1;
    while (end > start && isspace(static_cast<unsigned char>(s[end]))) {
        end--;
    }
    return s.substr(start, end - start + 1);
}

// Converts a string to uppercase
static string toUpper(const string& s) {
    string res = s;
    for (char& c : res) {
        c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    }
    return res;
}

// Cleans a fixed-length character array into a std::string up to null-terminator or maxLen
static string cleanFixedField(const char* buf, int maxLen) {
    int len = 0;
    while (len < maxLen && buf[len] != '\0') {
        len++;
    }
    return string(buf, len);
}

// Removes surrounding single or double quotes from a literal value
static string stripQuotes(const string& s) {
    string t = trim(s);
    if (t.length() >= 2) {
        if ((t.front() == '\'' && t.back() == '\'') ||
            (t.front() == '\"' && t.back() == '\"')) {
            return t.substr(1, t.length() - 2);
        }
    }
    return t;
}

// Normalizes field names into canonical representations
static string normalizeFieldName(QueryTable table, const string& rawField) {
    string f = toUpper(trim(rawField));

    // Remove extra internal spaces
    string normalized;
    bool lastSpace = false;
    for (char c : f) {
        if (isspace(static_cast<unsigned char>(c))) {
            if (!lastSpace) {
                normalized += ' ';
                lastSpace = true;
            }
        } else {
            normalized += c;
            lastSpace = false;
        }
    }

    if (table == QueryTable::DOCTORS) {
        if (normalized == "DOCTOR ID" || normalized == "DOCTORID" ||
            normalized == "ID" || normalized == "DOC ID" || normalized == "DOCID") {
            return "Doctor ID";
        }
        if (normalized == "DOCTOR NAME" || normalized == "DOCTORNAME" ||
            normalized == "NAME") {
            return "Doctor Name";
        }
        if (normalized == "DOCTOR SPECIALTY" || normalized == "DOCTOR SPECIALIZATION" ||
            normalized == "SPECIALTY" || normalized == "SPECIALIZATION" ||
            normalized == "DOCTORSPECIALTY" || normalized == "DOCTORSPECIALIZATION") {
            return "Doctor Specialty";
        }
        if (normalized == "DOCTOR ADDRESS" || normalized == "DOCTORADDRESS" ||
            normalized == "ADDRESS") {
            return "Doctor Address";
        }
    } else if (table == QueryTable::APPOINTMENTS) {
        if (normalized == "APPOINTMENT ID" || normalized == "APPOINTMENTID" ||
            normalized == "APP ID" || normalized == "APPID" || normalized == "ID") {
            return "Appointment ID";
        }
        if (normalized == "APPOINTMENT DATE" || normalized == "APPOINTMENTDATE" ||
            normalized == "APP DATE" || normalized == "DATE") {
            return "Appointment Date";
        }
        if (normalized == "DOCTOR ID" || normalized == "DOCTORID" ||
            normalized == "DOC ID" || normalized == "DOCID") {
            return "Doctor ID";
        }
    }
    return "";
}

// Finds the first occurrence of a whole keyword (e.g. "FROM", "WHERE") outside of quotes
static size_t findKeywordOutsideQuotes(const string& s, const string& keyword, size_t startPos = 0) {
    string kwUpper = toUpper(keyword);
    size_t kwLen = keyword.length();
    bool inSingleQuote = false;
    bool inDoubleQuote = false;

    for (size_t i = startPos; i < s.length(); i++) {
        char c = s[i];
        if (c == '\'' && !inDoubleQuote) {
            inSingleQuote = !inSingleQuote;
        } else if (c == '\"' && !inSingleQuote) {
            inDoubleQuote = !inDoubleQuote;
        } else if (!inSingleQuote && !inDoubleQuote) {
            // Check if keyword matches at position i
            if (i + kwLen <= s.length()) {
                string sub = toUpper(s.substr(i, kwLen));
                if (sub == kwUpper) {
                    // Check word boundary before
                    bool boundaryBefore = (i == 0 || isspace(static_cast<unsigned char>(s[i - 1])));
                    // Check word boundary after
                    bool boundaryAfter = (i + kwLen == s.length() || isspace(static_cast<unsigned char>(s[i + kwLen])));

                    if (boundaryBefore && boundaryAfter) {
                        return i;
                    }
                }
            }
        }
    }
    return string::npos;
}

// Splits the WHERE clause on "AND" outside quotes
static vector<string> splitConditionsByAnd(const string& condStr) {
    vector<string> conditions;
    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    size_t lastPos = 0;

    for (size_t i = 0; i < condStr.length(); i++) {
        char c = condStr[i];
        if (c == '\'' && !inDoubleQuote) {
            inSingleQuote = !inSingleQuote;
        } else if (c == '\"' && !inSingleQuote) {
            inDoubleQuote = !inDoubleQuote;
        } else if (!inSingleQuote && !inDoubleQuote) {
            if (i + 3 <= condStr.length()) {
                string sub = toUpper(condStr.substr(i, 3));
                if (sub == "AND") {
                    bool boundaryBefore = (i == 0 || isspace(static_cast<unsigned char>(condStr[i - 1])));
                    bool boundaryAfter = (i + 3 == condStr.length() || isspace(static_cast<unsigned char>(condStr[i + 3])));

                    if (boundaryBefore && boundaryAfter) {
                        string token = trim(condStr.substr(lastPos, i - lastPos));
                        if (!token.empty()) {
                            conditions.push_back(token);
                        }
                        i += 2; // skip to end of "AND"
                        lastPos = i + 1;
                    }
                }
            }
        }
    }

    if (lastPos < condStr.length()) {
        string token = trim(condStr.substr(lastPos));
        if (!token.empty()) {
            conditions.push_back(token);
        }
    }
    return conditions;
}

// ============================================================================
// QUERY PARSER
// ============================================================================

ParsedQuery parseQuery(const string& queryString) {
    ParsedQuery pq;
    pq.isValid = false;
    pq.table = QueryTable::UNKNOWN;
    pq.isSelectAll = false;

    string q = trim(queryString);

    // Strip trailing semicolon if present
    if (!q.empty() && q.back() == ';') {
        q.pop_back();
        q = trim(q);
    }

    if (q.empty()) {
        pq.errorMessage = "Error: Query is empty.";
        return pq;
    }

    // Must start with "SELECT "
    if (toUpper(q.substr(0, 7)) != "SELECT " && toUpper(q.substr(0, 7)) != "SELECT\t") {
        pq.errorMessage = "Syntax Error: Query must begin with keyword 'SELECT'.";
        return pq;
    }

    // Locate "FROM"
    size_t fromPos = findKeywordOutsideQuotes(q, "FROM", 6);
    if (fromPos == string::npos) {
        pq.errorMessage = "Syntax Error: Missing 'FROM' clause in query.";
        return pq;
    }

    // Extract fields substring
    string fieldsPart = trim(q.substr(6, fromPos - 6));
    if (fieldsPart.empty()) {
        pq.errorMessage = "Syntax Error: No fields specified in SELECT clause.";
        return pq;
    }

    // Locate "WHERE"
    size_t wherePos = findKeywordOutsideQuotes(q, "WHERE", fromPos + 4);
    if (wherePos == string::npos) {
        pq.errorMessage = "Syntax Error: Missing 'WHERE' clause. Queries without WHERE require full sequential scans, which are prohibited.";
        return pq;
    }

    // Extract table substring
    string tablePart = trim(q.substr(fromPos + 4, wherePos - (fromPos + 4)));
    if (tablePart.empty()) {
        pq.errorMessage = "Syntax Error: Missing table name after 'FROM'.";
        return pq;
    }

    // Identify Table
    string tableUpper = toUpper(tablePart);
    if (tableUpper == "DOCTORS" || tableUpper == "DOCTOR") {
        pq.table = QueryTable::DOCTORS;
    } else if (tableUpper == "APPOINTMENTS" || tableUpper == "APPOINTMENT") {
        pq.table = QueryTable::APPOINTMENTS;
    } else {
        pq.errorMessage = "Syntax Error: Unknown table '" + tablePart + "'. Supported tables are Doctors and Appointments.";
        return pq;
    }

    // Parse Fields
    string fieldsUpper = toUpper(fieldsPart);
    if (fieldsUpper == "ALL" || fieldsUpper == "*") {
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

    // Extract WHERE conditions substring
    string condPart = trim(q.substr(wherePos + 5));
    if (condPart.empty()) {
        pq.errorMessage = "Syntax Error: WHERE clause cannot be empty.";
        return pq;
    }

    // Split conditions joined by AND
    vector<string> rawConditions = splitConditionsByAnd(condPart);
    if (rawConditions.empty()) {
        pq.errorMessage = "Syntax Error: No valid conditions found in WHERE clause.";
        return pq;
    }

    for (const string& rawCond : rawConditions) {
        // Find '=' outside quotes
        size_t eqPos = string::npos;
        bool inSingleQuote = false;
        bool inDoubleQuote = false;
        for (size_t i = 0; i < rawCond.length(); i++) {
            char c = rawCond[i];
            if (c == '\'' && !inDoubleQuote) {
                inSingleQuote = !inSingleQuote;
            } else if (c == '\"' && !inSingleQuote) {
                inDoubleQuote = !inDoubleQuote;
            } else if (!inSingleQuote && !inDoubleQuote && c == '=') {
                eqPos = i;
                break;
            }
        }

        if (eqPos == string::npos) {
            pq.errorMessage = "Syntax Error: Invalid condition '" + rawCond + "'. Expected format: Field='Value'.";
            return pq;
        }

        string rawFieldName = trim(rawCond.substr(0, eqPos));
        string rawValue = trim(rawCond.substr(eqPos + 1));

        string canonicalField = normalizeFieldName(pq.table, rawFieldName);
        if (canonicalField.empty()) {
            pq.errorMessage = "Syntax Error: Invalid condition field '" + rawFieldName + "' for table " + tablePart + ".";
            return pq;
        }

        string cleanVal = stripQuotes(rawValue);
        if (cleanVal.empty() && rawValue != "''" && rawValue != "\"\"") {
            pq.errorMessage = "Syntax Error: Missing value in condition '" + rawCond + "'.";
            return pq;
        }

        pq.conditions.push_back(QueryCondition{canonicalField, cleanVal});
    }

    pq.isValid = true;
    return pq;
}

// ============================================================================
// RESULT DISPLAY FORMATTERS
// ============================================================================

static void printDivider(const vector<int>& widths) {
    cout << "+";
    for (int w : widths) {
        cout << string(w + 2, '-') << "+";
    }
    cout << "\n";
}

static void printDoctorHeader(const vector<string>& headers, const vector<int>& widths) {
    printDivider(widths);
    cout << "|";
    for (size_t i = 0; i < headers.size(); i++) {
        cout << " " << left << setw(widths[i]) << headers[i] << " |";
    }
    cout << "\n";
    printDivider(widths);
}

static void printDoctorRow(const Doctor& d, const ParsedQuery& pq, const vector<int>& widths) {
    string idStr = cleanFixedField(d.ID, sizeof(d.ID));
    string nameStr = cleanFixedField(d.name, sizeof(d.name));
    string specStr = cleanFixedField(d.specialization, sizeof(d.specialization));
    string addrStr = cleanFixedField(d.address, sizeof(d.address));

    cout << "|";
    if (pq.isSelectAll) {
        cout << " " << left << setw(widths[0]) << idStr << " |";
        cout << " " << left << setw(widths[1]) << nameStr << " |";
        cout << " " << left << setw(widths[2]) << specStr << " |";
        cout << " " << left << setw(widths[3]) << addrStr << " |";
    } else {
        for (size_t i = 0; i < pq.selectedFields.size(); i++) {
            const string& f = pq.selectedFields[i];
            if (f == "Doctor ID") {
                cout << " " << left << setw(widths[i]) << idStr << " |";
            } else if (f == "Doctor Name") {
                cout << " " << left << setw(widths[i]) << nameStr << " |";
            } else if (f == "Doctor Specialty") {
                cout << " " << left << setw(widths[i]) << specStr << " |";
            } else if (f == "Doctor Address") {
                cout << " " << left << setw(widths[i]) << addrStr << " |";
            }
        }
    }
    cout << "\n";
}

static void printAppointmentRow(const Appointment& a, const ParsedQuery& pq, const vector<int>& widths) {
    string idStr = cleanFixedField(a.ID, sizeof(a.ID));
    string dateStr = cleanFixedField(a.date, sizeof(a.date));
    string docIdStr = cleanFixedField(a.doctor_id, sizeof(a.doctor_id));

    cout << "|";
    if (pq.isSelectAll) {
        cout << " " << left << setw(widths[0]) << idStr << " |";
        cout << " " << left << setw(widths[1]) << dateStr << " |";
        cout << " " << left << setw(widths[2]) << docIdStr << " |";
    } else {
        for (size_t i = 0; i < pq.selectedFields.size(); i++) {
            const string& f = pq.selectedFields[i];
            if (f == "Appointment ID") {
                cout << " " << left << setw(widths[i]) << idStr << " |";
            } else if (f == "Appointment Date") {
                cout << " " << left << setw(widths[i]) << dateStr << " |";
            } else if (f == "Doctor ID") {
                cout << " " << left << setw(widths[i]) << docIdStr << " |";
            }
        }
    }
    cout << "\n";
}

// ============================================================================
// QUERY EXECUTION ENGINE
// ============================================================================

bool executeParsedQuery(const ParsedQuery& pq) {
    if (!pq.isValid) {
        cout << "[Query Error] " << pq.errorMessage << "\n";
        return false;
    }

    if (pq.table == QueryTable::DOCTORS) {
        // Classify conditions
        string idCond = "";
        string nameCond = "";
        string specCond = "";
        string addrCond = "";

        for (const auto& c : pq.conditions) {
            if (c.field == "Doctor ID") idCond = c.value;
            else if (c.field == "Doctor Name") nameCond = c.value;
            else if (c.field == "Doctor Specialty") specCond = c.value;
            else if (c.field == "Doctor Address") addrCond = c.value;
        }

        vector<Doctor> results;
        ifstream docFile("doctor.dat", ios::binary);
        if (!docFile) {
            cout << "[Query Error] Could not open doctor.dat file.\n";
            return false;
        }

        // 1. Primary Index Search: Doctor ID
        if (!idCond.empty()) {
            cout << "[Query Engine] Strategy: Binary Search on Doctor Primary Index (O(log N))...\n";
            int offset = searchPrimaryIndex(doctorPrimaryIndex, idCond.c_str());
            if (offset != -1) {
                Doctor d;
                docFile.seekg(offset, ios::beg);
                if (docFile.read(reinterpret_cast<char*>(&d), sizeof(Doctor)) && d.ID[0] != '*') {
                    // Check other filter conditions if present
                    bool match = true;
                    if (!nameCond.empty() && cleanFixedField(d.name, sizeof(d.name)) != nameCond) match = false;
                    if (!specCond.empty() && cleanFixedField(d.specialization, sizeof(d.specialization)) != specCond) match = false;
                    if (!addrCond.empty() && cleanFixedField(d.address, sizeof(d.address)) != addrCond) match = false;
                    if (match) {
                        results.push_back(d);
                    }
                }
            }
        }
        // 4. Multiple Secondary Indexes: Doctor Name AND Doctor Specialty
        else if (!nameCond.empty() && !specCond.empty()) {
            cout << "[Query Engine] Strategy: Multiple Secondary Indexes Intersection (Doctor Name + Specialty)...\n";
            char nameIDs[MAX_NAME_INDEX_SIZE][KEY_SIZE];
            int nameCount = searchDoctorNameIndex(nameCond.c_str(), nameIDs, MAX_NAME_INDEX_SIZE);

            char specIDs[MAX_SPECIALTY_INDEX_SIZE][KEY_SIZE];
            int specCount = searchDoctorSpecialtyIndex(specCond.c_str(), specIDs, MAX_SPECIALTY_INDEX_SIZE);

            cout << "               -> Found " << nameCount << " doctor(s) matching Name '" << nameCond << "'\n";
            cout << "               -> Found " << specCount << " doctor(s) matching Specialty '" << specCond << "'\n";

            // Intersect the two primary key lists
            vector<string> matchedKeys;
            for (int i = 0; i < nameCount; i++) {
                string k1 = cleanFixedField(nameIDs[i], KEY_SIZE);
                for (int j = 0; j < specCount; j++) {
                    string k2 = cleanFixedField(specIDs[j], KEY_SIZE);
                    if (k1 == k2) {
                        matchedKeys.push_back(k1);
                        break;
                    }
                }
            }
            cout << "               -> Intersection size: " << matchedKeys.size() << " doctor(s).\n";

            // Resolve each matching primary key via Primary Index
            for (const string& key : matchedKeys) {
                int offset = searchPrimaryIndex(doctorPrimaryIndex, key.c_str());
                if (offset != -1) {
                    Doctor d;
                    docFile.seekg(offset, ios::beg);
                    if (docFile.read(reinterpret_cast<char*>(&d), sizeof(Doctor)) && d.ID[0] != '*') {
                        if (addrCond.empty() || cleanFixedField(d.address, sizeof(d.address)) == addrCond) {
                            results.push_back(d);
                        }
                    }
                }
            }
        }
        // 3. Secondary Index Search: Doctor Name
        else if (!nameCond.empty()) {
            cout << "[Query Engine] Strategy: Secondary Index on Doctor Name (Linked List on File)...\n";
            char docIDs[MAX_NAME_INDEX_SIZE][KEY_SIZE];
            int count = searchDoctorNameIndex(nameCond.c_str(), docIDs, MAX_NAME_INDEX_SIZE);
            cout << "               -> Found " << count << " doctor(s) in Doctor Name index.\n";

            for (int i = 0; i < count; i++) {
                string key = cleanFixedField(docIDs[i], KEY_SIZE);
                int offset = searchPrimaryIndex(doctorPrimaryIndex, key.c_str());
                if (offset != -1) {
                    Doctor d;
                    docFile.seekg(offset, ios::beg);
                    if (docFile.read(reinterpret_cast<char*>(&d), sizeof(Doctor)) && d.ID[0] != '*') {
                        if (addrCond.empty() || cleanFixedField(d.address, sizeof(d.address)) == addrCond) {
                            results.push_back(d);
                        }
                    }
                }
            }
        }
        // Secondary Index Search: Doctor Specialty
        else if (!specCond.empty()) {
            cout << "[Query Engine] Strategy: Secondary Index on Doctor Specialty (Linked List on File)...\n";
            char docIDs[MAX_SPECIALTY_INDEX_SIZE][KEY_SIZE];
            int count = searchDoctorSpecialtyIndex(specCond.c_str(), docIDs, MAX_SPECIALTY_INDEX_SIZE);
            cout << "               -> Found " << count << " doctor(s) in Doctor Specialty index.\n";

            for (int i = 0; i < count; i++) {
                string key = cleanFixedField(docIDs[i], KEY_SIZE);
                int offset = searchPrimaryIndex(doctorPrimaryIndex, key.c_str());
                if (offset != -1) {
                    Doctor d;
                    docFile.seekg(offset, ios::beg);
                    if (docFile.read(reinterpret_cast<char*>(&d), sizeof(Doctor)) && d.ID[0] != '*') {
                        if (addrCond.empty() || cleanFixedField(d.address, sizeof(d.address)) == addrCond) {
                            results.push_back(d);
                        }
                    }
                }
            }
        }
        // Non-indexed condition (Prohibited sequential scan)
        else {
            cout << "[Query Error] Full sequential file scan is strictly prohibited.\n";
            cout << "              WHERE condition must filter on an indexed field:\n";
            cout << "              - Doctor ID (Primary Index)\n";
            cout << "              - Doctor Name (Secondary Index)\n";
            cout << "              - Doctor Specialty (Secondary Index)\n";
            return false;
        }

        // Display Doctor Results
        if (results.empty()) {
            cout << "No records found matching the specified query criteria.\n";
            return true;
        }

        // Determine column widths
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

        printDoctorHeader(headers, widths);
        for (const auto& d : results) {
            printDoctorRow(d, pq, widths);
        }
        printDivider(widths);
        cout << "(" << results.size() << " record(s) returned)\n";
        return true;
    }

    else if (pq.table == QueryTable::APPOINTMENTS) {
        // Classify conditions
        string appIdCond = "";
        string docIdCond = "";
        string dateCond = "";

        for (const auto& c : pq.conditions) {
            if (c.field == "Appointment ID") appIdCond = c.value;
            else if (c.field == "Doctor ID") docIdCond = c.value;
            else if (c.field == "Appointment Date") dateCond = c.value;
        }

        vector<Appointment> results;
        ifstream appFile("app.dat", ios::binary);
        if (!appFile) {
            cout << "[Query Error] Could not open app.dat file.\n";
            return false;
        }

        // 1. Primary Index Search: Appointment ID
        if (!appIdCond.empty()) {
            cout << "[Query Engine] Strategy: Binary Search on Appointment Primary Index (O(log N))...\n";
            int offset = searchPrimaryIndex(appointmentPrimaryIndex, appIdCond.c_str());
            if (offset != -1) {
                Appointment a;
                appFile.seekg(offset, ios::beg);
                if (appFile.read(reinterpret_cast<char*>(&a), sizeof(Appointment)) && a.ID[0] != '*') {
                    bool match = true;
                    if (!docIdCond.empty() && cleanFixedField(a.doctor_id, sizeof(a.doctor_id)) != docIdCond) match = false;
                    if (!dateCond.empty() && cleanFixedField(a.date, sizeof(a.date)) != dateCond) match = false;
                    if (match) {
                        results.push_back(a);
                    }
                }
            }
        }
        // 2. Secondary Index Search: Doctor ID in Appointments
        else if (!docIdCond.empty()) {
            cout << "[Query Engine] Strategy: Secondary Index on Doctor ID in Appointments (Linked List on File)...\n";
            char appIDs[MAX_APPOINTMENT_DOCTOR_INDEX_SIZE][KEY_SIZE];
            int count = searchAppointmentsByDoctorID(docIdCond.c_str(), appIDs, MAX_APPOINTMENT_DOCTOR_INDEX_SIZE);
            cout << "               -> Found " << count << " appointment(s) for Doctor ID '" << docIdCond << "'.\n";

            for (int i = 0; i < count; i++) {
                string appKey = cleanFixedField(appIDs[i], KEY_SIZE);
                int offset = searchPrimaryIndex(appointmentPrimaryIndex, appKey.c_str());
                if (offset != -1) {
                    Appointment a;
                    appFile.seekg(offset, ios::beg);
                    if (appFile.read(reinterpret_cast<char*>(&a), sizeof(Appointment)) && a.ID[0] != '*') {
                        if (dateCond.empty() || cleanFixedField(a.date, sizeof(a.date)) == dateCond) {
                            results.push_back(a);
                        }
                    }
                }
            }
        }
        // Non-indexed condition (Prohibited sequential scan)
        else {
            cout << "[Query Error] Full sequential file scan is strictly prohibited.\n";
            cout << "              WHERE condition must filter on an indexed field:\n";
            cout << "              - Appointment ID (Primary Index)\n";
            cout << "              - Doctor ID (Secondary Index)\n";
            return false;
        }

        // Display Appointment Results
        if (results.empty()) {
            cout << "No records found matching the specified query criteria.\n";
            return true;
        }

        // Determine column widths
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

        printDoctorHeader(headers, widths);
        for (const auto& a : results) {
            printAppointmentRow(a, pq, widths);
        }
        printDivider(widths);
        cout << "(" << results.size() << " record(s) returned)\n";
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

// ============================================================================
// UI & MAIN MENU IMPLEMENTATION
// ============================================================================

void showWelcomeScreen() {
    cout << "\n";
    cout << "===============================================================================\n";
    cout << "||                                                                           ||\n";
    cout << "||                   HEALTHCARE MANAGEMENT SYSTEM (IS321)                    ||\n";
    cout << "||                       File Management & Processing                        ||\n";
    cout << "||                                                                           ||\n";
    cout << "===============================================================================\n";
    cout << "|| Team Responsibilities:                                                    ||\n";
    cout << "||  - Member 1: File Initialization, Record Insert/Delete, Avail List        ||\n";
    cout << "||  - Member 2: Primary Indexes (Doctors & Appointments), Binary Search      ||\n";
    cout << "||  - Member 3: Update Operations, Doctor Name Secondary Index               ||\n";
    cout << "||  - Member 4: Doctor Specialty & Appointment Secondary Indexes, Rebuild   ||\n";
    cout << "||  - Member 5: SQL Query Engine, User Interface & Integration Layer         ||\n";
    cout << "===============================================================================\n\n";
}

void displayMainMenu() {
    cout << "\n--------------------------- MAIN MENU ---------------------------\n";
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
    cout << "-----------------------------------------------------------------\n";
    cout << "Enter your choice (1-11): ";
}

void handleDeleteAppointmentMenu() {
    char id[KEY_SIZE];
    cout << "\n--- Delete Appointment ---\n";
    cout << "Enter Appointment ID to delete: ";
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
    cout << "\n--- Delete Doctor ---\n";
    cout << "Enter Doctor ID to delete: ";
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
    cout << "\n--- Print Doctor Info ---\n";
    cout << "Enter Doctor ID: ";
    cin >> ws;
    cin.getline(id, KEY_SIZE);

    int offset = searchPrimaryIndex(doctorPrimaryIndex, id);
    if (offset == -1) {
        cout << "Error: Doctor with ID '" << id << "' not found in Primary Index.\n";
        return;
    }

    ifstream f("doctor.dat", ios::binary);
    if (!f) {
        cout << "Error: Could not open doctor.dat file.\n";
        return;
    }

    Doctor d;
    f.seekg(offset, ios::beg);
    if (f.read(reinterpret_cast<char*>(&d), sizeof(Doctor)) && d.ID[0] != '*') {
        cout << "\n========== Doctor Record ==========\n";
        cout << "  Doctor ID      : " << cleanFixedField(d.ID, sizeof(d.ID)) << "\n";
        cout << "  Doctor Name    : " << cleanFixedField(d.name, sizeof(d.name)) << "\n";
        cout << "  Specialization : " << cleanFixedField(d.specialization, sizeof(d.specialization)) << "\n";
        cout << "  Address        : " << cleanFixedField(d.address, sizeof(d.address)) << "\n";
        cout << "  File Offset    : " << offset << " bytes\n";
        cout << "===================================\n";
    } else {
        cout << "Error: Record is deleted or unreadable.\n";
    }
    f.close();
}

void handlePrintAppointmentInfoMenu() {
    char id[KEY_SIZE];
    cout << "\n--- Print Appointment Info ---\n";
    cout << "Enter Appointment ID: ";
    cin >> ws;
    cin.getline(id, KEY_SIZE);

    int offset = searchPrimaryIndex(appointmentPrimaryIndex, id);
    if (offset == -1) {
        cout << "Error: Appointment with ID '" << id << "' not found in Primary Index.\n";
        return;
    }

    ifstream f("app.dat", ios::binary);
    if (!f) {
        cout << "Error: Could not open app.dat file.\n";
        return;
    }

    Appointment a;
    f.seekg(offset, ios::beg);
    if (f.read(reinterpret_cast<char*>(&a), sizeof(Appointment)) && a.ID[0] != '*') {
        cout << "\n======== Appointment Record ========\n";
        cout << "  Appointment ID : " << cleanFixedField(a.ID, sizeof(a.ID)) << "\n";
        cout << "  Date           : " << cleanFixedField(a.date, sizeof(a.date)) << "\n";
        cout << "  Doctor ID      : " << cleanFixedField(a.doctor_id, sizeof(a.doctor_id)) << "\n";
        cout << "  File Offset    : " << offset << " bytes\n";
        cout << "====================================\n";
    } else {
        cout << "Error: Record is deleted or unreadable.\n";
    }
    f.close();
}

void handleWriteQueryMenu() {
    cout << "\n=================================================================\n";
    cout << "                        SQL QUERY ENGINE                         \n";
    cout << "=================================================================\n";
    cout << "Supported Query Examples:\n";
    cout << "  1. SELECT ALL FROM Doctors WHERE Doctor ID='xxx';\n";
    cout << "  2. SELECT ALL FROM Appointments WHERE Doctor ID='xxx';\n";
    cout << "  3. SELECT Doctor Name FROM Doctors WHERE Doctor Name='xxx';\n";
    cout << "  4. SELECT ALL FROM Doctors WHERE Doctor Name='xxx' AND Doctor Specialty='yyy';\n";
    cout << "-----------------------------------------------------------------\n";
    cout << "Enter query (or type 'back' to return to menu):\n> ";

    string query;
    cin >> ws;
    getline(cin, query);

    if (query.empty() || toUpper(query) == "BACK") {
        return;
    }

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
            case 1:
                add_doctor();
                break;
            case 2:
                add_appointment();
                break;
            case 3:
                update_doctor_name();
                break;
            case 4:
                update_doctor_id();
                break;
            case 5:
                update_appointment_date();
                break;
            case 6:
                handleDeleteAppointmentMenu();
                break;
            case 7:
                handleDeleteDoctorMenu();
                break;
            case 8:
                handlePrintDoctorInfoMenu();
                break;
            case 9:
                handlePrintAppointmentInfoMenu();
                break;
            case 10:
                handleWriteQueryMenu();
                break;
            case 11:
                cout << "\nThank you for using the Healthcare Management System. Goodbye!\n";
                break;
            default:
                cout << "Invalid choice. Please select an option between 1 and 11.\n";
                break;
        }
    }
}

// ============================================================================
// SAMPLE DATA SEEDER (HELPER FOR INITIALIZATION / TESTING)
// ============================================================================

void seedSampleDataIfEmpty() {
    if (doctorPrimaryIndex.count > 0 || appointmentPrimaryIndex.count > 0) {
        return; // Data already exists
    }

    cout << "[Initialization] Database is currently empty.\n";
    cout << "[Initialization] Seeding initial 5 Doctors and 10 Appointments...\n";

    // 5 Doctors (Exactly 95 bytes each)
    Doctor sampleDocs[5];
    memset(sampleDocs, 0, sizeof(sampleDocs));

    strcpy(sampleDocs[0].ID, "D001");
    strcpy(sampleDocs[0].name, "Dr. Ahmed Hassan");
    strcpy(sampleDocs[0].specialization, "Cardiology");
    strcpy(sampleDocs[0].address, "10 Tahrir St, Cairo");

    strcpy(sampleDocs[1].ID, "D002");
    strcpy(sampleDocs[1].name, "Dr. Sara Mohamed");
    strcpy(sampleDocs[1].specialization, "Pediatrics");
    strcpy(sampleDocs[1].address, "25 El-Nasr Rd, Giza");

    strcpy(sampleDocs[2].ID, "D003");
    strcpy(sampleDocs[2].name, "Dr. John Smith");
    strcpy(sampleDocs[2].specialization, "Cardiology");
    strcpy(sampleDocs[2].address, "14 Horreya Ave, Alex");

    strcpy(sampleDocs[3].ID, "D004");
    strcpy(sampleDocs[3].name, "Dr. Mona Ali");
    strcpy(sampleDocs[3].specialization, "Dermatology");
    strcpy(sampleDocs[3].address, "8 El-Galaa St, Cairo");

    strcpy(sampleDocs[4].ID, "D005");
    strcpy(sampleDocs[4].name, "Dr. John Smith");
    strcpy(sampleDocs[4].specialization, "Orthopedics");
    strcpy(sampleDocs[4].address, "30 Abbasia, Cairo");

    ofstream docFile("doctor.dat", ios::binary | ios::trunc);
    int availHead = -1;
    docFile.write(reinterpret_cast<char*>(&availHead), sizeof(int));
    for (int i = 0; i < 5; i++) {
        docFile.write(reinterpret_cast<char*>(&sampleDocs[i]), sizeof(Doctor));
    }
    docFile.close();

    // 10 Appointments (Exactly 60 bytes each)
    Appointment sampleApps[10];
    memset(sampleApps, 0, sizeof(sampleApps));

    strcpy(sampleApps[0].ID, "A001");
    strcpy(sampleApps[0].date, "2026-11-20 09:00");
    strcpy(sampleApps[0].doctor_id, "D001");

    strcpy(sampleApps[1].ID, "A002");
    strcpy(sampleApps[1].date, "2026-11-20 10:30");
    strcpy(sampleApps[1].doctor_id, "D001");

    strcpy(sampleApps[2].ID, "A003");
    strcpy(sampleApps[2].date, "2026-11-21 11:00");
    strcpy(sampleApps[2].doctor_id, "D002");

    strcpy(sampleApps[3].ID, "A004");
    strcpy(sampleApps[3].date, "2026-11-21 14:00");
    strcpy(sampleApps[3].doctor_id, "D002");

    strcpy(sampleApps[4].ID, "A005");
    strcpy(sampleApps[4].date, "2026-11-22 09:30");
    strcpy(sampleApps[4].doctor_id, "D003");

    strcpy(sampleApps[5].ID, "A006");
    strcpy(sampleApps[5].date, "2026-11-22 15:00");
    strcpy(sampleApps[5].doctor_id, "D003");

    strcpy(sampleApps[6].ID, "A007");
    strcpy(sampleApps[6].date, "2026-11-23 10:00");
    strcpy(sampleApps[6].doctor_id, "D004");

    strcpy(sampleApps[7].ID, "A008");
    strcpy(sampleApps[7].date, "2026-11-23 12:30");
    strcpy(sampleApps[7].doctor_id, "D004");

    strcpy(sampleApps[8].ID, "A009");
    strcpy(sampleApps[8].date, "2026-11-24 11:00");
    strcpy(sampleApps[8].doctor_id, "D005");

    strcpy(sampleApps[9].ID, "A010");
    strcpy(sampleApps[9].date, "2026-11-24 16:00");
    strcpy(sampleApps[9].doctor_id, "D001");

    ofstream appFile("app.dat", ios::binary | ios::trunc);
    appFile.write(reinterpret_cast<char*>(&availHead), sizeof(int));
    for (int i = 0; i < 10; i++) {
        appFile.write(reinterpret_cast<char*>(&sampleApps[i]), sizeof(Appointment));
    }
    appFile.close();

    // Rebuild all RAM indexes from the newly populated files
    rebuildIndexes();

    cout << "[Initialization] Seeding complete: 5 Doctors and 10 Appointments loaded.\n";
}
