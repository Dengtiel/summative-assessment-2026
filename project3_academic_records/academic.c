/*
 * academic.c
 * Course performance and academic records manager.
 * Handles full CRUD, search, sorting, analytics, and saves to a binary file.
 *
 * Compile: gcc -Wall -o academic academic.c -lm
 * Run:     ./academic
 */

#include "academic.h"
#include <ctype.h>

// sets up the database with a small initial capacity, grows later if needed
void db_init(Database *db) {
    db->capacity = 8;
    db->count    = 0;
    db->records  = (Student *)malloc(db->capacity * sizeof(Student));
    if (!db->records) { fprintf(stderr, "FATAL: malloc failed\n"); exit(1); }
}

void db_free(Database *db) {
    free(db->records);
    db->records  = NULL;
    db->count    = db->capacity = 0;
}

// doubles the array size when we run out of room
static int db_ensure_capacity(Database *db) {
    if (db->count < db->capacity) return 1;
    int newCap = db->capacity * 2;
    Student *tmp = (Student *)realloc(db->records, newCap * sizeof(Student));
    if (!tmp) { fprintf(stderr, "ERROR: realloc failed\n"); return 0; }
    db->records  = tmp;
    db->capacity = newCap;
    return 1;
}

float compute_gpa(float *grades, int n) {
    if (n <= 0) return 0.0f;
    float sum = 0.0f;
    for (int i = 0; i < n; i++) sum += grades[i];
    return sum / n;
}

// rejects duplicate IDs before inserting
int db_add(Database *db, Student s) {
    /* Duplicate ID check */
    for (int i = 0; i < db->count; i++) {
        if (db->records[i].id == s.id) {
            printf("ERROR: Student ID %d already exists.\n", s.id);
            return 0;
        }
    }
    if (!db_ensure_capacity(db)) return 0;
    s.gpa = compute_gpa(s.grades, s.numGrades);
    db->records[db->count++] = s;
    return 1;
}

// removes the student and shifts everything left to fill the gap
int db_delete(Database *db, int id) {
    for (int i = 0; i < db->count; i++) {
        if (db->records[i].id == id) {
            /* Shift left */
            for (int j = i; j < db->count - 1; j++)
                db->records[j] = db->records[j + 1];
            db->count--;
            return 1;
        }
    }
    return 0;
}

// finds the record by ID and overwrites it, keeping the original ID intact
int db_update(Database *db, int id, Student updated) {
    for (int i = 0; i < db->count; i++) {
        if (db->records[i].id == id) {
            updated.id  = id;  /* preserve ID */
            updated.gpa = compute_gpa(updated.grades, updated.numGrades);
            db->records[i] = updated;
            return 1;
        }
    }
    return 0;
}

// saves everything to a binary file — count first, then the records
int db_save(const Database *db, const char *filename) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) { perror("fopen save"); return 0; }
    fwrite(&db->count, sizeof(int), 1, fp);
    fwrite(db->records, sizeof(Student), db->count, fp);
    fclose(fp);
    return 1;
}

// loads from the binary file, silently returns 0 if the file doesn't exist yet
int db_load(Database *db, const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) return 0;  /* file may not exist yet — that's OK */
    int n;
    if (fread(&n, sizeof(int), 1, fp) != 1) { fclose(fp); return 0; }
    /* expand if needed */
    while (db->capacity < n) db_ensure_capacity(db);
    if (fread(db->records, sizeof(Student), n, fp) != (size_t)n) {
        fclose(fp); return 0;
    }
    db->count = n;
    fclose(fp);
    return 1;
}

Student *search_by_id(Database *db, int id) {
    for (int i = 0; i < db->count; i++)
        if (db->records[i].id == id) return &db->records[i];
    return NULL;
}

// lowercases both the query and the stored name so the match isn't case-sensitive
Student *search_by_name(Database *db, const char *name) {
    char lower_q[MAX_NAME], lower_n[MAX_NAME];
    strncpy(lower_q, name, MAX_NAME - 1);
    for (int c = 0; lower_q[c]; c++) lower_q[c] = tolower(lower_q[c]);
    for (int i = 0; i < db->count; i++) {
        strncpy(lower_n, db->records[i].name, MAX_NAME - 1);
        for (int c = 0; lower_n[c]; c++) lower_n[c] = tolower(lower_n[c]);
        if (strstr(lower_n, lower_q)) return &db->records[i];
    }
    return NULL;
}

