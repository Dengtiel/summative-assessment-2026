#!/bin/bash
# server_monitor.sh
# A quick script I put together to keep an eye on server health.
# Watches CPU, memory, disk, and running processes, logs alerts
# when things go over the limits, and has a simple menu to use it.
#
# Usage: bash server_monitor.sh

# defaults — change these if your server needs different limits
CPU_THRESHOLD=80
MEM_THRESHOLD=80
DISK_THRESHOLD=85
MONITOR_INTERVAL=60          # how often the background check runs (in seconds)
LOG_FILE="$HOME/server_monitor.log"
MONITORING_PID_FILE="/tmp/server_monitor_bg.pid"
MONITOR_RUNNING=false

# terminal colors, nothing fancy
RED='\033[0;31m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
BOLD='\033[1m'
RESET='\033[0m'

# just returns a formatted timestamp, used in logs and the dashboard header
timestamp() {
    date '+%Y-%m-%d %H:%M:%S'
}

# writes a line to the log file with a timestamp and severity level
log_message() {
    local level="$1"
    local msg="$2"
    echo "[$(timestamp)] [$level] $msg" >> "$LOG_FILE"
}

# make sure the basic tools we need are actually installed before doing anything
check_dependencies() {
    local missing=0
    for cmd in top free df ps awk grep sed; do
        if ! command -v "$cmd" &>/dev/null; then
            echo -e "${RED}ERROR: Required command '$cmd' not found.${RESET}"
            missing=1
        fi
    done
    [ $missing -eq 1 ] && exit 1
}

# grabs current CPU usage — tries top first, falls back to /proc/stat if that fails
get_cpu_usage() {
    # Use top in batch mode, extract CPU idle, calculate usage
    local idle
    idle=$(top -bn1 | grep "Cpu(s)" | awk '{print $8}' | sed 's/%id,//' | tr -d ' ')
    # Fallback: /proc/stat based calculation
    if [ -z "$idle" ]; then
        local cpu_line
        cpu_line=$(grep '^cpu ' /proc/stat)
        local user system idle_val total
        user=$(echo "$cpu_line" | awk '{print $2}')
        system=$(echo "$cpu_line" | awk '{print $4}')
        idle_val=$(echo "$cpu_line" | awk '{print $5}')
        total=$((user + system + idle_val))
        idle=$(awk "BEGIN {printf \"%.1f\", ($idle_val/$total)*100}")
    fi
    awk "BEGIN {printf \"%.1f\", 100 - $idle}"
}

# returns used MB, total MB, and usage percentage — space-separated for easy reading
get_memory_usage() {
    local total used
    total=$(free -m | awk '/^Mem:/{print $2}')
    used=$(free -m  | awk '/^Mem:/{print $3}')
    echo "$used $total $(awk "BEGIN {printf \"%.1f\", ($used/$total)*100}")"
}

# lists all mounted filesystems with how full they are
get_disk_usage() {
    df -h --output=target,pcent,used,size | tail -n +2
}

# top 5 processes eating the most CPU right now
get_top_processes() {
    ps aux --sort=-%cpu | head -6 | awk 'NR==1{print "USER\tPID\t%CPU\t%MEM\tCOMMAND"} NR>1{printf "%s\t%s\t%s\t%s\t%s\n",$1,$2,$3,$4,$11}'
}

get_process_count() {
    ps aux | wc -l
}

