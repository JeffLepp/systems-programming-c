#include <getopt.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <stdbool.h>
// final cleand version

#define ADDRESS_LENGTH 64  // 64-bit memory addressing

// this is cache line, we need to see if it contains anything, tag for hits, and least recently used
typedef struct {
    bool valid;
    uint64_t tag;
    uint64_t lru;   // larger is more recently used
} Line;

// this is cache set 
typedef struct {
    Line *lines;
} Set;

// handling inputs
typedef struct {
    int s;  // set index bits
    int E;    // num lines for set
    int b;  // block offset bits 
    const char *trace;
    int verbose;
} config_t;

// actual cache container
typedef struct {
    Set *sets;
    int s, E, b;
    uint64_t time; //counter for lru
} Cache;

int global_hits = 0;
int global_misses = 0;
int global_evictions = 0;

// other variables as needed
typedef enum { HIT, MISS, MISS_AND_EVICT} AccessResult;

static char* result_to_word(AccessResult result) {
    return (result == HIT) ? "hit" : (result == MISS ? "miss" : "miss eviction");
}

// 1ULL is a unsigned long long and the replies show a cool way to iterate with it
// https://community.unix.com/t/c-code-1ull-and-bit-calculation/330493
// S will be 2^s
size_t num_sets_from_bits(int s) {
    return (size_t)1ULL << s;
}
// size_t sets_from_bits(int s) {
//     return (size_t)1 << s;
// }

/* 
 * this function provides a standard way for your cache
 * simulator to display its final statistics (i.e., hit and miss)
 */ 
void print_summary(int hits, int misses, int evictions)
{
    printf("hits:%d misses:%d evictions:%d\n", hits, misses, evictions);
}

/*
 * print usage info
 */
// after some dbugging i need to chnage this from argv[] to ptr to char
void print_usage(char *ptr)
{
    printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n", ptr);
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <num>   Number of set index bits.\n");
    printf("  -E <num>   Number of lines per set.\n");
    printf("  -b <num>   Number of block offset bits.\n");
    printf("  -t <file>  Trace file.\n");
    printf("\nExamples:\n");
    printf("  linux>  %s -s 4 -E 1 -b 4 -t traces/trace01.dat\n", ptr);
    printf("  linux>  %s -v -s 8 -E 2 -b 4 -t traces/trace01.dat\n", ptr);
    exit(0);
}

int parse_input(char *arg, char *flag, int *out) {
    char *end = NULL;
    errno = 0;

    // change atoi to strtol for error checking, had to change it
    long conversion = strtol(arg, &end, 10);
    
    if (errno || end == arg || *end != '\0' || conversion < 0 || conversion > 1<<30) {
        //perror("enexpected input")
        fprintf(stderr, "Error: Invalid value '%s' for %s\n", arg, flag);
        return 0;
    }

    *out = (int)conversion;
    return 1;
}

// int s;  // set index bits
// int E;    // num lines for set
// int b;  // block offset bits 
// set, evictions, block
// makes empty cache, s sets, E lines, all zeors
Cache *create(int s, int E, int b) {
    size_t S = num_sets_from_bits(s);

    Cache *cache = (Cache*)calloc(1, sizeof(Cache));
    if (!cache) {
        perror("calloc for cache init failed");
        exit(1);
    }
    cache->s = s;
    cache->E = E;
    cache->b = b; 
    cache->time = 0;

    cache->sets = (Set*)calloc(S, sizeof(Set));
    if (!cache->sets) { 
        perror("calloc sets"); 
        exit(1); 
    }

    for (size_t i = 0; i < S; i++) {
        cache->sets[i].lines = (Line*)calloc((size_t)E, sizeof(Line));
        if (!cache->sets[i].lines) { 
            perror("calloc lines"); 
            exit(1); 
        }
    }

    return cache;
}

void free_cache(Cache *cache) {
    if (!cache) {
        printf("No cache to free");
        return;
    }
    size_t S = num_sets_from_bits(cache->s);
    for (size_t i = 0; i < S; i++) {
        free(cache->sets[i].lines);
    }
    free(cache->sets);
    free(cache);
}

// need 64 bit, split into set index tag
void decode_address(uint64_t address, int s, int b, uint64_t *set_id, uint64_t *tag) {
    uint64_t mask = ((uint64_t)1 << s) - 1;
    *set_id = (address >> b) & mask;
    *tag = address >> (s + b);
}

