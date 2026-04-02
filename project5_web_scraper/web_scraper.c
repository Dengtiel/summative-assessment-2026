/*
 * web_scraper.c
 * Fetches multiple URLs concurrently using pthreads — one thread per URL.
 * Each thread saves the HTML response to its own file in scraped_output/.
 * Needs libcurl for HTTP and pthreads for parallelism.
 *
 * Compile: gcc -Wall -o web_scraper web_scraper.c -lpthread -lcurl
 * Run:     ./web_scraper urls.txt
 *          (falls back to a few hardcoded test URLs if no file is given)
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <curl/curl.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif
#include <errno.h>
#include <time.h>

#define MAX_URLS        64
#define MAX_URL_LEN    512
#define OUTPUT_DIR     "scraped_output"
#define CONNECT_TIMEOUT 15L
#define TOTAL_TIMEOUT   30L

/* Everything a thread needs to know about its assigned URL */
typedef struct {
    int    thread_id;
    char   url[MAX_URL_LEN];
    char   output_file[256];
    int    success;
    long   http_code;
    size_t bytes_received;
    double elapsed_sec;
    char   error_msg[256];
} ThreadArgs;

/* curl writes response chunks here; we grow it with realloc as needed */
typedef struct {
    char  *data;
    size_t size;
} MemoryBuffer;

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    MemoryBuffer *buf = (MemoryBuffer *)userp;

    char *tmp = realloc(buf->data, buf->size + realsize + 1);
    if (!tmp) return 0;  /* returning 0 tells curl something went wrong */

    buf->data = tmp;
    memcpy(buf->data + buf->size, contents, realsize);
    buf->size += realsize;
    buf->data[buf->size] = '\0';
    return realsize;
}

/* Thread ID gives us unique filenames without having to sanitize the URL */
static void url_to_filename(const char *url, int id, char *out, int outlen) {
    (void)url;
    snprintf(out, outlen, "%s/page_%03d.html", OUTPUT_DIR, id);
}

static int ensure_output_dir(void) {
#ifdef _WIN32
    if (_mkdir(OUTPUT_DIR) != 0 && errno != EEXIST) return 0;
#else
    if (mkdir(OUTPUT_DIR, 0755) != 0 && errno != EEXIST) return 0;
#endif
    return 1;
}

/* Each thread runs this — fetch the URL, write HTML to disk */
static void *scrape_url(void *arg) {
    ThreadArgs *ta = (ThreadArgs *)arg;
    ta->success = 0;
    ta->bytes_received = 0;

    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    CURL *curl = curl_easy_init();
    if (!curl) {
        snprintf(ta->error_msg, 256, "curl_easy_init() failed");
        pthread_exit(NULL);
    }

    MemoryBuffer buf = { .data = malloc(1), .size = 0 };
    if (!buf.data) {
        snprintf(ta->error_msg, 256, "malloc failed for buffer");
        curl_easy_cleanup(curl);
        pthread_exit(NULL);
    }
    buf.data[0] = '\0';

    curl_easy_setopt(curl, CURLOPT_URL,            ta->url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &buf);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, CONNECT_TIMEOUT);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        TOTAL_TIMEOUT);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,      "Mozilla/5.0 (scraper/1.0)");
    /* skipping SSL verification — fine for local testing, don't ship this */
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &ta->http_code);
    curl_easy_cleanup(curl);

    clock_gettime(CLOCK_MONOTONIC, &t_end);
    ta->elapsed_sec = (t_end.tv_sec  - t_start.tv_sec) +
                      (t_end.tv_nsec - t_start.tv_nsec) / 1e9;

    if (res != CURLE_OK) {
        snprintf(ta->error_msg, 256, "curl error: %s", curl_easy_strerror(res));
        free(buf.data);
        pthread_exit(NULL);
    }

    if (ta->http_code != 200) {
        snprintf(ta->error_msg, 256, "HTTP %ld", ta->http_code);
        free(buf.data);
        pthread_exit(NULL);
    }

    FILE *fp = fopen(ta->output_file, "w");
    if (!fp) {
        snprintf(ta->error_msg, 256, "fopen failed: %s", strerror(errno));
        free(buf.data);
        pthread_exit(NULL);
    }
    fprintf(fp, "<!-- Source: %s -->\n", ta->url);
    fwrite(buf.data, 1, buf.size, fp);
    fclose(fp);

    ta->bytes_received = buf.size;
    ta->success = 1;
    snprintf(ta->error_msg, 256, "OK");
    free(buf.data);
    return NULL;
}

