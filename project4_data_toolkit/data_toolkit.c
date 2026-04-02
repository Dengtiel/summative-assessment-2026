/*
 * data_toolkit.c
 * A small data analysis toolkit I put together to practice function pointers,
 * callbacks, and dynamic memory management in C.
 *
 * Compile: gcc -Wall -o data_toolkit data_toolkit.c -lm
 * Run:     ./data_toolkit
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

/* Core dataset structure — grows dynamically as we push values */
typedef struct {
    double *data;
    int     size;
    int     capacity;
} Dataset;

/* Function pointer typedefs — keeps the dispatcher code readable */
typedef double  (*TransformFn)(double);
typedef int     (*FilterFn)(double, double);
typedef int     (*CompareFn)(const void *, const void *);
typedef void    (*OperationFn)(Dataset *);

/* Each menu entry pairs a label with the function that handles it */
typedef struct {
    const char  *label;
    OperationFn  fn;
} DispatchEntry;

/* Global state shared across operations — not pretty but keeps signatures simple */
static Dataset *g_dataset   = NULL;
static double   g_threshold = 0.0;
static double   g_scale     = 1.0;

/* Allocate a dataset with a sensible starting capacity */
Dataset *ds_create(int initial_capacity) {
    Dataset *ds = (Dataset *)malloc(sizeof(Dataset));
    if (!ds) return NULL;
    ds->capacity = initial_capacity > 0 ? initial_capacity : 8;
    ds->size     = 0;
    ds->data     = (double *)malloc(ds->capacity * sizeof(double));
    if (!ds->data) { free(ds); return NULL; }
    return ds;
}

void ds_free(Dataset *ds) {
    if (!ds) return;
    free(ds->data);
    free(ds);
}

/* Double the buffer when we run out of room */
int ds_push(Dataset *ds, double val) {
    if (!ds) return 0;
    if (ds->size == ds->capacity) {
        int newCap = ds->capacity * 2;
        double *tmp = (double *)realloc(ds->data, newCap * sizeof(double));
        if (!tmp) { fprintf(stderr, "realloc failed\n"); return 0; }
        ds->data     = tmp;
        ds->capacity = newCap;
    }
    ds->data[ds->size++] = val;
    return 1;
}

void ds_reset(Dataset *ds) {
    if (ds) ds->size = 0;
}

void ds_display(const Dataset *ds) {
    if (!ds || ds->size == 0) { printf("  Dataset is empty.\n"); return; }
    printf("  [");
    for (int i = 0; i < ds->size; i++) {
        printf("%.4g", ds->data[i]);
        if (i < ds->size - 1) printf(", ");
    }
    printf("]  (n=%d)\n", ds->size);
}

/* Stats */

void op_compute_sum_avg(Dataset *ds) {
    if (!ds || ds->size == 0) { printf("  Dataset empty.\n"); return; }
    double sum = 0.0;
    for (int i = 0; i < ds->size; i++) sum += ds->data[i];
    printf("  Sum: %.6g\n  Average: %.6g\n", sum, sum / ds->size);
}

void op_min_max(Dataset *ds) {
    if (!ds || ds->size == 0) { printf("  Dataset empty.\n"); return; }
    double mn = ds->data[0], mx = ds->data[0];
    for (int i = 1; i < ds->size; i++) {
        if (ds->data[i] < mn) mn = ds->data[i];
        if (ds->data[i] > mx) mx = ds->data[i];
    }
    printf("  Min: %.6g\n  Max: %.6g\n", mn, mx);
}

/* Filters — pass the right comparator and we reuse the same loop */
int filter_above(double val, double thresh) { return val > thresh; }
int filter_below(double val, double thresh) { return val < thresh; }

void apply_filter(Dataset *ds, FilterFn fn, double threshold) {
    if (!ds || ds->size == 0) { printf("  Dataset empty.\n"); return; }
    int kept = 0;
    for (int i = 0; i < ds->size; i++)
        if (fn(ds->data[i], threshold)) ds->data[kept++] = ds->data[i];
    printf("  Filter removed %d values (kept %d).\n", ds->size - kept, kept);
    ds->size = kept;
}

