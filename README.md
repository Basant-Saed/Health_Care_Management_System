# Healthcare Management System (IS321)

[![C++ Standard](https://img.shields.io/badge/C%2B%2B-17-blue.svg?style=flat-square&logo=c%2B%2B)](https://en.cppreference.com/w/cpp/17)
[![Course](https://img.shields.io/badge/Course-IS321%20File%20Management-purple.svg?style=flat-square)](#project-overview)
[![Build Status](https://img.shields.io/badge/Build-Passing-brightgreen.svg?style=flat-square)](#installation--execution)
[![License](https://img.shields.io/badge/License-Academic-lightgrey.svg?style=flat-square)](#learning-outcomes)

A high-performance, low-level Healthcare Management System built in **modern C++** for the **IS321: File Management and Processing** university curriculum. 

The system implements database storage engine internals from scratch without relying on external database libraries. It manages Doctor and Appointment entities through direct binary file I/O, fixed-length binary records, in-memory sorted primary indexes with $O(\log N)$ binary search, disk-based secondary indexes using inverted linked lists, an AVAIL list for logical deletion and space recycling, and an integrated SQL-like query parsing engine.

---

## Table of Contents

- [Project Overview](#project-overview)
- [Key Features](#key-features)
- [System Architecture & File Structures](#system-architecture--file-structures)
- [Core File Management Concepts](#core-file-management-concepts)
  - [1. Fixed-Length Binary Records](#1-fixed-length-binary-records)
  - [2. AVAIL List & Record Space Recycling](#2-avail-list--record-space-recycling)
  - [3. Primary Indexing & Binary Search](#3-primary-indexing--binary-search)
  - [4. Secondary Indexing via "Linked List on a Different File"](#4-secondary-indexing-via-linked-list-on-a-different-file)
  - [5. SQL Query Engine & Index Selection](#5-sql-query-engine--index-selection)
- [Project Structure](#project-structure)
- [Technologies Used](#technologies-used)
- [Installation & Execution](#installation--execution)
- [How to Use & System Walkthrough](#how-to-use--system-walkthrough)
  - [Main Menu Operations](#main-menu-operations)
  - [SQL Query Engine Examples](#sql-query-engine-examples)
- [Verification & Automated Testing](#verification--automated-testing)
- [Learning Outcomes](#learning-outcomes)
- [Team & Contributors](#team--contributors)

---

## Project Overview

In production database management systems (DBMS), efficient data persistence and fast retrieval require specialized storage structures to avoid full-file scans ($O(N)$). 

This project explores the internal mechanics of database storage engines by developing a **Healthcare Management System** in C++ that manages hospital doctor rosters and patient appointment bookings. Rather than using standard relational databases (such as MySQL or SQLite), all storage, record positioning, indexing, deletion re-use, and query execution are implemented manually at the byte level.

### Managed Entities

1. **Doctors**: Doctors registered in the clinic with unique IDs, names, medical specializations, and clinic addresses.
2. **Appointments**: Scheduled clinic visits identified by unique Appointment IDs, booking timestamps, and foreign-key references to Doctor IDs.

---

## Key Features

### 🗂️ Record Storage & Space Management
- **Direct Binary File Serialization**: Records are stored as byte-packed binary data (`.dat` files) using `std::fstream` in `ios::binary` mode.
- **Fixed-Length Records**: Guaranteed deterministic byte sizes (95 bytes for Doctors, 60 bytes for Appointments) allowing instant $O(1)$ random access by record offset.
- **AVAIL List Space Recycling**: Logically deleted records are added to a Last-In, First-Out (LIFO) free list linked inside the data file. New records immediately reuse free slots before the file is expanded.

### ⚡ Multi-Level Indexing Subsystem
- **Primary Indexes**: In-memory tables sorted by unique primary keys (`Doctor ID` and `Appointment ID`) referencing direct file byte offsets.
- **$O(\log N)$ Binary Search**: High-speed record lookup avoiding sequential file traversal.
- **Secondary Indexes**: Secondary search keys (`Doctor Name`, `Doctor Specialty`, and Appointment `Doctor ID`) implemented using the **Linked List on a Different File** technique.
- **Decoupled Architecture**: Secondary indexes reference Primary Keys rather than physical byte offsets, isolating index stability from underlying record relocations.
- **Startup Index Reconstruction**: Automatic re-indexing engine parses binary data files on application startup to rehydrate all RAM and secondary index files.

### 🔍 SQL Query Engine
- **Declarative Query Processing**: Accepts standard SQL-like `SELECT` queries with support for projected attributes, `WHERE` equality filters, and multiple `AND` conditions.
- **Query Optimization & Plan Selection**: Analyzes query predicates and routes execution to the fastest index:
  - Primary Key query $\rightarrow$ Primary Index Binary Search ($O(\log N)$).
  - Secondary Key query $\rightarrow$ Secondary Inverted List traversal.
  - Multi-condition query $\rightarrow$ Secondary Index Intersection algorithm.
- **Sequential Scan Prevention**: Strictly rejects queries on non-indexed columns, enforcing index-based access patterns.

### 🖥️ Interactive CLI & Test Harness
- Comprehensive 11-option console interface for end users.
- Automated test suites validating parser correctness, index updates, and query result accuracy.

---

## System Architecture & File Structures

```
+---------------------------------------------------------------------------------+
|                                 APPLICATION UI                                  |
|                  Main Menu (11 Options)  |  SQL Query Engine                    |
+---------------------------------------------------------------------------------+
                                       |
                                       v
+---------------------------------------------------------------------------------+
|                                INDEXING LAYER                                   |
|   +------------------------------------+   +--------------------------------+   |
|   |         Primary Indexes            |   |       Secondary Indexes        |   |
|   |  - Doctor Primary Index (RAM)      |   |  - Doctor Name Index           |   |
|   |  - Appointment Primary Index (RAM) |   |  - Doctor Specialty Index      |   |
|   |  (Sorted Array + Binary Search)    |   |  - Appointment Doctor Index    |   |
|   +-----------------+------------------+   +---------------+----------------+   |
+---------------------|--------------------------------------|--------------------+
                      | Offset                               | Primary Key (ID)
                      v                                      v
+---------------------------------------------------------------------------------+
|                                 STORAGE LAYER                                   |
|   +-----------------------------------+   +---------------------------------+   |
|   |          Data Files               |   |      Inverted List Files        |   |
|   |  - doctor.dat (Header + Records)  |   |  - doctor_name_list.dat         |   |
|   |  - app.dat    (Header + Records)  |   |  - doctor_specialty_list.dat    |   |
|   |  (Fixed-Length Records + AVAIL)   |   |  - appointment_doctor_list.dat  |   |
|   +-----------------------------------+   +---------------------------------+   |
+---------------------------------------------------------------------------------+
```

---

## Core File Management Concepts

### 1. Fixed-Length Binary Records

All data structures use `#pragma pack(push, 1)` to eliminate compiler byte padding, ensuring consistent binary sizes across platforms:

```cpp
#pragma pack(push, 1)
struct Doctor {
    char ID[15];              // Primary Key
    char name[30];            // Doctor Full Name
    char specialization[20];  // Medical Specialty
    char address[30];         // Clinic Address
}; // Exactly 95 bytes

struct Appointment {
    char ID[15];              // Primary Key
    char date[30];            // Booking Date & Time string
    char doctor_id[15];       // Foreign Key referencing Doctor.ID
}; // Exactly 60 bytes
#pragma pack(pop)
```

Each data file begins with a 4-byte integer header (`availHead`) storing the byte offset of the first free deleted record. Actual data records start at byte offset `4`.

$$\text{Record Offset}(i) = 4 + (i \times \text{sizeof(Record)})$$

---

### 2. AVAIL List & Record Space Recycling

To avoid expensive file compaction during record deletions, the system implements an in-file **AVAIL List** (a LIFO linked stack):

1. **Header Record**: The first 4 bytes of `doctor.dat` and `app.dat` store `availHead` (initialized to `-1`).
2. **Logical Deletion**:
   - The first byte of the record's primary key is overwritten with `'*'`.
   - The subsequent 4 bytes store the current value of `availHead`.
   - The file header is updated so `availHead` points to this newly deleted record's offset.
3. **Record Reuse on Insertion**:
   - When inserting a new record, the system checks `availHead`.
   - If `availHead != -1`, the system reads the next available offset stored inside the deleted slot, overwrites the deleted slot with the new record, and updates `availHead`.
   - If `availHead == -1`, the record is appended to the end of the file (`EOF`).

```
Initial State:
[Header: availHead = -1] -> [Rec 0: D001] -> [Rec 1: D002] -> [Rec 2: D003]

After Deleting Rec 1 (Offset 99):
[Header: availHead = 99] -> [Rec 1: '*' + next(-1)]

After Deleting Rec 0 (Offset 4):
[Header: availHead = 4]  -> [Rec 0: '*' + next(99)] -> [Rec 1: '*' + next(-1)]

On Next Insert:
Writes into Offset 4; Updates Header: availHead = 99. Zero storage wasted!
```

---

### 3. Primary Indexing & Binary Search

Primary indexes reside in memory as sorted arrays:

```cpp
struct PrimaryIndexEntry {
    char key[15];
    int offset; // Direct byte offset in binary data file
};

struct PrimaryIndex {
    PrimaryIndexEntry entries[MAX_INDEX_SIZE];
    int count;
};
```

- **Lookup ($O(\log N)$)**: `binarySearchPrimary()` locates any key using standard bisection.
- **Insertion**: Uses binary search to find the correct insertion position in $O(\log N)$, followed by a right-shift to maintain strict ascending sort order. Duplicate primary keys are rejected.
- **Deletion**: Locates the index entry in $O(\log N)$ and shifts subsequent entries left by one.

---

### 4. Secondary Indexing via "Linked List on a Different File"

Because secondary keys (such as `Doctor Name` or `Specialization`) are non-unique, one secondary key can map to multiple records. Storing variable-length arrays inside the index causes severe internal fragmentation.

Instead, the system utilizes the **Linked List on a Different File** technique:

1. **In-Memory Secondary Index Table**: Contains unique secondary keys sorted alphabetically, paired with a `head` pointer (Relative Record Number - RRN) into the inverted list file.
2. **Inverted List File (`*_list.dat`)**: Stores fixed-size linked nodes (`SecondaryNode`):
   ```cpp
   struct SecondaryNode {
       char id[15]; // Primary Key of matching record
       int next;    // Next RRN in chain (-1 if end of list)
   };
   ```

```
Secondary Index (RAM)           Inverted List File (*_list.dat)
+----------------+------+       +-----+------------+------+
| Key            | Head | ----> | RRN | Doctor ID  | Next |
+----------------+------+       +-----+------------+------+
| Cardiology     |  0   |       |  0  | D001       |  2   |
| Pediatrics     |  1   |       |  1  | D002       | -1   |
+----------------+------+       |  2  | D003       | -1   |
                                +-----+------------+------+
Cardiology chain: RRN 0 (D001) -> RRN 2 (D003) -> End (-1)
```

> **Key Architectural Advantage**: Secondary nodes store **Primary Keys** (`id`), not direct byte addresses. When records in the data file are moved, updated, or rebuilt, the secondary inverted lists remain completely valid.

---

### 5. SQL Query Engine & Index Selection

The SQL engine parses user queries and maps them to the optimal indexing strategy:

| Query Example | Execution Strategy | Complexity |
| :--- | :--- | :---: |
| `SELECT ALL FROM Doctors WHERE Doctor ID='D001';` | Binary Search on Doctor Primary Index | $O(\log N)$ |
| `SELECT ALL FROM Appointments WHERE Doctor ID='D001';` | Traverse Appointment Doctor Secondary Inverted List $\rightarrow$ Fetch via Appointment Primary Index | $O(K \log M)$ |
| `SELECT Doctor Name FROM Doctors WHERE Doctor Name='Dr. Ahmed Hassan';` | Traverse Doctor Name Secondary Inverted List $\rightarrow$ Fetch & project `Doctor Name` | $O(K \log N)$ |
| `SELECT ALL FROM Doctors WHERE Doctor Name='...' AND Doctor Specialty='...';` | Retrieve IDs from Name index & Specialty index $\rightarrow$ Compute Set Intersection $\rightarrow$ Fetch Records | $O(K_1 + K_2 + I \log N)$ |
| `SELECT ALL FROM Doctors WHERE Doctor Address='Cairo';` | **REJECTED**: Full sequential scan prohibited | N/A (Error) |

---

## Project Structure

```
Health_Care_Management_System/
│
├── structs.h                  # Memory-packed struct definitions (Doctor, Appointment)
├── main.cpp                   # Application entry point & startup sequence
│
├── doctorManager.h            # Doctor file operations & AVAIL list interface
├── doctorManager.cpp          # Add, delete, update doctor records & manage doctor.dat
│
├── appointmentManager.h       # Appointment file operations interface
├── appointmentManager.cpp     # Add, delete, update appointments & manage app.dat
│
├── indexManager.h             # Primary index structures & binary search declarations
├── indexManager.cpp           # Primary index maintenance (sorted insert, delete, search)
│
├── secondaryIndex.h           # Secondary index structs & inverted list interface
├── secondaryIndex.cpp         # Secondary index operations (Name, Specialty, App-Doctor)
│
├── queryEngine.h              # SQL parser definitions, UI menu prototypes
├── queryEngine.cpp            # Query parsing, index execution strategies & console UI
│
├── test.cpp                   # Primary index verification test harness
├── test_member5.cpp           # Query Engine and integration test suite (32 tests)
│
├── .gitignore                 # Git ignore rules for compiled artifacts and binaries
└── README.md                  # Project documentation and technical manual
```

### Runtime Generated Data Files

| File | Purpose |
| :--- | :--- |
| `doctor.dat` | Binary data file for Doctor records (4-byte header + 95-byte records). |
| `app.dat` | Binary data file for Appointment records (4-byte header + 60-byte records). |
| `doctor_name_list.dat` | Inverted list file chaining Doctor IDs by Doctor Name. |
| `doctor_specialty_list.dat` | Inverted list file chaining Doctor IDs by Specialization. |
| `appointment_doctor_list.dat` | Inverted list file chaining Appointment IDs by Doctor ID. |

---

## Technologies Used

- **Language**: C++ (C++17 standard)
- **Compiler Support**: GCC / MinGW (`g++`), Clang, MSVC
- **Standard Library Modules**:
  - `<fstream>`: Low-level binary file I/O operations (`read`, `write`, `seekg`, `seekp`, `tellg`, `tellp`).
  - `<iostream>`, `<iomanip>`: Formatted table presentation and user interaction.
  - `<cstring>`: Precise fixed-size memory operations (`memset`, `memcpy`, `strncmp`).
  - `<vector>`, `<string>`: Query parsing and tokenization.
- **Zero External Dependencies**: Pure native C++ with no third-party database engines or external packages.

---

## Installation & Execution

### Prerequisites

- A modern C++ compiler supporting **C++17** (e.g., GCC 7+, Clang 5+, or Visual Studio 2017+).
- Git installed on your local machine.

### 1. Clone the Repository

```bash
git clone https://github.com/your-username/healthcare-management-system.git
cd healthcare-management-system
```

### 2. Compile the Application

Compile all translation units into the main executable:

```bash
g++ -std=c++17 main.cpp queryEngine.cpp doctorManager.cpp appointmentManager.cpp indexManager.cpp secondaryIndex.cpp -o healthcare_system.exe
```

*(On Linux / macOS, replace `-o healthcare_system.exe` with `-o healthcare_system`)*

### 3. Run the Application

```bash
# Windows
.\healthcare_system.exe

# Linux / macOS
./healthcare_system
```

---

## How to Use & System Walkthrough

Upon startup, the system verifies and initializes `doctor.dat` and `app.dat`, rebuilds all RAM indexes from disk, automatically seeds baseline sample data if the database is newly created, and presents the main interactive menu:

```
=========================================================
          HEALTHCARE MANAGEMENT SYSTEM (IS321)          
              File Management & Processing              
=========================================================

----------------------- MAIN MENU -----------------------
 1. Add New Doctor
 2. Add New Appointment
 3. Update Doctor Name (Doctor ID)
 4. Update Doctor ID (Key Update Test)
 5. Update Appointment Date (Appointment ID)
 6. Delete Appointment (Appointment ID)
 7. Delete Doctor (Doctor ID)
 8. Print Doctor Info (Doctor ID)
 9. Print Appointment Info (Appointment ID)
10. Write Query
11. Exit
---------------------------------------------------------
Enter your choice (1-11):
```

### Main Menu Operations

- **Options 1 & 2 (Add Record)**: Prompts for field data. Automatically verifies primary key uniqueness and assigns space using the AVAIL list or EOF.
- **Options 3, 4 & 5 (Update Operations)**: Updates non-key fields or primary keys. When a primary key changes, the primary index shifts positions, and all referencing secondary lists are automatically updated.
- **Options 6 & 7 (Delete Operations)**: Deletes records logically, inserts them into the AVAIL list stack, and purges entries from all active indexes.
- **Options 8 & 9 (Print Record Info)**: Uses the primary index to look up records in $O(\log N)$ and displays all attributes along with the record's exact byte offset.
- **Option 10 (Write Query)**: Launches the SQL Query Engine.

---

### SQL Query Engine Examples

Choose Option `10` from the main menu to enter SQL mode:

#### Example 1: Primary Index Search
```sql
SELECT ALL FROM Doctors WHERE Doctor ID='D001';
```
```
[Query Engine] Strategy: Binary Search on Doctor Primary Index (O(log N))...
+-----------------+--------------------------------+----------------------+--------------------------------+
| Doctor ID       | Doctor Name                    | Specialization       | Address                        |
+-----------------+--------------------------------+----------------------+--------------------------------+
| D001            | Dr. Ahmed Hassan               | Cardiology           | 10 Tahrir St, Cairo            |
+-----------------+--------------------------------+----------------------+--------------------------------+
(1 record(s) returned)
```

#### Example 2: Secondary Index Search on Appointments
```sql
SELECT ALL FROM Appointments WHERE Doctor ID='D001';
```
```
[Query Engine] Strategy: Secondary Index on Doctor ID in Appointments (Linked List on File)...
               -> Found 3 appointment(s) for Doctor ID 'D001'.
+-----------------+--------------------------------+-----------------+
| App ID          | Appointment Date               | Doctor ID       |
+-----------------+--------------------------------+-----------------+
| A010            | 2026-11-24 16:00               | D001            |
| A002            | 2026-11-20 10:30               | D001            |
| A001            | 2026-11-20 09:00               | D001            |
+-----------------+--------------------------------+-----------------+
(3 record(s) returned)
```

#### Example 3: Secondary Index Search with Attribute Projection
```sql
SELECT Doctor Name FROM Doctors WHERE Doctor Name='Dr. John Smith';
```
```
[Query Engine] Strategy: Secondary Index on Doctor Name (Linked List on File)...
               -> Found 2 doctor(s) in Doctor Name index.
+--------------------------------+
| Doctor Name                    |
+--------------------------------+
| Dr. John Smith                 |
| Dr. John Smith                 |
+--------------------------------+
(2 record(s) returned)
```

#### Example 4: Multiple Secondary Index Intersection
```sql
SELECT ALL FROM Doctors WHERE Doctor Name='Dr. John Smith' AND Doctor Specialty='Cardiology';
```
```
[Query Engine] Strategy: Multiple Secondary Indexes Intersection (Doctor Name + Specialty)...
               -> Found 2 doctor(s) matching Name 'Dr. John Smith'
               -> Found 2 doctor(s) matching Specialty 'Cardiology'
               -> Intersection size: 1 doctor(s).
+-----------------+--------------------------------+----------------------+--------------------------------+
| Doctor ID       | Doctor Name                    | Specialization       | Address                        |
+-----------------+--------------------------------+----------------------+--------------------------------+
| D003            | Dr. John Smith                 | Cardiology           | 14 Horreya Ave, Alex           |
+-----------------+--------------------------------+----------------------+--------------------------------+
(1 record(s) returned)
```

#### Example 5: Prohibited Sequential Scan Handling
```sql
SELECT ALL FROM Doctors WHERE Doctor Address='Cairo';
```
```
[Query Error] Full sequential file scan is strictly prohibited.
              WHERE condition must filter on an indexed field:
              - Doctor ID (Primary Index)
              - Doctor Name (Secondary Index)
              - Doctor Specialty (Secondary Index)
```

---

## Verification & Automated Testing

The project includes two dedicated verification harnesses:

### 1. Comprehensive Query Engine & Integration Suite (`test_member5.cpp`)

Executes 32 automated unit and integration tests covering parser tokenization, invalid SQL handling, primary index lookups, secondary inverted list traversals, and multi-index intersections:

```bash
# Compile and run Member 5 test suite
g++ -std=c++17 test_member5.cpp queryEngine.cpp doctorManager.cpp appointmentManager.cpp indexManager.cpp secondaryIndex.cpp -o test_member5.exe
.\test_member5.exe
```

```
=========================================================
         IS321 MEMBER 5 COMPREHENSIVE TEST SUITE         
=========================================================
[PASS] Parse Query 1 (Doctor Primary Index)
[PASS] Query 1 Table is DOCTORS
[PASS] Query 1 is SELECT ALL
[PASS] Query 1 has 1 condition
[PASS] Parse Query 2 (Appointments by Doctor ID)
[PASS] Parse Query 3 (Doctor Name projection & condition)
[PASS] Parse Query 4 (Multiple Secondary Indexes AND)
[PASS] Catch missing 'FROM'
[PASS] Catch missing 'WHERE'
[PASS] Catch unknown table 'Patients'
[PASS] Catch unknown field 'Salary'
[PASS] Catch malformed condition without '='
...
=========================================================
 FINAL TEST RESULTS: 32 Passed, 0 Failed.
=========================================================
```

### 2. Primary Index Unit Test Suite (`test.cpp`)

Validates array-shifting insertion, bisection search correctness, duplicate key prevention, and in-place deletions:

```bash
g++ -std=c++17 test.cpp doctorManager.cpp appointmentManager.cpp indexManager.cpp secondaryIndex.cpp -o test.exe
.\test.exe
```

Choose Option `11` in the menu to run the 17 automated in-memory unit tests:
```
Running index-only self test...
  [PASS] 5 keys inserted
  [PASS] index is sorted ascending after unsorted inserts
  [PASS] smallest key first, largest key last
  [PASS] binary search finds D7 with the correct offset
  [PASS] binary search returns -1 for a missing key
  [PASS] duplicate key D5 is rejected
  [PASS] delete middle key D5
  [PASS] index still sorted after delete (shift left)
  ...
Result: 17 passed, 0 failed.
```

---

## Learning Outcomes

By building this system from scratch, the project demonstrates mastery of fundamental database engineering and low-level systems programming concepts:

1. **Physical File Structures**: Direct binary serialization, struct memory alignment (`#pragma pack`), and byte-level file positioning via stream pointers (`seekg`/`seekp`).
2. **Space Reclamation Algorithms**: Managing variable-size deletions in constant-size record stores using in-file AVAIL lists.
3. **Index Design Trade-offs**: Understanding the trade-offs between write overhead (maintaining sorted order in memory and disk lists) and read acceleration ($O(\log N)$ versus $O(N)$ table scans).
4. **Relational Inverted Lists**: Practical implementation of the "Linked List on a Different File" pattern to handle 1-to-many relationships without unbounded array padding.
5. **Query Planning & Evaluation**: Lexical parsing of SQL statements, condition extraction, and choosing the optimal index path for query execution.

---

## Team & Contributors

This project was developed as a collaborative team effort for the **IS321: File Management and Processing** course:

| Member | Assigned Responsibilities | Primary Module |
| :--- | :--- | :--- |
| **Basant Saed** (`@Basant-Saed`) | Record Structs, Header Records, Binary File I/O, AVAIL List & Record Deletion/Reuse | `structs.h`, `doctorManager.cpp`, `appointmentManager.cpp` |
| **Shaimaa Mahmoud** (`@Shaimaa-mahm0ud`) | Primary Indexes (Doctors & Appointments), Binary Search, Sorted Array Insertion | `indexManager.h`, `indexManager.cpp` |
| **Sama Salem** (`@Sama-Salem`) | Record Update Operations, Doctor Name Secondary Index | `doctorManager.cpp`, `secondaryIndex.cpp` |
| **Seif Ahmed** (`@Seif`) | Doctor Specialty & Appointment Doctor Secondary Indexes, Startup Index Rebuilder | `secondaryIndex.h`, `secondaryIndex.cpp` |
| **Sara Elbably** (`@SaraElbably-2024`) | SQL Query Engine, Welcome Screen, Main Menu UI, Full System Integration & Testing | `queryEngine.h`, `queryEngine.cpp`, `test_member5.cpp` |

---

*Developed for the Faculty of Computers and Artificial Intelligence, Cairo University (FCAI-CU) — Course IS321.*
