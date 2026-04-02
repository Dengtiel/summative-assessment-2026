# Systems Programming — Summative Assessment

**Student:** Deng Mayen Deng Akol
**Course:** Programming in C 

---

## What This Is

This repo has all five projects for the summative assessment. Each project is in its own folder. Below is a breakdown of what each one does, how to run it, and what you need installed first.

---

## Project Structure

```
assignment/
├── README.md
├── project1_traffic/
│   └── traffic_controller.ino
├── project2_server_monitor/
│   └── server_monitor.sh
├── project3_academic_records/
│   ├── academic.h
│   └── academic.c
├── project4_data_toolkit/
│   └── data_toolkit.c
└── project5_web_scraper/
    ├── web_scraper.c
    └── urls.txt
```

---

## Project 1 — Smart Traffic Light Controller

This one simulates a smart traffic light system for two intersections using an Arduino Uno. The simulation runs in Tinkercad and the PCB design was done in EasyEDA.

The two intersections run opposite phases — when one is green the other is red. Push buttons act as vehicle detectors, and the green light duration adjusts automatically depending on how many vehicles are detected. All timing is done with `millis()` so nothing blocks the loop.

**How the circuit is wired:**

| Component | Pin |
|---|---|
| Intersection 1 — Red | D13 |
| Intersection 1 — Yellow | D12 |
| Intersection 1 — Green | D11 |
| Intersection 1 — Button | D7 |
| Intersection 2 — Red | D8 |
| Intersection 2 — Yellow | D9 |
| Intersection 2 — Green | D10 |
| Intersection 2 — Button | D6 |

Each LED has a 220Ω resistor. Buttons use `INPUT_PULLUP` so no external resistors needed.

**Green timing logic:**
- Base: 10 seconds
- Medium traffic (4+ vehicles): 12 seconds
- Heavy traffic (9+ vehicles): 15 seconds

**Serial monitor commands:**

| Command | What it does |
|---|---|
| `1` | Toggle manual mode — Intersection 1 |
| `2` | Toggle manual mode — Intersection 2 |
| `R` | Set red (manual intersections only) |
| `G` | Set green (manual intersections only) |
| `Y` | Set yellow (manual intersections only) |
| `E` | Emergency stop — all lights go red |
| `S` | Reset system back to automatic |
| `V` | Show detailed statistics |
| `M` | Show the command menu |