// similar logic to "https://coffeebeforearch.github.io/2020/12/16/cache-simulator.html" 
// so this needs to be access , hit/miss/evic, and is based of lru as victim
// need global vars
// preforms one access/update
AccessResult access_cache(Cache *cache, uint64_t address) {
    cache->time++;     

    // int s;  // set index bits
    // int E;    // num lines for set
    // int b;  // block offset bits 
    // set, evictions, block      

    uint64_t set_id, tag; 
    decode_address(address, cache->s, cache->b, &set_id, &tag);
    Set *set = &cache->sets[set_id];

    // try hit
    for (int i = 0; i < cache->E; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            global_hits++;
            set->lines[i].lru = cache->time;
            return HIT;
        }
    }

    // miss fill empty
    global_misses++;
    for (int i = 0; i < cache->E; i++) {
        if (!set->lines[i].valid) {
            set->lines[i].valid = true;
            set->lines[i].tag   = tag;
            set->lines[i].lru   = cache->time;
            return MISS;
        }
    }

    // miss evict
    int victim = 0;
    for (int i = 1; i < cache->E; i++) {
        if (set->lines[i].lru < set->lines[victim].lru) {
            victim = i; // ties ok
        }
    }
    global_evictions++;
    set->lines[victim].tag = tag;
    set->lines[victim].lru = cache->time;
    return MISS_AND_EVICT;
}

char* result(AccessResult result){
    if (result == HIT) {
        return "hit";
    } else if (result == MISS) {
        return "miss";
    } else {
        return "miss eviction";
    }
    return "errorerror";
}

//
// void modify(Cache *cache, uint64_t address, int size, int verbose) {
//     access_cache(cache, 'M', address, size, verbose);
//     access_cache(cache, 'M', address, size, verbose);
// }

int replay_trace(Cache *cache, const char *tracefile, int verbose) {
    FILE *file = fopen(tracefile, "r");
    if (!file) {
        perror("file fail");
        return -1;
    }

    // no space before I but M L S will

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char op;
        unsigned long long address_temp = 0ULL;
        int size = 0;

        // Instruction load, M is? L is? S is?

        if (line[0] == 'I') {
            // instruction load not tested for 
            continue;
        } else if (sscanf(line, " %c %llx,%d", &op, &address_temp, &size) != 3) {
            perror("line not formed correctly");
            fclose(file);
            exit(1);
        }

        uint64_t address = (uint64_t)address_temp;

        if (op == 'L' || op == 'S') {
            AccessResult result = access_cache(cache, address);
            if (verbose) {
                printf("%c %llx,%d %s\n", op, address_temp, size, result_to_word(result));
            }
        } else if (op == 'M') {
            AccessResult result1 = access_cache(cache, address);            
            AccessResult result2 = access_cache(cache, address);
            if (verbose) {
                printf("%c %llx,%d %s %s\n", op, address_temp, size, result_to_word(result1), result_to_word(result2));
            }
        } else {
            continue;
        }

    }
    fclose(file);
    return 0;
}

/*
 * starting point
 */
int main(int argc, char* argv[])
{
	// complete your simulator
    // int s;  // set index bits
    // int E;    // num lines for set
    // int b;  // block offset bits 
    // set, evictions, block 
    config_t cfg;
    cfg.s = -1;
    cfg.E = -1;
    cfg.b = -1;
    cfg.verbose = 0;
    int option;

    while ((option = getopt(argc, argv, "s:E:b:t:vh")) != -1) {
        switch(option) {
            case 's':
                cfg.s = atoi(optarg);
                break;
                
            case 'E':
                cfg.E = atoi(optarg);
                break;

            case 'b':
                cfg.b = atoi(optarg);
                break;

            case 't':
                cfg.trace = optarg;
                break;

            case 'v':
                cfg.verbose = 1;
                break;

            case 'h':
                print_usage(argv[0]);
                return 0;

            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    if (cfg.s < 0 || cfg.E <= 0 || cfg.b < 0 || cfg.trace == NULL) {
        print_usage(argv[0]);
        return 1;
    }
    Cache *cache = create(cfg.s, cfg.E, cfg.b);

    int runinfo = replay_trace(cache, cfg.trace, cfg.verbose);
    print_summary(global_hits, global_misses, global_evictions);
    free_cache(cache);
    return runinfo;

    // output cache hit and miss statistics
    //print_summary(hit_count, miss_count, eviction_count);
    
    // assignment done. life is good!
    return 0;
}