# the main dashboard — clears the screen and prints everything in one shot
display_health() {
    clear
    echo -e "${BOLD}${CYAN}  SERVER HEALTH MONITOR DASHBOARD${RESET}"
    echo -e "${CYAN}  $(timestamp)${RESET}"

    # CPU
    local cpu
    cpu=$(get_cpu_usage)
    echo -e "\n${BOLD}  CPU${RESET}"
    if awk "BEGIN {exit !($cpu >= $CPU_THRESHOLD)}"; then
        echo -e "  Usage: ${RED}${cpu}%  ⚠ ALERT (threshold: ${CPU_THRESHOLD}%)${RESET}"
        log_message "ALERT" "CPU usage ${cpu}% exceeded threshold ${CPU_THRESHOLD}%"
    else
        echo -e "  Usage: ${GREEN}${cpu}%${RESET}  (threshold: ${CPU_THRESHOLD}%)"
    fi

    # Memory
    echo -e "\n${BOLD}  MEMORY${RESET}"
    read -r mem_used mem_total mem_pct <<< "$(get_memory_usage)"
    if awk "BEGIN {exit !($mem_pct >= $MEM_THRESHOLD)}"; then
        echo -e "  Used: ${RED}${mem_used}MB / ${mem_total}MB (${mem_pct}%)  ⚠ ALERT${RESET}"
        log_message "ALERT" "Memory usage ${mem_pct}% exceeded threshold ${MEM_THRESHOLD}%"
    else
        echo -e "  Used: ${GREEN}${mem_used}MB / ${mem_total}MB (${mem_pct}%)${RESET}"
    fi

    # Disk
    echo -e "\n${BOLD}  DISK${RESET}"
    while IFS= read -r line; do
        local pct
        pct=$(echo "$line" | awk '{print $2}' | tr -d '%')
        if [ -n "$pct" ] && awk "BEGIN {exit !($pct >= $DISK_THRESHOLD)}"; then
            echo -e "  ${RED}$line  ⚠ ALERT${RESET}"
            log_message "ALERT" "Disk usage ${pct}% on $(echo "$line"|awk '{print $1}') exceeded threshold"
        else
            echo -e "  ${GREEN}$line${RESET}"
        fi
    done <<< "$(get_disk_usage)"

    # Processes
    echo -e "\n${BOLD}  PROCESSES${RESET}"
    echo -e "  Active process count: ${CYAN}$(get_process_count)${RESET}"
    echo -e "\n  Top 5 by CPU:"
    get_top_processes | while IFS= read -r line; do
        echo "  $line"
    done

    echo ""
    log_message "INFO" "Health check completed. CPU:${cpu}% MEM:${mem_pct}% "
}

# lets you update the alert thresholds and check interval interactively
# validates input so you can't accidentally set something insane
configure_thresholds() {
    echo -e "\n${BOLD}Configure Alert Thresholds${RESET}"
    echo -e "Current: CPU=${CPU_THRESHOLD}% | MEM=${MEM_THRESHOLD}% | DISK=${DISK_THRESHOLD}%\n"

    read -rp "New CPU  threshold (1-100) [Enter to keep ${CPU_THRESHOLD}]: " val
    if [ -n "$val" ]; then
        if [[ "$val" =~ ^[0-9]+$ ]] && [ "$val" -ge 1 ] && [ "$val" -le 100 ]; then
            CPU_THRESHOLD=$val
            echo -e "${GREEN}CPU threshold set to ${CPU_THRESHOLD}%${RESET}"
        else
            echo -e "${RED}Invalid value. Keeping ${CPU_THRESHOLD}%${RESET}"
        fi
    fi

    read -rp "New MEM  threshold (1-100) [Enter to keep ${MEM_THRESHOLD}]: " val
    if [ -n "$val" ]; then
        if [[ "$val" =~ ^[0-9]+$ ]] && [ "$val" -ge 1 ] && [ "$val" -le 100 ]; then
            MEM_THRESHOLD=$val
            echo -e "${GREEN}Memory threshold set to ${MEM_THRESHOLD}%${RESET}"
        else
            echo -e "${RED}Invalid value. Keeping ${MEM_THRESHOLD}%${RESET}"
        fi
    fi

    read -rp "New DISK threshold (1-100) [Enter to keep ${DISK_THRESHOLD}]: " val
    if [ -n "$val" ]; then
        if [[ "$val" =~ ^[0-9]+$ ]] && [ "$val" -ge 1 ] && [ "$val" -le 100 ]; then
            DISK_THRESHOLD=$val
            echo -e "${GREEN}Disk threshold set to ${DISK_THRESHOLD}%${RESET}"
        else
            echo -e "${RED}Invalid value. Keeping ${DISK_THRESHOLD}%${RESET}"
        fi
    fi

    read -rp "New monitoring interval in seconds [Enter to keep ${MONITOR_INTERVAL}]: " val
    if [ -n "$val" ]; then
        if [[ "$val" =~ ^[0-9]+$ ]] && [ "$val" -ge 5 ]; then
            MONITOR_INTERVAL=$val
            echo -e "${GREEN}Interval set to ${MONITOR_INTERVAL}s${RESET}"
        else
            echo -e "${RED}Invalid value (min 5s). Keeping ${MONITOR_INTERVAL}s${RESET}"
        fi
    fi
    log_message "CONFIG" "Thresholds updated: CPU=${CPU_THRESHOLD}% MEM=${MEM_THRESHOLD}% DISK=${DISK_THRESHOLD}% INTERVAL=${MONITOR_INTERVAL}s"
}

# prints the last 30 lines of the log so you don't have to open it manually
view_logs() {
    if [ ! -f "$LOG_FILE" ]; then
        echo -e "${YELLOW}No log file found at $LOG_FILE${RESET}"
        return
    fi
    echo -e "\n${BOLD}Log File: $LOG_FILE${RESET}"
    echo -e "${BOLD}Lines: $(wc -l < "$LOG_FILE")${RESET}\n"
    echo "--- Last 30 entries ---"
    tail -30 "$LOG_FILE"
}

