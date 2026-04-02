/**
 * academic.h — Structures and prototypes for Academic Records Analyzer
 */
#ifndef ACADEMIC_H
#define ACADEMIC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_NAME    64
#define MAX_COURSE  64
#define MAX_GRADES  10
#define RECORDS_FILE "records.bin"

/* ─── Student Record Structure ─────────────────────────────────────────── */
typedef struct {
    int    id;
    char   name[MAX_NAME];
    char   course[MAX_COURSE];
    int    age;
    float  grades[MAX_GRADES];   /* individual subject grades */
    int    numGrades;
    float  gpa;                  /* computed average */
} Student;

/* ─── Database ──────────────────────────────────────────────────────────── */
typedef struct {
    Student *records;   /* dynamically allocated array */
    int      count;
    int      capacity;
} Database;

/* ─── Function Prototypes ───────────────────────────────────────────────── */
/* db management */
void db_init(Database *db);
void db_free(Database *db);
int  db_add(Database *db, Student s);
int  db_delete(Database *db, int id);
int  db_update(Database *db, int id, Student updated);

/* GPA */
float compute_gpa(float *grades, int n);

/* file I/O */
int  db_save(const Database *db, const char *filename);
int  db_load(Database *db, const char *filename);

/* search */
Student *search_by_id(Database *db, int id);
Student *search_by_name(Database *db, const char *name);

/* sort (manual algorithms) */
void sort_by_gpa(Database *db);
void sort_by_name(Database *db);
void sort_by_id(Database *db);

/* display */
void display_all(const Database *db);
void display_student(const Student *s);

/* analytics */
void report_class_average(const Database *db);
void report_top_n(const Database *db, int n);
void report_best_per_course(const Database *db);
void report_gpa_stats(const Database *db);

/* UI helpers */
void clear_input_buffer(void);
int  read_int(const char *prompt, int min, int max);
float read_float(const char *prompt, float min, float max);
void read_string(const char *prompt, char *buf, int maxlen);

#endif /* ACADEMIC_H */