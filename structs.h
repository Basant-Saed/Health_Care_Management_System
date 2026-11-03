#ifndef STRUCTURES_H
#define STRUCTURES_H

#pragma pack(push, 1)           
struct Doctor {
    char ID[15];
    char name[30];
    char specialization[20];
    char address[30];
};

struct Appointment {
    char ID[15];
    char date[30];
    char doctor_id[15];
};

#pragma pack(pop)

#endif