**To run it:**
1. Go to [tinkercad.com](https://www.tinkercad.com) and create a new circuit
2. Add an Arduino Uno, 6 LEDs, 6x 220Ω resistors, and 2 push buttons
3. Wire everything according to the table above
4. Open the code editor, switch to Text mode, paste `traffic_controller.ino`
5. Hit Start Simulation and open the Serial Monitor at 9600 baud

---

## Project 2 — Linux Server Health Monitor

A bash script that watches your server's CPU, memory, disk, and processes. It runs in a loop, alerts you when something goes over the limit, and logs everything with timestamps.

**What it monitors:**
- CPU usage (via `top`)
- Memory used vs total (via `free -m`)
- Disk space per partition (via `df -h`)
- Active process count and top 5 by CPU (via `ps`)

**Default alert thresholds:**
- CPU: 80%
- Memory: 80%
- Disk: 85%
- Check interval: 60 seconds

You can change all of these from inside the menu without editing the script.

**Menu options:**
1. Display current system health
2. Configure alert thresholds
3. View activity log
4. Clear log
5. Start background monitoring
6. Stop background monitoring
7. Exit

**To run it:**
```bash
cd project2_server_monitor
chmod +x server_monitor.sh
bash server_monitor.sh
```

Logs are saved to `~/server_monitor.log` with entries like:
```
[2025-06-01 14:23:45] [ALERT] CPU usage 87.3% exceeded threshold 80%
```

---

## Project 3 — Academic Records Analyzer

A C program that manages student academic records. You can add, view, update, and delete students, sort and search through them, and generate reports like class averages, top performers, and best student per course.

Records are saved to a binary file and reload automatically the next time you run the program.

**What gets stored per student:**
- Student ID (must be unique)
- Full name
- Course name
- Age
- Up to 10 subject grades
- GPA (calculated automatically)

**Compile and run:**
```bash
cd project3_academic_records
gcc -Wall -o academic academic.c -lm
./academic
```

**Menu options:**
1. Add student record
2. Display all records
3. Update a record
4. Delete a record
5. Search (by ID or name)
6. Sort (by GPA, name, or ID)
7. Reports and analytics
8. Save to file
9. Exit

**Sorting algorithms used:** Selection sort for all three sort types — by GPA descending, by name alphabetically, and by ID numerically.

**Search:** Exact match by ID, or case-insensitive partial match by name using `strstr()`.

**Reports available:**
- Highest, lowest, average, and median GPA
- Top N students
- Best student per course
- Course average GPA

---

## Project 4 — Data Analysis Toolkit

A C program built around function pointers. The main idea is that every operation in the menu maps to a function pointer in a dispatch table — there is no big if-else or switch statement running the menu. Filters and transformations are passed as callbacks to generic processing functions.

**Compile and run:**
```bash
cd project4_data_toolkit
gcc -Wall -o data_toolkit data_toolkit.c -lm
./data_toolkit
```

**Operations available:**

| # | Operation |
|---|---|
| 0 | Create dataset from keyboard |
| 1 | Load dataset from file |
| 2 | Display dataset |
| 3 | Sum and average |
| 4 | Min and max |
| 5 | Filter — keep values above threshold |
| 6 | Filter — keep values below threshold |
| 7 | Transform — scale by factor |
| 8 | Transform — square root |
| 9 | Transform — square |
| 10 | Transform — natural log |
| 11 | Sort ascending |
| 12 | Sort descending |
| 13 | Search for a value |
| 14 | Save dataset to file |
| 15 | Reset dataset |
| 99 | Exit |

Sorting uses `qsort()` with `cmp_asc` or `cmp_desc` passed as a comparator callback. The dataset expands automatically with `realloc()` as you add values. All memory is freed cleanly before exit.

---

## Project 5 — Multi-threaded Web Scraper

A C program that downloads multiple web pages at the same time using POSIX threads. One thread per URL — they all run in parallel and each one saves its downloaded HTML to its own file.

Because every thread works on its own data (its own curl handle, its own buffer, its own output file), there is no shared state between threads and no locking is needed.

**What you need installed:**
```bash
sudo apt install libcurl4-openssl-dev
```

**Compile and run:**
```bash
cd project5_web_scraper
gcc -Wall -o web_scraper web_scraper.c -lpthread -lcurl

# Run with default URLs
./web_scraper

# Run with your own URL list
./web_scraper urls.txt
```

**urls.txt format:**
```
# Lines starting with # are ignored
https://example.com
https://httpbin.org/html
https://www.wikipedia.org
```

Output files go into `scraped_output/` — one file per thread named `page_000.html`, `page_001.html`, and so on.

After all threads finish, a summary table prints showing each thread's URL, status (OK or FAIL), bytes downloaded, and how long it took. Failed URLs show the exact error message from curl.

---

## Dependencies Summary

| Project | What you need |
|---|---|
| Project 1 | Tinkercad account (free), EasyEDA account (free) |
| Project 2 | Linux or WSL, bash, standard GNU tools |
| Project 3 | GCC, make |
| Project 4 | GCC, make |
| Project 5 | GCC, libcurl (`sudo apt install libcurl4-openssl-dev`) |

---

## Running on Windows

All C and bash projects require a Linux environment. On Windows, use WSL:

```powershell
# Run in PowerShell as Administrator
wsl --install
```

Then open VS Code and connect to WSL using the remote extension.

---

## Notes

- Records for Project 3 are saved as `records.bin` in the same folder as the binary
- Project 5 creates the `scraped_output/` folder automatically if it does not exist
- For Project 2, set the CPU threshold to something low like 5% during your demo so alerts fire immediately
- The GitHub repo must stay public until the RO publishes grades
