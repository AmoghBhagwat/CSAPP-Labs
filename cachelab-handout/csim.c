#include "cachelab.h"
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>

bool verbose = false;
int setIndexBits;
int tagBits;
int associativity;
int blockBits;
FILE *tracefile;
long long accessTime = 0;

int hits = 0;
int misses = 0;
int evictions = 0;

typedef struct CacheLine {
    bool valid;     // 1 bit
    int tag;        // t bits
    int lastUsed;
    char *data;     // 2^b bytes
} CacheLine;

typedef struct CacheSet {
    CacheLine *lines;
} CacheSet;

typedef struct Cache {
    CacheSet *sets;
} Cache;

void accessCache(Cache* cache, long long address) {
    int setIndex = (address >> blockBits) & ((1LL << setIndexBits) - 1);
    int tag = address >> (blockBits + setIndexBits);
    if (verbose) {
        printf("setIndex: %d, tag: %d ", setIndex, tag);
    }
    for (int i = 0; i < associativity; i++) {
        if (cache->sets[setIndex].lines[i].valid && cache->sets[setIndex].lines[i].tag == tag) {
            cache->sets[setIndex].lines[i].lastUsed = accessTime++;
            hits++;
            if (verbose) {
                printf("hit ");
            }
            return;
        }
    }
    misses++;
    if (verbose) {
        printf("miss ");
    }
    for (int i = 0; i < associativity; i++) {
        if (!cache->sets[setIndex].lines[i].valid) {
            cache->sets[setIndex].lines[i].valid = 1;
            cache->sets[setIndex].lines[i].tag = tag;
            cache->sets[setIndex].lines[i].lastUsed = accessTime++;
            return;
        }
    }
    evictions++;
    if (verbose) {
        printf("eviction ");
    }
    int max = 0;
    for (int i = 0; i < associativity; i++) {
        if (cache->sets[setIndex].lines[i].lastUsed < cache->sets[setIndex].lines[max].lastUsed) {
            max = i;
        }
    }
    cache->sets[setIndex].lines[max].tag = tag;
    cache->sets[setIndex].lines[max].lastUsed = accessTime++;
}

Cache* createCache(int setIndexBits, int associativity) {
    Cache *cache = (Cache*) malloc(sizeof(Cache));
    int numSets = 1LL << setIndexBits;
    cache->sets = (CacheSet*) malloc(numSets * sizeof(CacheSet));
    for (int i = 0; i < numSets; i++) {
        cache->sets[i].lines = (CacheLine*) malloc(associativity * sizeof(CacheLine));
        for (int j = 0; j < associativity; j++) {
            cache->sets[i].lines[j].valid = 0;
            cache->sets[i].lines[j].tag = 0;
            cache->sets[i].lines[j].lastUsed = 0;
            cache->sets[i].lines[j].data = (char*) malloc((1LL << blockBits) * sizeof(char));
        }
    }
    return cache;
}

void cleanupCache(Cache *cache) {
    int numSets = 1LL << setIndexBits;
    for (int i = 0; i < numSets; i++) {
        for (int j = 0; j < associativity; j++) {
            free(cache->sets[i].lines[j].data);
        }
        free(cache->sets[i].lines);
    }
    free(cache->sets);
    free(cache);
}

void simulateCache(Cache* cache) {
    char operation;
    long long address;
    int size;
    while (fscanf(tracefile, " %c %llx,%d", &operation, &address, &size) == 3) {
        if (verbose && (operation == 'L' || operation == 'S' || operation == 'M')) {
            printf("%c %llx,%d ", operation, address, size);
        }
        switch (operation) {
        case 'L':
            accessCache(cache, address);
            break;
        case 'S':
            accessCache(cache, address);
            break;
        case 'M':
            accessCache(cache, address);
            accessCache(cache, address);
            break;
        default:
            break;
        }
        if (verbose && (operation == 'L' || operation == 'S' || operation == 'M')) {
            printf("\n");
        }
    }
}

void parseArguments(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n", argv[0]);
        exit(1);
    }

    int opt;
    while ((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
        switch (opt) {
        case 'h':
            printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n", argv[0]);
            printf("Options:\n");
            printf("\t-h\t\tPrint this help message.\n");
            printf("\t-v\t\tOptional verbose flag.\n");
            printf("\t-s <num>\tNumber of set index bits.\n");
            printf("\t-E <num>\tNumber of lines per set.\n");
            printf("\t-b <num>\tNumber of block offset bits.\n");
            printf("\t-t <file>\tTrace file.\n");
            printf("\n");
            printf("Examples:\n");
            printf("\tlinux>  %s -s 4 -E 1 -b 4 -t traces/yi.trace\n", argv[0]);
            printf("\tlinux>  %s -v -s 8 -E 2 -b 4 -t traces/yi.trace\n", argv[0]);
            exit(1);
            break;
        case 'v':
            verbose = true;
            break;
        case 's':
            setIndexBits = atoi(optarg);
            break;
        case 'E':
            associativity = atoi(optarg);
            break;
        case 'b':
            blockBits = atoi(optarg);
            break;
        case 't':
            tracefile = fopen(optarg, "r");
            break;
        default:
            fprintf(stderr, "Usage: %s [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n", argv[0]);
            exit(1);
        }

        tagBits = 64 - setIndexBits - blockBits;
    }
}

int main(int argc, char **argv) {
    parseArguments(argc, argv);

    Cache* cache = createCache(setIndexBits, associativity);
    simulateCache(cache);
    cleanupCache(cache);
    
    printSummary(hits, misses, evictions);
    return 0;
}
