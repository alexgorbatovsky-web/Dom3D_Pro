#pragma once
#include <stdio.h>

#include <cmath> 

#define OK 0x00
#define BAD 1

#define DDELTA 0.000001
#define BUFSIZE 180
#define LONGBUFSIZE 1024

#define  message_astra message_to_file

#define message_error_ message_to_file

//extern void Message_err(const char* message);
#define Message_err  message_to_file
#define IDS_BAD_ALLOC_MEMORY "Bad alloc memory"

void message_to_file(const char* text);

inline int __IntAbs(const int n) {
    int s = n >> 31;
    return (n ^ s) - s;
}

