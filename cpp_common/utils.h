#pragma once


#pragma warning(disable : 4996)

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long int u64;


time_t str_to_time(const char* str);
int time_to_str(time_t time, char* buf, int size);
char* time_to_str(time_t time);
int now_to_str(char* buf, int size);
char* now_to_str();
char* _now_to_str();


void open_log_file(const char* file_path);
void close_log_file();
void log_str(const char* str);
void log(const char* pcString, ...);
void log_ln(const char* pcString, ...);
void log_hex(void* data, int len);


int imax(int num1, int num2);
int imin(int num1, int num2);