#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>
// Jefferson Kline, 011734323

// I tried to make the code easy to read with comments, but the general flow is
// 1. Read input file and populate process list
// 2. Sort process list based on arrival time and process ID
// 3. For each scheduling algorithm (FCFS, SJF, RR):
// 4.   Reset process list to initial state
// 5.   Simulate the scheduling algorithm
// 6.   Print process specifics and summary data

// Each process is represented by a struct data type which includes:
// arrival time (A), CPU burst limit (B), total CPU time (C), multiplier (M), process ID, and runtime states

// TOC:
// Random Num Functions,    Line 60
// Printing Helpers,        Line 108
// Input Reading Function,  Line 214
// Tie Handler Functions,   Line 257
// Reset Function,          Line 291
// Queue FIFO,              Line 315
// Sims FCFS -> RR -> SJF,  Line 408, Line 516, Line 607
// Main Function,           Line 616


// Headers as needed

typedef enum {false, true} bool;        // Allows boolean types in C
typedef enum {UNSTARTED, READY, RUNNING, BLOCKED, TERMINATED} State;

/* Defines a job struct */
typedef struct Process {
    uint32_t A;                         // A: Arrival time of the process
    uint32_t B;                         // B: Upper Bound of CPU burst times of the given random integer list
    uint32_t C;                         // C: Total CPU time required
    uint32_t M;                         // M: Multiplier of CPU burst time
    uint32_t processID;                 // The process ID given upon input read
    uint32_t prev_burst_time;

    State state;
    uint32_t remaining_cpu;
    uint32_t cpu_burst_remaining;
    uint32_t io_burst_remaining;
    uint32_t rr_quantum_remaining;

    // Add other fields as needed, a lot of these were missing defs from base code given
    uint32_t finishingTime;
    uint32_t currentCPUTimeRun;
    uint32_t currentIOBlockedTime;
    uint32_t currentWaitingTime;
} _process;

uint32_t TOTAL_CREATED_PROCESSES = 0;   // The total number of processes constructed
uint32_t TOTAL_FINISHED_PROCESSES = 0;
uint32_t CURRENT_CYCLE = 0;
uint32_t TOTAL_NUMBER_OF_CYCLES_SPENT_BLOCKED = 0; 
uint32_t CPU_BUSY_TICKS = 0;
uint32_t IO_BUSY_TICKS = 0;

// Additional variables as needed
const uint32_t SEED_VALUE = 200;
static uint32_t rand_id = 0;
enum {RR_QUANTUM = 2};
FILE* random_file = NULL;

// ---------------------- RANDOM NUMBER FUNCTIONS ---------------------- //

/**
 * Reads a random non-negative integer X from a file with a given line named random-numbers (in the current directory)
 */
uint32_t getRandNumFromFile(uint32_t line, FILE* random_num_file_ptr){
    uint32_t end, loop;
    char str[512];

    rewind(random_num_file_ptr); // reset to be beginning
    for(end = loop = 0;loop<line;++loop){
        if(0==fgets(str, sizeof(str), random_num_file_ptr)){ //include '\n'
            end = 1;  //can't input (EOF)
            break;
        }
    }
    if(!end) {
        return (uint32_t) atoi(str);
    }

    // Fail-safe return
    return (uint32_t) 1804289383;
}

/**
 * Reads a random non-negative integer X from a file named random-numbers.
 * Returns the CPU Burst: : 1 + (random-number-from-file % upper_bound)
 */
uint32_t randomOS(uint32_t upper_bound, uint32_t process_indx, FILE* random_num_file_ptr)
{
    char str[20];
    
    uint32_t unsigned_rand_int = (uint32_t) getRandNumFromFile(SEED_VALUE+process_indx, random_num_file_ptr);
    uint32_t returnValue = 1 + (unsigned_rand_int % upper_bound);

    return returnValue;
} 

uint32_t next_random_num() {
    char buffer[64];
    if (!fgets(buffer, sizeof(buffer), random_file)) {
        fprintf(stderr, "Error reading random number.\n");
        exit(1);
    }
    return (uint32_t)strtoul(buffer, NULL, 10);
}

/********************* SOME PRINTING HELPERS *********************/


/**
 * Prints to standard output the original input
 * process_list is the original processes inputted (in array form)
 */
