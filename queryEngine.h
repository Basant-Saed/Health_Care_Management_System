#ifndef QUERY_ENGINE_H
#define QUERY_ENGINE_H

#include <string>
#include <vector>
#include "structs.h"
#include "indexManager.h"
#include "secondaryIndex.h"
#include "doctorManager.h"
#include "appointmentManager.h"

// ============================================================================
// IS321 - Healthcare Management System
// Member 5 Responsibilities:
//   1. SQL-like Query Engine:
//      - Query parser for SELECT ... FROM ... WHERE ... [AND ...]
//      - Table, field, condition, and value identification
//      - Intelligent index selection (Doctor/Appointment Primary Indexes,
//        Doctor Name Secondary Index, Doctor Specialty Secondary Index,
//        Appointment Doctor ID Secondary Index, or Multiple Secondary Indexes)
//      - No full sequential scans when indexes apply
//      - Display of matching results or clear error messages
//   2. Welcome Screen & Main Menu (Options 1 to 11)
//   3. Integration Layer connecting with Member 1, 2, 3, and 4 functions
// ============================================================================

// Target tables supported by the Query Engine
enum class QueryTable {
    UNKNOWN,
    DOCTORS,
    APPOINTMENTS
};

// Condition representation in WHERE clause: Field = 'Value'
struct QueryCondition {
    std::string field;   // Canonical field name (e.g. "Doctor ID", "Doctor Name")
    std::string value;   // Literal search value (e.g. "D001", "Cardiology")
};

// Parsed SQL query structure
struct ParsedQuery {
    bool isValid;
    std::string errorMessage;
    QueryTable table;
    std::vector<std::string> selectedFields; // "ALL" or individual canonical field names
    std::vector<QueryCondition> conditions;
    bool isSelectAll;
};

// ============================================================================
// QUERY ENGINE API
// ============================================================================

// Parses an SQL-like query string into a ParsedQuery structure
ParsedQuery parseQuery(const std::string& queryString);

// Executes a parsed query, selects appropriate indexes, and displays results
bool executeParsedQuery(const ParsedQuery& query);

// High-level entry point: parses and executes an SQL-like query string
bool executeQuery(const std::string& queryString);


// ============================================================================
// UI & MENU API
// ============================================================================

// Displays the formatted system welcome screen
void showWelcomeScreen();

// Displays the main menu options (1 to 11)
void displayMainMenu();

// Runs the main menu loop until user chooses Exit (Option 11)
void runMainMenu();

// Menu action handlers (integrating with teammates' functions)
void handleDeleteAppointmentMenu();
void handleDeleteDoctorMenu();
void handlePrintDoctorInfoMenu();
void handlePrintAppointmentInfoMenu();
void handleWriteQueryMenu();

// Helper to seed initial 5 doctors and 10 appointments if database is empty
void seedSampleDataIfEmpty();

#endif // QUERY_ENGINE_H
