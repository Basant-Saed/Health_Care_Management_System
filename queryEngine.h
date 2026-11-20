#ifndef QUERY_ENGINE_H
#define QUERY_ENGINE_H

#include <string>
#include <vector>
#include "structs.h"
#include "indexManager.h"
#include "secondaryIndex.h"
#include "doctorManager.h"
#include "appointmentManager.h"

// Target tables for queries
enum class QueryTable {
    UNKNOWN,
    DOCTORS,
    APPOINTMENTS
};

// Condition in WHERE clause (Field = 'Value')
struct QueryCondition {
    std::string field;
    std::string value;
};

// Parsed SQL query data
struct ParsedQuery {
    bool isValid;
    std::string errorMessage;
    QueryTable table;
    std::vector<std::string> selectedFields;
    std::vector<QueryCondition> conditions;
    bool isSelectAll;
};

// Query Engine Functions
ParsedQuery parseQuery(const std::string& queryString);
bool executeParsedQuery(const ParsedQuery& query);
bool executeQuery(const std::string& queryString);

// UI and Menu Functions
void showWelcomeScreen();
void displayMainMenu();
void runMainMenu();
void handleDeleteAppointmentMenu();
void handleDeleteDoctorMenu();
void handlePrintDoctorInfoMenu();
void handlePrintAppointmentInfoMenu();
void handleWriteQueryMenu();

// Helper to seed initial sample data if files are empty
void seedSampleDataIfEmpty();

#endif