void printStart(_process process_list[])
{
    printf("The original input was: %u", TOTAL_CREATED_PROCESSES);

    uint32_t i = 0;
    for (; i < TOTAL_CREATED_PROCESSES; ++i)
    {
        printf(" ( %u %u %u %u)", process_list[i].A, process_list[i].B,
               process_list[i].C, process_list[i].M);
    }
    printf("\n");
} 

/**
 * Prints to standard output the final output
 * finished_process_list is the terminated processes (in array form) in the order they each finished in.
 */
void printFinal(_process finished_process_list[])
{
    printf("The (sorted) input is: %u", TOTAL_CREATED_PROCESSES);

    for (uint32_t i = 0; i < TOTAL_FINISHED_PROCESSES; ++i)
    {
        printf(" ( %u %u %u %u)", finished_process_list[i].A, finished_process_list[i].B,
               finished_process_list[i].C, finished_process_list[i].M);
    }
    printf("\n");
} // End of the print final function

/**
 * Prints out specifics for each process  (helper function, you may need to adjust variables accordingly)
 * @param process_list The original processes inputted, in array form
 */
void printProcessSpecifics(_process process_list[])
{
    uint32_t i = 0;
    printf("\n");
    for (; i < TOTAL_CREATED_PROCESSES; ++i)
    {
        printf("Process %i:\n", process_list[i].processID);
        printf("\t(A,B,C,M) = (%i,%i,%i,%i)\n", process_list[i].A, process_list[i].B,
               process_list[i].C, process_list[i].M);
        printf("\tFinishing time: %i\n", process_list[i].finishingTime);
        printf("\tTurnaround time: %i\n", process_list[i].finishingTime - process_list[i].A);
        printf("\tI/O time: %i\n", process_list[i].currentIOBlockedTime);
        printf("\tWaiting time: %i\n", process_list[i].currentWaitingTime);
        printf("\n");
    }
} // End of the print process specifics function

/**
 * Prints out the summary data (helper function, you may need to adjust variables accordingly)
 * process_list The original processes inputted in array form
 */
void printSummaryData(_process process_list[])
{
    uint32_t i = 0;
    double total_amount_of_time_utilizing_cpu = 0.0;
    double total_amount_of_time_io_blocked = 0.0;
    double total_amount_of_time_spent_waiting = 0.0;
    double total_turnaround_time = 0.0;
    uint32_t final_finishing_time = CURRENT_CYCLE;

    // i had to change some things here but kept the same logic, i also added average turnaround to the pirnt block in sumamry
    for (; i < TOTAL_CREATED_PROCESSES; ++i)
    {
        total_amount_of_time_utilizing_cpu += process_list[i].currentCPUTimeRun;
        total_amount_of_time_io_blocked += process_list[i].currentIOBlockedTime;
        total_amount_of_time_spent_waiting += process_list[i].currentWaitingTime;
        total_turnaround_time += (process_list[i].finishingTime - process_list[i].A);
    }

    // Calculates the CPU utilisation
    //double cpu_util = total_amount_of_time_utilizing_cpu / final_finishing_time;
    double cpu_util = (double) CPU_BUSY_TICKS / (double)final_finishing_time;

    // Calculates the IO utilisation
    //double io_util = (double) TOTAL_NUMBER_OF_CYCLES_SPENT_BLOCKED / final_finishing_time;
    double io_util = (double) IO_BUSY_TICKS / (double)final_finishing_time;

    // Calculates the throughput (Number of processes over the final finishing time times 100)
    double throughput =  100 * ((double) TOTAL_CREATED_PROCESSES/ final_finishing_time);

    // Calculates the average turnaround time
    double avg_turnaround_time = total_turnaround_time / TOTAL_CREATED_PROCESSES;

    // Calculates the average waiting time
    double avg_waiting_time = total_amount_of_time_spent_waiting / TOTAL_CREATED_PROCESSES;

    printf("Summary Data:\n");
    printf("\tFinishing time: %i\n", final_finishing_time);
    printf("\tAverage turnaround: %6f\n", avg_turnaround_time);
    printf("\tCPU Utilisation: %6f\n", cpu_util);
    printf("\tI/O Utilisation: %6f\n", io_util);
    printf("\tThroughput: %6f processes per hundred cycles\n", throughput);
    //printf("\tAverage turnaround time: %6f\n" avg_waiting_time);
} // End of the print summary data function

// ---------------------- INPUT READING FUNCTION ---------------------- //