static void swap_students(Student *a, Student *b) {
    Student tmp = *a; *a = *b; *b = tmp;
}

// selection sort — highest GPA goes first
void sort_by_gpa(Database *db) {
    for (int i = 0; i < db->count - 1; i++) {
        int max_idx = i;
        for (int j = i + 1; j < db->count; j++)
            if (db->records[j].gpa > db->records[max_idx].gpa) max_idx = j;
        if (max_idx != i) swap_students(&db->records[i], &db->records[max_idx]);
    }
}

void sort_by_name(Database *db) {
    for (int i = 0; i < db->count - 1; i++) {
        int min_idx = i;
        for (int j = i + 1; j < db->count; j++)
            if (strcmp(db->records[j].name, db->records[min_idx].name) < 0)
                min_idx = j;
        if (min_idx != i) swap_students(&db->records[i], &db->records[min_idx]);
    }
}

void sort_by_id(Database *db) {
    for (int i = 0; i < db->count - 1; i++) {
        int min_idx = i;
        for (int j = i + 1; j < db->count; j++)
            if (db->records[j].id < db->records[min_idx].id) min_idx = j;
        if (min_idx != i) swap_students(&db->records[i], &db->records[min_idx]);
    }
}

void display_student(const Student *s) {
    printf("  ID: %-6d  Name: %-20s  Course: %-20s  Age: %d  GPA: %.2f\n",
           s->id, s->name, s->course, s->age, s->gpa);
    printf("  Grades: ");
    for (int i = 0; i < s->numGrades; i++) printf("%.1f ", s->grades[i]);
    printf("\n");
}

void display_all(const Database *db) {
    if (db->count == 0) { printf("  No records found.\n"); return; }
    printf("  %-6s %-20s %-20s %-4s %-6s\n", "ID", "Name", "Course", "Age", "GPA");
    printf("  %s\n", "--------------------------------------------------------------");
    for (int i = 0; i < db->count; i++) {
        printf("  %-6d %-20s %-20s %-4d %.2f\n",
               db->records[i].id, db->records[i].name,
               db->records[i].course, db->records[i].age,
               db->records[i].gpa);
    }
}

void report_class_average(const Database *db) {
    if (db->count == 0) { printf("  No records.\n"); return; }
    float sum = 0.0f;
    for (int i = 0; i < db->count; i++) sum += db->records[i].gpa;
    printf("  Class Average GPA: %.2f\n", sum / db->count);
}

// calculates high, low, mean, and median — copies the GPA array just for sorting
void report_gpa_stats(const Database *db) {
    if (db->count == 0) { printf("  No records.\n"); return; }

    float hi = db->records[0].gpa, lo = db->records[0].gpa, sum = 0.0f;
    for (int i = 0; i < db->count; i++) {
        float g = db->records[i].gpa;
        if (g > hi) hi = g;
        if (g < lo) lo = g;
        sum += g;
    }

    /* Median: copy GPAs, sort, pick middle */
    float *arr = (float *)malloc(db->count * sizeof(float));
    for (int i = 0; i < db->count; i++) arr[i] = db->records[i].gpa;
    /* simple bubble sort for median */
    for (int i = 0; i < db->count - 1; i++)
        for (int j = 0; j < db->count - i - 1; j++)
            if (arr[j] > arr[j+1]) { float t = arr[j]; arr[j] = arr[j+1]; arr[j+1] = t; }
    float median = (db->count % 2 == 0)
        ? (arr[db->count/2 - 1] + arr[db->count/2]) / 2.0f
        : arr[db->count / 2];
    free(arr);

    printf("  Highest GPA: %.2f | Lowest GPA: %.2f | Average: %.2f | Median: %.2f\n",
           hi, lo, sum / db->count, median);
}