void op_filter_above(Dataset *ds) { apply_filter(ds, filter_above, g_threshold); }
void op_filter_below(Dataset *ds) { apply_filter(ds, filter_below, g_threshold); }

/* Transformations — each one is a plain function we hand off as a callback */
double transform_scale(double x)  { return x * g_scale; }
double transform_sqrt (double x)  { return x >= 0 ? sqrt(x) : x; }
double transform_square(double x) { return x * x; }
double transform_log  (double x)  { return x > 0 ? log(x) : x; }

void apply_transform(Dataset *ds, TransformFn fn) {
    if (!ds || ds->size == 0) { printf("  Dataset empty.\n"); return; }
    for (int i = 0; i < ds->size; i++) ds->data[i] = fn(ds->data[i]);
    printf("  Transformation applied.\n");
    ds_display(ds);
}

void op_transform_scale (Dataset *ds) { apply_transform(ds, transform_scale);  }
void op_transform_sqrt  (Dataset *ds) { apply_transform(ds, transform_sqrt);   }
void op_transform_square(Dataset *ds) { apply_transform(ds, transform_square); }
void op_transform_log   (Dataset *ds) { apply_transform(ds, transform_log);    }

/* qsort comparators */
int cmp_asc (const void *a, const void *b) {
    double da = *(double *)a, db = *(double *)b;
    return (da > db) - (da < db);
}
int cmp_desc(const void *a, const void *b) { return cmp_asc(b, a); }

void op_sort_asc (Dataset *ds) {
    if (!ds || ds->size == 0) { printf("  Dataset empty.\n"); return; }
    qsort(ds->data, ds->size, sizeof(double), cmp_asc);
    printf("  Sorted ascending.\n");
    ds_display(ds);
}
void op_sort_desc(Dataset *ds) {
    if (!ds || ds->size == 0) { printf("  Dataset empty.\n"); return; }
    qsort(ds->data, ds->size, sizeof(double), cmp_desc);
    printf("  Sorted descending.\n");
    ds_display(ds);
}

/* Linear scan — good enough for the sizes we're dealing with */
void op_search(Dataset *ds) {
    if (!ds || ds->size == 0) { printf("  Dataset empty.\n"); return; }
    printf("  Enter value to search: ");
    double target; scanf("%lf", &target); getchar();
    int found = 0;
    for (int i = 0; i < ds->size; i++)
        if (fabs(ds->data[i] - target) < 1e-9) {
            printf("  Found at index %d\n", i); found = 1;
        }
    if (!found) printf("  Value %.6g not found.\n", target);
}

/* File I/O — simple line-by-line format, lines starting with # are skipped */
void op_save_file(Dataset *ds) {
    if (!ds || ds->size == 0) { printf("  Nothing to save.\n"); return; }
    char filename[128];
    printf("  Output filename: "); fgets(filename, 128, stdin);
    filename[strcspn(filename, "\n")] = '\0';
    FILE *fp = fopen(filename, "w");
    if (!fp) { perror("fopen"); return; }
    fprintf(fp, "# Dataset exported — n=%d\n", ds->size);
    for (int i = 0; i < ds->size; i++) fprintf(fp, "%.10g\n", ds->data[i]);
    fclose(fp);
    printf("  Saved %d values to '%s'\n", ds->size, filename);
}

void op_load_file(Dataset *ds) {
    char filename[128];
    printf("  Input filename: "); fgets(filename, 128, stdin);
    filename[strcspn(filename, "\n")] = '\0';
    FILE *fp = fopen(filename, "r");
    if (!fp) { perror("fopen"); return; }
    ds_reset(ds);
    char line[64]; int n = 0;
    while (fgets(line, 64, fp)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        double v; if (sscanf(line, "%lf", &v) == 1) { ds_push(ds, v); n++; }
    }
    fclose(fp);
    printf("  Loaded %d values.\n", n);
    ds_display(ds);
}