// reads the input from the random number file and populates the process list
int read_inputs(FILE* in_file, _process** out_list) {

    // Other variables
    int temp_process_counter = 0;

    // Count num processes to handle based on ranomd int file
    fscanf(in_file, "%d", &temp_process_counter);
    TOTAL_CREATED_PROCESSES = (uint32_t)temp_process_counter;

    _process* temp = (_process*) malloc(temp_process_counter * sizeof(_process));

    for (uint32_t i = 0; i < (uint32_t)temp_process_counter; i++) {
        unsigned A, B, C, M;
        fscanf(in_file, " ( %u %u %u %u )", &A, &B, &C, &M);

        temp[i].A = A;
        temp[i].B = B;
        temp[i].C = C;
        temp[i].M = M;

        temp[i].processID =i;
        temp[i].state = UNSTARTED;
        temp[i].remaining_cpu = temp[i].C;
        temp[i].cpu_burst_remaining = 0;
        temp[i].io_burst_remaining = 0;
        temp[i].prev_burst_time = 0;
        temp[i].rr_quantum_remaining = 0;

        temp[i].finishingTime = 0;
        temp[i].currentCPUTimeRun = 0;
        temp[i].currentIOBlockedTime = 0;
        temp[i].currentWaitingTime = 0;
    }

    *out_list = temp;
    return temp_process_counter;
}

// ---------------------- TIE HANDLER FUNCTIONS ---------------------- //

// handles ties based on arrival time, then process ID
int tie_handler(const _process* process1, const _process* process2) {
    if (process1->A != process2->A) {
        return (process1->A < process2->A);
    } else {
        return (process1->processID < process2->processID);
    }
}

// qsort wrapper for tie handler
int tie_wrapper(const void* a, const void* b) {
    const _process* process_a = (const _process*)a;
    const _process* process_b = (const _process*)b;
    if (process_a->A != process_b->A) {
        if (process_a->A < process_b->A) {
            return -1;
        } else {
            return 1;
        }
    }
    if (process_a->processID != process_b->processID) {
        if (process_a->processID < process_b->processID) {
            return -1;
        } else {
            return 1;
        }
    }

    return 0;
}

// ---------------------- RESET FUNCTION ---------------------- //

// resets all the process data to initial values for re-simulation
void reset(_process* dest, _process* source, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        dest[i] = source[i];
        dest[i].state = UNSTARTED;
        dest[i].remaining_cpu = dest[i].C;
        dest[i].cpu_burst_remaining = 0;
        dest[i].io_burst_remaining = 0;
        dest[i].prev_burst_time = 0;
        dest[i].rr_quantum_remaining = 0;
        dest[i].finishingTime = 0;
        dest[i].currentCPUTimeRun = 0;
        dest[i].currentIOBlockedTime = 0;
        dest[i].currentWaitingTime = 0;
    } 
    TOTAL_FINISHED_PROCESSES = 0;
    CURRENT_CYCLE = 0;
    TOTAL_NUMBER_OF_CYCLES_SPENT_BLOCKED = 0;
    CPU_BUSY_TICKS = 0;
    IO_BUSY_TICKS = 0;
}

// ---------------------- QUEUE: FIFO --------------------- //


typedef struct {
    int* data;
    int front;
    int rear;
    int capacity;
    int size;
} Queue;

// initialize the queue
void initQueue(Queue* q, int capacity) {
    q->data = (int*)malloc(capacity * sizeof(int));
    q->front = 0;
    q->size = 0;
    q->rear = 0;
    q->capacity = capacity;
}

// enqueue an element to the queue
void enqueue(Queue* q, int value) {
    if (q->size == q->capacity) {
        printf(" Queue is full!\n");
        return;
    }
    q->data[q->rear] = value;
    q->rear = (q->rear + 1) % q->capacity;
    q->size++;
}

// dequeue an element from the queue
int dequeue(Queue* q) {
    if (q->size == 0) {
        printf("Queue is empty!\n");
        return -1;
    }
    int value = q->data[q->front];
    q->front = (q->front + 1) % q->capacity;
    q->size--;
    return value;
}

// check if the queue is empty
void freeQueue(Queue* q) {
    free(q->data);
    q->data = NULL;
    q->front = 0;
    q->rear = 0;
    q->capacity = 0;
    q->size = 0;
}

// ---------------------- schedule helpers --------------------- //