// works on a copy of the db so the original order stays untouched
void report_top_n(const Database *db, int n) {
    /* work on a copy to avoid mutating db */
    Database tmp;
    db_init(&tmp);
    for (int i = 0; i < db->count; i++) db_add(&tmp, db->records[i]);
    sort_by_gpa(&tmp);
    int show = n < tmp.count ? n : tmp.count;
    printf("  Top %d students:\n", show);
    for (int i = 0; i < show; i++) {
        printf("  %d. ", i + 1);
        display_student(&tmp.records[i]);
    }
    db_free(&tmp);
}

// groups students by course and picks the one with the highest GPA in each
void report_best_per_course(const Database *db) {
    if (db->count == 0) { printf("  No records.\n"); return; }
    /* Collect unique courses */
    char courses[64][MAX_COURSE];
    int  nc = 0;
    for (int i = 0; i < db->count; i++) {
        int found = 0;
        for (int c = 0; c < nc; c++)
            if (strcmp(courses[c], db->records[i].course) == 0) { found=1; break; }
        if (!found && nc < 64) strncpy(courses[nc++], db->records[i].course, MAX_COURSE-1);
    }
    printf("  Best student per course:\n");
    for (int c = 0; c < nc; c++) {
        Student *best = NULL;
        for (int i = 0; i < db->count; i++)
            if (strcmp(db->records[i].course, courses[c]) == 0)
                if (!best || db->records[i].gpa > best->gpa) best = &db->records[i];
        if (best) {
            printf("  Course: %-20s → %s (GPA %.2f)\n",
                   courses[c], best->name, best->gpa);
        }
    }
}