void op_create_dataset(Dataset *ds) {
    ds_reset(ds);
    int n;
    printf("  How many values to enter? "); scanf("%d", &n); getchar();
    if (n <= 0) { printf("  Invalid count.\n"); return; }
    printf("  Enter %d space-separated values:\n  ", n);
    for (int i = 0; i < n; i++) {
        double v; scanf("%lf", &v); ds_push(ds, v);
    }
    getchar();
    printf("  Dataset created.\n");
    ds_display(ds);
}

void op_display_dataset(Dataset *ds) { ds_display(ds); }

void op_reset_dataset(Dataset *ds) { ds_reset(ds); printf("  Dataset cleared.\n"); }

/* Prompt wrappers — collect user input into globals before calling the real op */
void op_filter_above_prompt(Dataset *ds) {
    printf("  Filter above threshold: "); scanf("%lf", &g_threshold); getchar();
    op_filter_above(ds);
    ds_display(ds);
}
void op_filter_below_prompt(Dataset *ds) {
    printf("  Filter below threshold: "); scanf("%lf", &g_threshold); getchar();
    op_filter_below(ds);
    ds_display(ds);
}
void op_scale_prompt(Dataset *ds) {
    printf("  Scale factor: "); scanf("%lf", &g_scale); getchar();
    op_transform_scale(ds);
}

/* Dispatch table — add a new row here and it shows up in the menu automatically */
static DispatchEntry dispatch_table[] = {
    /* 0 */  { "Create / load dataset from keyboard",  op_create_dataset      },
    /* 1 */  { "Load dataset from file",               op_load_file           },
    /* 2 */  { "Display dataset",                      op_display_dataset     },
    /* 3 */  { "Compute sum & average",                op_compute_sum_avg     },
    /* 4 */  { "Find minimum & maximum",               op_min_max             },
    /* 5 */  { "Filter: keep values ABOVE threshold",  op_filter_above_prompt },
    /* 6 */  { "Filter: keep values BELOW threshold",  op_filter_below_prompt },
    /* 7 */  { "Transform: scale by factor",           op_scale_prompt        },
    /* 8 */  { "Transform: square root",               op_transform_sqrt      },
    /* 9 */  { "Transform: square",                    op_transform_square    },
    /* 10 */ { "Transform: natural log",               op_transform_log       },
    /* 11 */ { "Sort ascending",                       op_sort_asc            },
    /* 12 */ { "Sort descending",                      op_sort_desc           },
    /* 13 */ { "Search for value",                     op_search              },
    /* 14 */ { "Save dataset to file",                 op_save_file           },
    /* 15 */ { "Reset / clear dataset",                op_reset_dataset       },
};

#define NUM_OPS ((int)(sizeof(dispatch_table) / sizeof(dispatch_table[0])))

static int dispatch(int choice, Dataset *ds) {
    if (choice < 0 || choice >= NUM_OPS) {
        printf("  ERROR: Invalid operation index %d\n", choice);
        return 0;
    }
    if (!dispatch_table[choice].fn) {
        printf("  ERROR: NULL function pointer at index %d\n", choice);
        return 0;
    }
    dispatch_table[choice].fn(ds);
    return 1;
}

int main(void) {
    Dataset *ds = ds_create(16);
    if (!ds) { fprintf(stderr, "FATAL: Failed to allocate dataset\n"); return 1; }
    g_dataset = ds;

    printf("Data Analysis Toolkit\n");

    int running = 1;
    while (running) {
        printf("\nOperations:\n");
        for (int i = 0; i < NUM_OPS; i++)
            printf("  %2d) %s\n", i, dispatch_table[i].label);
        printf("  99) Exit\n");

        int choice;
        printf("  Choose operation: ");
        if (scanf("%d", &choice) != 1) { getchar(); printf("  Invalid input.\n"); continue; }
        getchar();

        if (choice == 99) { running = 0; break; }

        if (choice < 0 || choice >= NUM_OPS) {
            printf("  Invalid choice. Please enter 0-%d or 99.\n", NUM_OPS - 1);
            continue;
        }

        dispatch(choice, ds);
    }

    ds_free(ds);
    g_dataset = NULL;
    printf("Memory freed. Goodbye!\n");
    return 0;
}