// checks for arriving processes at the current cycle
// void mark_arrivals(_process process_list[]) {
//     for (uint32_t i = 0; i < TOTAL_CREATED_PROCESSES; i++) {
//         if (process_list[i].A == CURRENT_CYCLE && process_list[i].state == UNSTARTED) {
//             process_list[i].state = READY;
//         }
//     }
// }

// checks if any processes are blocked
int any_processes_blocked(_process process_list[]) {
    for (uint32_t i = 0; i < TOTAL_CREATED_PROCESSES; i++) {
        if (process_list[i].state == BLOCKED) {
            return 1;
        }
    }
    return 0;
}

// tracks time for each process based on its state
int account_tick(_process process_list[]) {
    int blocked_found = 0;
    for (uint32_t i = 0; i < TOTAL_CREATED_PROCESSES; i++) {
        if (process_list[i].state == BLOCKED) {
            process_list[i].currentIOBlockedTime++;
            blocked_found = 1;
        } else if (process_list[i].state == READY) {
            process_list[i].currentWaitingTime++;
        } else if (process_list[i].state == RUNNING) {
            process_list[i].currentCPUTimeRun++;
            CPU_BUSY_TICKS++;
        }
    }
    return blocked_found;
}

// ---------------------- Simulation fcfs --------------------- //
// https://www.geeksforgeeks.org/operating-systems/program-for-fcfs-cpu-scheduling-set-1/
// ^ Aided me with understanding the basic logic to begin building my own implementation

// make errors if not static
static inline void start_burst_cpu(_process* process) {
    if (process->remaining_cpu == 0) {
        return;
    }

    uint32_t temp = next_random_num();
    uint32_t burst = 1 + (temp % process->B);

    if (burst > process->remaining_cpu) {
        burst = process->remaining_cpu;
    }

    process->cpu_burst_remaining = burst;
    process->prev_burst_time = burst;

}

// first come first serve, it is the simplest scheduling algorithm where the process that arrives first is served first
void sim_fcfs(_process process_list[]) {
    Queue q;
    initQueue(&q, TOTAL_CREATED_PROCESSES * 4 + 8);
    _process* current_process = NULL;

    CPU_BUSY_TICKS = 0;
    IO_BUSY_TICKS = 0;
    CURRENT_CYCLE = 0;
    TOTAL_FINISHED_PROCESSES = 0;

    for (uint32_t i = 0; i < TOTAL_CREATED_PROCESSES; i++) {
        if ((process_list[i].state == UNSTARTED) && (process_list[i].A == 0)) {
            process_list[i].state = READY; 
            enqueue(&q, (int)i);
        }
    }

    while (TOTAL_FINISHED_PROCESSES < TOTAL_CREATED_PROCESSES) {

        // mark_arrivals(process_list);
        // count IO busy once/cycle

        // if no current process, get next from queue
        if (!current_process && q.size > 0) {
            int next_index = dequeue(&q);
            current_process = &process_list[next_index];
            if (current_process->cpu_burst_remaining == 0) {
                start_burst_cpu(current_process);
            }
            current_process->state = RUNNING;
        }

        int blocked_this_cycle = account_tick(process_list);
        if (blocked_this_cycle) {
            IO_BUSY_TICKS++;
        }


        // advance current process if exists
        if (current_process) {
            if (current_process->cpu_burst_remaining > 0) {
                current_process->cpu_burst_remaining--;
                current_process->remaining_cpu--;
            }
        }

        // update blocked processes
        for (uint32_t i = 0; i < TOTAL_CREATED_PROCESSES; i++) {
            if (process_list[i].state == BLOCKED) {

                if (process_list[i].io_burst_remaining > 0) {
                    process_list[i].io_burst_remaining--;
                }
            }
        }

        //check if current process finished or needs to block
        if (current_process) {

            if (current_process->remaining_cpu == 0) {
                current_process->state = TERMINATED;
                current_process->finishingTime = CURRENT_CYCLE + 1;
                TOTAL_FINISHED_PROCESSES++;
                current_process = NULL;

            } else if (current_process->cpu_burst_remaining == 0) {
                current_process->state = BLOCKED;
                current_process->io_burst_remaining = current_process->prev_burst_time * current_process->M;
                current_process = NULL;
            }
        }

        for (uint32_t i = 0; i < TOTAL_CREATED_PROCESSES; i++) {
            if (process_list[i].state == BLOCKED && process_list[i].io_burst_remaining == 0) {
                process_list[i].state = READY;
                enqueue(&q, (int)i);
            }
        }

        CURRENT_CYCLE++;

        // check for new arrivals
        for(uint32_t i = 0; i < TOTAL_CREATED_PROCESSES; i++) {
            if (process_list[i].state == UNSTARTED && process_list[i].A == CURRENT_CYCLE) {
                process_list[i].state = READY; 
                enqueue(&q, (int)i);
            }
        }
    }

    freeQueue(&q);
}