# wipes the log — asks for confirmation first so you don't nuke it by accident
clear_logs() {
    read -rp "Are you sure you want to clear the log? (yes/no): " confirm
    if [ "$confirm" = "yes" ]; then
        > "$LOG_FILE"
        echo -e "${GREEN}Log cleared.${RESET}"
        log_message "INFO" "Log file cleared by user"
    else
        echo "Cancelled."
    fi
}

# this runs in the background and silently logs alerts without touching the screen
background_monitor() {
    log_message "INFO" "Background monitoring started (interval: ${MONITOR_INTERVAL}s)"
    while true; do
        # Run health check silently, logging alerts only
        local cpu
        cpu=$(get_cpu_usage)
        read -r mem_used mem_total mem_pct <<< "$(get_memory_usage)"

        awk "BEGIN {exit !($cpu >= $CPU_THRESHOLD)}" && \
            log_message "ALERT" "BG: CPU ${cpu}% >= ${CPU_THRESHOLD}%"

        awk "BEGIN {exit !($mem_pct >= $MEM_THRESHOLD)}" && \
            log_message "ALERT" "BG: Memory ${mem_pct}% >= ${MEM_THRESHOLD}%"

        while IFS= read -r line; do
            local pct
            pct=$(echo "$line" | awk '{print $2}' | tr -d '%')
            [ -n "$pct" ] && awk "BEGIN {exit !($pct >= $DISK_THRESHOLD)}" && \
                log_message "ALERT" "BG: Disk ${pct}% on $(echo "$line"|awk '{print $1}')"
        done <<< "$(get_disk_usage)"

        sleep "$MONITOR_INTERVAL"
    done
}

# spawns the monitor as a background job and saves the PID so we can stop it later
start_monitoring() {
    if [ -f "$MONITORING_PID_FILE" ] && kill -0 "$(cat "$MONITORING_PID_FILE")" 2>/dev/null; then
        echo -e "${YELLOW}Monitoring already running (PID $(cat "$MONITORING_PID_FILE"))${RESET}"
        return
    fi
    background_monitor &
    echo $! > "$MONITORING_PID_FILE"
    echo -e "${GREEN}Background monitoring started (PID $!, interval ${MONITOR_INTERVAL}s)${RESET}"
    log_message "INFO" "Monitoring started"
}

# kills the background process using the saved PID file
stop_monitoring() {
    if [ -f "$MONITORING_PID_FILE" ]; then
        local pid
        pid=$(cat "$MONITORING_PID_FILE")
        if kill "$pid" 2>/dev/null; then
            echo -e "${GREEN}Monitoring stopped (PID $pid)${RESET}"
            log_message "INFO" "Monitoring stopped"
        else
            echo -e "${YELLOW}Process not found (already stopped?)${RESET}"
        fi
        rm -f "$MONITORING_PID_FILE"
    else
        echo -e "${YELLOW}No monitoring process found.${RESET}"
    fi
}

print_menu() {
    echo -e "\n${BOLD}${CYAN}  Server Health Monitor${RESET}"
    echo -e "  ${BOLD}1${RESET}) Display current system health"
    echo -e "  ${BOLD}2${RESET}) Configure monitoring thresholds"
    echo -e "  ${BOLD}3${RESET}) View activity logs"
    echo -e "  ${BOLD}4${RESET}) Clear logs"
    echo -e "  ${BOLD}5${RESET}) Start background monitoring"
    echo -e "  ${BOLD}6${RESET}) Stop background monitoring"
    echo -e "  ${BOLD}7${RESET}) Exit"
    echo ""
}

main() {
    check_dependencies
    # Ensure log file exists
    touch "$LOG_FILE" 2>/dev/null || { echo "ERROR: Cannot write to $LOG_FILE"; exit 1; }
    log_message "INFO" "Server monitor started"

    while true; do
        print_menu
        read -rp "Choose option [1-7]: " choice
        case "$choice" in
            1) display_health ;;
            2) configure_thresholds ;;
            3) view_logs ;;
            4) clear_logs ;;
            5) start_monitoring ;;
            6) stop_monitoring ;;
            7)
                stop_monitoring 2>/dev/null
                log_message "INFO" "Server monitor exited"
                echo -e "${GREEN}Goodbye!${RESET}"
                exit 0
                ;;
            *)
                echo -e "${RED}Invalid option. Please enter 1-7.${RESET}"
                ;;
        esac
        echo -e "\nPress Enter to continue..."
        read -r
    done
}
main
