#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "mem.h"
#include "mem_internals.h"
#include "util.h"

#define HEAP_SIZE (4 * 4096)

int passed_tests = 0;

char* run_test(char* (*test_fn)(), const char* test_name) {
    printf("Running test: %s\n", test_name);
    void* addr = heap_init(HEAP_SIZE);
    if (addr == NULL) {
        printf("Failed to initialize heap for %s\n", test_name);
        return "Heap initialization failed";
    }
    char* warning = test_fn();
    heap_term();
    if (!warning) {
        printf("%s -- PASSED\n", test_name);
        passed_tests++;
    } else {
        printf("%s -- FAILED: %s\n", test_name, warning);
    }
    return warning;
}

static char* test_success_allocation() {
    void* ptr = _malloc(64);
    if (ptr == NULL) {
        return "Failed to allocate memory.";
    }
    return NULL;
}

static char* test_single_free_block() {
    void* arr[3] = {_malloc(128), _malloc(64), _malloc(32)};
    _free(arr[1]);
    struct block_header* first_block = (struct block_header*)(arr[1] - offsetof(struct block_header, contents));
    if (!first_block->is_free) {
        return "Block wasn't freed.";
    }
    return NULL;
}

static char* test_free_two_blocks() {
    void* arr[3] = {_malloc(256), _malloc(256), _malloc(256)};
    _free(arr[0]);
    _free(arr[2]);
    struct block_header* block1_header = (struct block_header*)(arr[0] - offsetof(struct block_header, contents));
    struct block_header* block3_header = (struct block_header*)(arr[2] - offsetof(struct block_header, contents));
    if (!block1_header->is_free || !block3_header->is_free) {
        return "Not all blocks were freed.";
    }
    return NULL;
}

static char* test_new_region_expands_old_region() {
    void* initial = _malloc(HEAP_SIZE - 64);
    void* extended = _malloc(HEAP_SIZE);
    if (initial == NULL || extended == NULL) {
        return "Failed to expand.";
    }
    struct block_header* initial_block = (struct block_header*)(initial - offsetof(struct block_header, contents));
    struct block_header* extended_block = (struct block_header*)(extended - offsetof(struct block_header, contents));
    if (initial_block->next != extended_block) {
        return "It was not expanded correctly.";
    }
    return NULL;
}

static char* test_regions_expansion_other_place() {
    struct block_header* start = HEAP_START;
    void* prvt = mmap(HEAP_START + REGION_MIN_SIZE, 100,0,MAP_PRIVATE | 0x20,-1,0);
    if (prvt == MAP_FAILED)
        return "Mmap failed to allocate region";
    _malloc(REGION_MIN_SIZE - offsetof(struct block_header, contents));
    struct block_header* expansion = (struct block_header*)(_malloc(REGION_MIN_SIZE - offsetof(struct block_header, contents)) - offsetof(struct block_header, contents));
    if (munmap(prvt, 100) == -1)
        return "Munmap failed to free region";
    if (start->next != expansion)
        return "Wrong reference to a new region";
    if (start + offsetof(struct block_header, contents) + start->capacity.bytes == expansion)
        return "Wrong location of a new region";
    return NULL;
}

int main() {
    run_test(test_success_allocation, "Test success allocation");
    run_test(test_single_free_block, "Test single block freeing");
    run_test(test_free_two_blocks, "Test two blocks freeing");
    run_test(test_new_region_expands_old_region, "Test region expansion");
    run_test(test_regions_expansion_other_place, "Test regions expansions");
    printf("-----\n");
    printf("Passed %d tests out of 5\n", passed_tests);
    return 0;
}