// ---------------------- Simulation RR --------------------- //
//https://www.geeksforgeeks.org/operating-systems/program-for-round-robin-scheduling-for-the-same-arrival-time/
// This also helped me get an idea of how to do my own implementation.

void sim_rr(_process process_list[]) {
    Queue q;
    initQueue(&q, TOTAL_CREATED_PROCESSES * 4 + 8);
    _process* current_process = NULL;

    CPU_BUSY_TICKS = 0;
    IO_BUSY_TICKS = 0;
    CURRENT_CYCLE = 0;
    TOTAL_FINISHED_PROCESSES = 0;

    while (TOTAL_FINISHED_PROCESSES < TOTAL_CREATED_PROCESSES) {

        // check for new arrivals
        for(uint32_t i = 0; i < TOTAL_CREATED_PROCESSES; i++) {
            if (process_list[i].state == UNSTARTED && process_list[i].A == CURRENT_CYCLE) {
                process_list[i].state = READY; 
                enqueue(&q, (int)i);
            }
        }

        // if no current process, get next from queue
        if (!current_process && q.size > 0) {
            int next_index = dequeue(&q);
            current_process = &process_list[next_index];
            if (current_process->cpu_burst_remaining == 0) {
                start_burst_cpu(current_process);
            }
            current_process->state = RUNNING;
            current_process->rr_quantum_remaining = RR_QUANTUM;
        }

        int blocked_this_cycle = account_tick(process_list);
        if (blocked_this_cycle) {
            IO_BUSY_TICKS++;
        }

        if (current_process) {
            if (current_process->cpu_burst_remaining > 0) {
                current_process->cpu_burst_remaining--;
                current_process->remaining_cpu--;
            }

            if (current_process->rr_quantum_remaining > 0) {
                current_process->rr_quantum_remaining--;
            }
        }

        for (uint32_t i = 0; i < TOTAL_CREATED_PROCESSES; i++) {
            if (process_list[i].state == BLOCKED) {

                if (process_list[i].io_burst_remaining > 0) {
                    process_list[i].io_burst_remaining--;
                }
            }
        }

        if (current_process) {
            if (current_process->remaining_cpu == 0) {
                current_process->state = TERMINATED;
                current_process->finishingTime = CURRENT_CYCLE + 1;
                TOTAL_FINISHED_PROCESSES++;
                current_process = NULL;

            } else if (current_process->cpu_burst_remaining == 0) {
                current_process->state = BLOCKED;
                current_process->io_burst_remaining = current_process->prev_burst_time * current_process->M;
                current_process = NULL;

            } else if (current_process->rr_quantum_remaining == 0) {
                current_process->state = READY;
                enqueue(&q, (int)(current_process - process_list));
                current_process = NULL;
            }
        }

        for (uint32_t i = 0; i < TOTAL_CREATED_PROCESSES; i++) {
            if (process_list[i].state == BLOCKED && process_list[i].io_burst_remaining == 0) {
                process_list[i].state = READY;
                enqueue(&q, (int)i);
            }
        }

        CURRENT_CYCLE++;
    }

    freeQueue(&q);
}

// ---------------------- Simulation SJF --------------------- //

// selects the next process to run based on shortest job first
int pick_next_sjf(_process process_list[]) {
    int best_index = -1;

    // find the process with the shortest remaining CPU time
    for (uint32_t i = 0; i < TOTAL_CREATED_PROCESSES; i++) {
        if (process_list[i].state!= READY) {
            //printf("Notready\n");
            continue;
        }

        if (best_index == -1){
            best_index = (int)i;
            continue;
        }

        if (process_list[i].remaining_cpu < process_list[best_index].remaining_cpu) {
            best_index = (int )i;
            continue;
        }

        if (process_list[i].A < process_list[best_index].A) {
            best_index = (int)i;
            continue;
        }
        if (process_list[i].processID < process_list[best_index].processID) {
            best_index = (int)i;
            continue;
        }
    }
    return best_index;
}