// flushes leftover characters after a scanf so the next read doesn't break
void clear_input_buffer(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

// keeps asking until the user enters a valid integer in range
int read_int(const char *prompt, int min, int max) {
    int val; char extra;
    while (1) {
        printf("%s", prompt);
        if (scanf("%d%c", &val, &extra) == 2 && extra == '\n' &&
            val >= min && val <= max) return val;
        printf("  Please enter a number between %d and %d.\n", min, max);
        clear_input_buffer();
    }
}

float read_float(const char *prompt, float min, float max) {
    float val; char extra;
    while (1) {
        printf("%s", prompt);
        if (scanf("%f%c", &val, &extra) == 2 && extra == '\n' &&
            val >= min && val <= max) return val;
        printf("  Please enter a number between %.1f and %.1f.\n", min, max);
        clear_input_buffer();
    }
}

void read_string(const char *prompt, char *buf, int maxlen) {
    printf("%s", prompt);
    fgets(buf, maxlen, stdin);
    buf[strcspn(buf, "\n")] = '\0';
}

static void menu_add(Database *db) {
    Student s = {0};
    s.id = read_int("  Student ID: ", 1, 999999);
    clear_input_buffer();
    read_string("  Name: ", s.name, MAX_NAME);
    read_string("  Course: ", s.course, MAX_COURSE);
    s.age = read_int("  Age: ", 1, 120);
    s.numGrades = read_int("  Number of subjects (1-10): ", 1, MAX_GRADES);
    clear_input_buffer();
    for (int i = 0; i < s.numGrades; i++) {
        char prompt[32];
        sprintf(prompt, "  Grade %d (0-100): ", i + 1);
        s.grades[i] = read_float(prompt, 0.0f, 100.0f);
        clear_input_buffer();
    }
    if (db_add(db, s)) {
        printf("  ✓ Record added. GPA: %.2f\n", s.gpa);
        db_save(db, RECORDS_FILE);
    }
}

static void menu_update(Database *db) {
    int id = read_int("  Enter Student ID to update: ", 1, 999999);
    clear_input_buffer();
    Student *found = search_by_id(db, id);
    if (!found) { printf("  Not found.\n"); return; }
    printf("  Current record:\n"); display_student(found);
    Student updated = *found;
    read_string("  New Name (Enter to keep): ", updated.name, MAX_NAME);
    if (strlen(updated.name) == 0) strncpy(updated.name, found->name, MAX_NAME);
    read_string("  New Course (Enter to keep): ", updated.course, MAX_COURSE);
    if (strlen(updated.course) == 0) strncpy(updated.course, found->course, MAX_COURSE);
    updated.age = read_int("  New Age: ", 1, 120);
    updated.numGrades = read_int("  Number of subjects: ", 1, MAX_GRADES);
    clear_input_buffer();
    for (int i = 0; i < updated.numGrades; i++) {
        char prompt[32];
        sprintf(prompt, "  Grade %d: ", i + 1);
        updated.grades[i] = read_float(prompt, 0.0f, 100.0f);
        clear_input_buffer();
    }
    if (db_update(db, id, updated)) {
        printf("  ✓ Record updated.\n");
        db_save(db, RECORDS_FILE);
    }
}

static void menu_delete(Database *db) {
    int id = read_int("  Enter Student ID to delete: ", 1, 999999);
    clear_input_buffer();
    if (db_delete(db, id)) {
        printf("  ✓ Record deleted.\n");
        db_save(db, RECORDS_FILE);
    } else {
        printf("  Student ID %d not found.\n", id);
    }
}

static void menu_search(Database *db) {
    printf("  Search by: 1) ID  2) Name\n");
    int choice = read_int("  Choice: ", 1, 2);
    clear_input_buffer();
    Student *s = NULL;
    if (choice == 1) {
        int id = read_int("  Enter ID: ", 1, 999999);
        clear_input_buffer();
        s = search_by_id(db, id);
    } else {
        char name[MAX_NAME];
        read_string("  Enter name (partial OK): ", name, MAX_NAME);
        s = search_by_name(db, name);
    }
    if (s) { printf("  Found:\n"); display_student(s); }
    else    printf("  Not found.\n");
}

static void menu_sort(Database *db) {
    printf("  Sort by: 1) GPA  2) Name  3) ID\n");
    int choice = read_int("  Choice: ", 1, 3);
    clear_input_buffer();
    if (choice == 1) sort_by_gpa(db);
    else if (choice == 2) sort_by_name(db);
    else sort_by_id(db);
    printf("  ✓ Sorted.\n");
    display_all(db);
}

static void menu_reports(Database *db) {
    printf("\n  REPORTS\n");
    printf("  1) Class statistics\n");
    printf("  2) Top N students\n");
    printf("  3) Best per course\n");
    printf("  4) Course average GPA\n");
    int choice = read_int("  Choice: ", 1, 4);
    clear_input_buffer();
    switch (choice) {
        case 1: report_gpa_stats(db); break;
        case 2: {
            int n = read_int("  How many top students? ", 1, 100);
            clear_input_buffer();
            report_top_n(db, n);
            break;
        }
        case 3: report_best_per_course(db); break;
        case 4: report_class_average(db); break;
    }
}

int main(void) {
    Database db;
    db_init(&db);

    if (db_load(&db, RECORDS_FILE))
        printf("  Loaded %d record(s) from %s\n", db.count, RECORDS_FILE);

    int running = 1;
    while (running) {
        printf("\n  Academic Records Analyzer\n");
        printf("  1) Add student record\n");
        printf("  2) Display all records\n");
        printf("  3) Update record\n");
        printf("  4) Delete record\n");
        printf("  5) Search\n");
        printf("  6) Sort records\n");
        printf("  7) Reports & Analytics\n");
        printf("  8) Save to file\n");
        printf("  9) Exit\n");

        int choice = read_int("  Choose [1-9]: ", 1, 9);
        clear_input_buffer();

        switch (choice) {
            case 1: menu_add(&db);        break;
            case 2: display_all(&db);     break;
            case 3: menu_update(&db);     break;
            case 4: menu_delete(&db);     break;
            case 5: menu_search(&db);     break;
            case 6: menu_sort(&db);       break;
            case 7: menu_reports(&db);    break;
            case 8:
                if (db_save(&db, RECORDS_FILE)) printf("  ✓ Saved.\n");
                else printf("  ERROR: Save failed.\n");
                break;
            case 9: running = 0; break;
        }
    }

    db_free(&db);
    printf("Goodbye!\n");
    return 0;