/* Read URLs from a file, one per line; lines starting with # are ignored */
static int load_urls(const char *filename, char urls[][MAX_URL_LEN], int maxurls) {
    FILE *fp = fopen(filename, "r");
    if (!fp) { perror("fopen urls file"); return 0; }
    int n = 0;
    while (n < maxurls && fgets(urls[n], MAX_URL_LEN, fp)) {
        urls[n][strcspn(urls[n], "\n\r")] = '\0';
        if (strlen(urls[n]) > 0 && urls[n][0] != '#') n++;
    }
    fclose(fp);
    return n;
}

/* Print a results table after all threads finish */
static void print_summary(ThreadArgs *args, int n) {
    printf("\nScraping Summary\n");
    printf("%-4s %-35s %-8s %-8s %-10s\n",
           "ID", "URL (truncated)", "Status", "Bytes", "Time(s)");
    printf("-----------------------------------------------------------------------\n");
    int ok = 0;
    for (int i = 0; i < n; i++) {
        char trunc_url[36];
        snprintf(trunc_url, sizeof(trunc_url), "%.35s", args[i].url);
        printf("%-4d %-35s %-8s %-8zu %-10.2f\n",
               args[i].thread_id,
               trunc_url,
               args[i].success ? "OK" : "FAIL",
               args[i].bytes_received,
               args[i].elapsed_sec);
        if (!args[i].success)
            printf("     Error: %s\n", args[i].error_msg);
        if (args[i].success) ok++;
    }
    printf("-----------------------------------------------------------------------\n");
    printf("%d/%d URLs scraped successfully. Output in ./%s/\n", ok, n, OUTPUT_DIR);
}

int main(int argc, char *argv[]) {
    printf("Multi-threaded Web Scraper\n");

    static char urls[MAX_URLS][MAX_URL_LEN];
    int num_urls = 0;

    if (argc >= 2) {
        num_urls = load_urls(argv[1], urls, MAX_URLS);
        if (num_urls == 0) {
            fprintf(stderr, "ERROR: No URLs loaded from '%s'\n", argv[1]);
            return 1;
        }
    } else {
        /* handful of reliable test endpoints */
        const char *defaults[] = {
            "https://example.com",
            "https://httpbin.org/html",
            "https://www.wikipedia.org",
            "https://httpbin.org/get",
        };
        num_urls = (int)(sizeof(defaults)/sizeof(defaults[0]));
        for (int i = 0; i < num_urls; i++)
            strncpy(urls[i], defaults[i], MAX_URL_LEN - 1);
    }

    printf("  URLs to scrape: %d\n", num_urls);

    if (!ensure_output_dir()) {
        perror("mkdir");
        return 1;
    }

    /* curl_global_init must be called once before any threads touch curl */
    curl_global_init(CURL_GLOBAL_ALL);

    pthread_t  *threads = (pthread_t  *)malloc(num_urls * sizeof(pthread_t));
    ThreadArgs *args    = (ThreadArgs *)malloc(num_urls * sizeof(ThreadArgs));
    if (!threads || !args) {
        fprintf(stderr, "malloc failed for thread arrays\n");
        return 1;
    }

    for (int i = 0; i < num_urls; i++) {
        args[i].thread_id = i;
        strncpy(args[i].url, urls[i], MAX_URL_LEN - 1);
        url_to_filename(urls[i], i, args[i].output_file, sizeof(args[i].output_file));
        args[i].success = 0;
        args[i].error_msg[0] = '\0';

        int rc = pthread_create(&threads[i], NULL, scrape_url, &args[i]);
        if (rc != 0) {
            fprintf(stderr, "pthread_create failed for URL %d: %s\n", i, strerror(rc));
            args[i].success = 0;
            snprintf(args[i].error_msg, 256, "pthread_create failed");
            threads[i] = 0;
        } else {
            printf("  [Thread %d] Started -> %s\n", i, urls[i]);
        }
    }

    /* wait for everyone to finish before printing results */
    for (int i = 0; i < num_urls; i++) {
        if (threads[i]) pthread_join(threads[i], NULL);
    }

    curl_global_cleanup();
    print_summary(args, num_urls);

    free(threads);
    free(args);
    return 0;
}