void sim_sjf(_process process_list[]) {
    _process* current_process = NULL;

    CPU_BUSY_TICKS = 0;
    IO_BUSY_TICKS = 0;
    CURRENT_CYCLE = 0;
    TOTAL_FINISHED_PROCESSES = 0;

    while (TOTAL_FINISHED_PROCESSES < TOTAL_CREATED_PROCESSES) {

        // check for new arrivals
        for(uint32_t i = 0; i < TOTAL_CREATED_PROCESSES; i++) {
            if (process_list[i].state == UNSTARTED && process_list[i].A == CURRENT_CYCLE) {
                process_list[i].state = READY; 
            }
        }

        // if no current process, get next from SJF
        if (!current_process) {
            int next_index = pick_next_sjf(process_list);
            if (next_index != -1) {
                current_process = &process_list[next_index];
                if (current_process->cpu_burst_remaining == 0) {
                    start_burst_cpu(current_process);
                }
                current_process->state = RUNNING;
            }
        }

        int blocked_this_cycle = account_tick(process_list);
        if (blocked_this_cycle) {
            IO_BUSY_TICKS++;
        }

        if (current_process) {
            if (current_process->cpu_burst_remaining > 0) {
                current_process->cpu_burst_remaining--;
                current_process->remaining_cpu--;
            }
        }

        // update blocked processes
        for (uint32_t i = 0; i < TOTAL_CREATED_PROCESSES; i++) {
            if (process_list[i].state == BLOCKED) {

                if (process_list[i].io_burst_remaining > 0) {
                    process_list[i].io_burst_remaining--;
                }
            }
        }

        // check if current process finished or needs to block
        if (current_process) {
            if (current_process->remaining_cpu == 0) {
                current_process->state = TERMINATED;
                current_process->finishingTime = CURRENT_CYCLE + 1;
                TOTAL_FINISHED_PROCESSES++;
                current_process = NULL;

            } else if (current_process->cpu_burst_remaining == 0) {
                current_process->state = BLOCKED;
                current_process->io_burst_remaining = current_process->prev_burst_time * current_process->M;
                current_process = NULL;
            }
        }

        // check for processes that have finished blocking
        for (uint32_t i = 0; i < TOTAL_CREATED_PROCESSES; i++) {
            if (process_list[i].state == BLOCKED && process_list[i].io_burst_remaining == 0) {
                process_list[i].state = READY;
            }
        }
        CURRENT_CYCLE++;
    }
}

// ---------------------- MAIN FUNCTION ---------------------- //


int main(int argc, char *argv[])
{
    uint32_t total_num_of_process;               // Read from the file -- number of process to create
    //_process* process_list[total_num_of_process]; // Creates a container for all processes    

    _process* process_list = NULL;

    if (argc < 2) {
        fprintf(stderr, "Usage is %s <input_file>\n", argv[0]);
        return 1;
    }

    char* input_file_path = argv[1];
    char* random_file_path = (argc >= 3) ? argv[2] : "random-numbers";

    FILE* input_file = fopen(input_file_path, "r");
    random_file = fopen(random_file_path, "r");

    total_num_of_process = read_inputs(input_file, &process_list);
    fclose(input_file);

    TOTAL_CREATED_PROCESSES = total_num_of_process;

    printStart(process_list);
    qsort(process_list, TOTAL_CREATED_PROCESSES, sizeof(_process), tie_wrapper);

    _process* process_list_copy = ( _process*) malloc(TOTAL_CREATED_PROCESSES * sizeof(_process));

    printf("\n------------------- FCFS --------------------\n");

    reset(process_list_copy, process_list, TOTAL_CREATED_PROCESSES);
    sim_fcfs(process_list_copy);
    printProcessSpecifics(process_list_copy);
    printSummaryData(process_list_copy);
    reset(process_list_copy, process_list, TOTAL_CREATED_PROCESSES);

    printf("\n------------------- SJF --------------------\n");

    sim_sjf(process_list_copy);
    printProcessSpecifics(process_list_copy);
    printSummaryData(process_list_copy);
    reset(process_list_copy, process_list, TOTAL_CREATED_PROCESSES);

    printf("\n------------------- RR --------------------\n" );

    sim_rr(process_list_copy);
    printProcessSpecifics(process_list_copy);
    printSummaryData(process_list_copy);
    reset(process_list_copy, process_list, TOTAL_CREATED_PROCESSES);

    free(process_list_copy);
    free(process_list);
    fclose(random_file);

    return 0;
} 