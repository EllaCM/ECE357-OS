#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdbool.h>

#include "tas.h"

#define NSLOTS_TEST 10
#define NSLOTS 100
#define NPROCS 10
#define NITERS 100000
#define NRANGE 1000

typedef struct {
    int data;
    int lock;
} shared_m;

struct dll {
    int value;
    struct dll *fwd, *rev;
};

struct naive_slab {
    char freemap[NSLOTS_TEST];
    struct dll slots[NSLOTS_TEST];
};

struct slab {
    int spinlock;
    char freemap[NSLOTS];
    struct dll slots[NSLOTS];
};

// put seqlock relevent locks and count in the slab for the memory efficiency
struct seq_slab {
    int spinlock;
    int count;
    int countlock;
    int dll_spinlock;
    char freemap[NSLOTS];
    struct dll slots[NSLOTS];
};

void spin_lock(int *lock);

void spin_unlock(int *lock);

void test1();

void test2();

void test5();

void test6();

/* Allocate an object from the slab and return a pointer to it, or NULL
 * if the slab is full */
void *naive_slab_alloc(struct naive_slab *slab);

void *slab_alloc(struct slab *slab);

void *seq_slab_alloc(struct seq_slab *slab);

/* Free an object whose address is the second parameter, within the slab
 * pointed to by the first parameter. Return -1 if the second parameter
 * is not actually the valid address of such an object, or if the object
 * pointer corresponds to a slot which is currently marked as free.
 * Return 1 on success */
int naive_slab_dealloc(struct naive_slab *slab, void *object);

int slab_dealloc(struct slab *slab, void *object);

int seq_slab_dealloc(struct seq_slab *slab, void *object);

/* The first parameter is a pointer to the anchor of a circular, DLL
* which is in ascending numerical order of the node->value. The fwd
* pointer of the anchor therefore points to the node with the lowest
* value, and the rev pointer to the highest. The second parameter
* is the value of a new node, which is to be allocated using the
* slab_alloc function, and inserted into the proper place in the list
* The return value is a pointer to the inserted node, or NULL on failure */
struct dll *dll_insert(struct dll *anchor, int value, struct slab *slab);

/* Unlink the given node and free it using slab_dealloc. Note that while
* it is not strictly necessary to supply the anchor, having it might be
* helpful, depending on the locking strategy you employ. You can
* simplify the problem by assuming that the given node is in fact
* contained on the given list. */
void dll_delete(struct dll *anchor, struct dll *node, struct slab *slab);

/* Find the node on the specified list with the specified value and
* return a pointer to it, or NULL if no such node exists. Optimize
* the search with the knowledge that the list is stored in sorted order */
struct dll *dll_find(struct dll *anchor, int value);

struct dll *seq_dll_insert(struct dll *anchor, int value, struct seq_slab *slab);

void seq_dll_delete(struct dll *anchor, struct dll *node, struct seq_slab *slab);

struct dll *seq_dll_find(struct dll *anchor, int value, struct seq_slab *slab);

void write_seqlock(struct seq_slab *s);

void write_sequnlock(struct seq_slab *s);

int read_seqbegin(struct seq_slab *s);

int read_seqretry(struct seq_slab *s, int orig);

void print_dll(struct dll *anchor);

void dll_find_and_delete(struct dll *anchor, int value, struct slab *slab);

void seq_dll_find_and_delete(struct dll *anchor, int value, struct seq_slab *slab);

bool is_dll_sorted(struct dll *anchor);

void get_time(struct timeval *time);

void print_time_info(struct timeval *result);

void print_info();

static shared_m *m;

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: hw6 test_case_number\n");
        exit(EXIT_FAILURE);
    }
    int test_case = atoi(argv[1]);
    switch (test_case) {
        case 1:
            test1();
            break;
        case 2:
            test2();
            break;
        case 5:
            test5();
            break;
        case 6:
            test6();
            break;
        default:
            fprintf(stderr, "undefined test case number\n");
            exit(EXIT_FAILURE);
    }
    return 0;
}

// test spinlock
void test1() {
    // without spinlock
    int expected = NITERS * NPROCS;
    m = mmap(NULL, sizeof(*m),
             PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS,
             -1, 0);
    if (m == MAP_FAILED) {
        fprintf(stderr, "Failed to map memory: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    m->data = 0;
    int i;
    int j;
    for (i = 0; i < NPROCS; i++) {
        switch (fork()) {
            case -1:
                perror("Failed to fork");
                exit(EXIT_FAILURE);
            case 0:
                for (j = 0; j < NITERS; j++) {
                    m->data++;
                }
                exit(EXIT_SUCCESS
                );
            default:
                break;
        }
    }
    while ((wait(NULL)) > 0);
    fprintf(stdout, "Without Spinlock: expected: %d, result: %d, difference: %d\n",
            expected, m->data, expected - m->data);

    // with spinlock
    m->data = 0;
    for (i = 0; i < NPROCS; i++) {
        switch (fork()) {
            case -1:
                perror("Failed to fork");
                exit(EXIT_FAILURE);
            case 0:
                for (j = 0; j < NITERS; j++) {
                    spin_lock(&m->lock);
                    m->data++;
                    spin_unlock(&m->lock);
                }
                exit(EXIT_SUCCESS);
            default:
                break;
        }
    }
    while ((wait(NULL)) > 0);
    fprintf(stdout, "With Spinlock: expected: %d, result: %d, difference: %d\n",
            expected, m->data, expected - m->data);
}

// test slab works
void test2() {
    struct naive_slab s;
    memset(s.freemap, 0, sizeof(s.freemap));
    int i;
    for (i = 1; i < NSLOTS_TEST + 2; i++) {
        fprintf(stderr, "%d ", i);
        struct dll *tmp = naive_slab_alloc(&s);
        if (tmp == NULL) {
            fprintf(stderr, "failed\n");
        } else {
            fprintf(stderr, "inserted\n");
        }
        for (int j = 0; j < NSLOTS; j++) {
            fprintf(stderr, "%d ", s.freemap[j]);
        }
        fprintf(stderr, "\n");
    }

    for (i = 0; i < NSLOTS_TEST + 1; i++) {
        if (naive_slab_dealloc(&s, &s.slots[i]) == 1) fprintf(stderr, "%d deleted\n", i);
        else fprintf(stderr, "%d failed\n", i);
        for (int j = 0; j < NSLOTS; j++) {
            fprintf(stderr, "%d ", s.freemap[j]);
        }
        fprintf(stderr, "\n");
    }
}

// test slab with spinlock
void test5() {
    print_info();
    struct timeval start, end, result;
    int i, j;
    struct slab *s = mmap(NULL, sizeof(*s), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS,
                          -1, 0);
    if (s == MAP_FAILED) {
        fprintf(stderr, "Failed to map memory: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
//    struct dll *anchor = create_anchor();
    struct dll *anchor = slab_alloc(s);
    anchor->fwd = anchor;
    anchor->rev = anchor;

    get_time(&start);
    for (i = 0; i < NPROCS; i++) {
        switch (fork()) {
            case -1:
                perror("Failed to fork");
                exit(EXIT_FAILURE);
            case 0:
                srand(time(0) * i * 123);
                struct dll *tmp;
                for (j = 0; j < NITERS; j++) {
                    int v1 = (rand());
                    int v2 = (rand()) % NRANGE;
                    switch(v1%2) {
                        case 0:
                            tmp = dll_insert(anchor, v2, s);
                            break;
                        case 1:
                            tmp = dll_find(anchor, v2);
                            if (tmp != NULL) {
                                dll_delete(anchor, tmp, s);
                            }
//                            consulted Jonathan Pedoeem about duplicate find,
//                            if there is duplicate element
//                            dll_find_and_delete(anchor, v2, s);
                            break;
                        default:
                            break;
                    }
                }
                exit(EXIT_SUCCESS);
            default:
                break;
        }
    }
    while ((wait(NULL)) > 0);
    get_time(&end);
    timersub(&end, &start, &result);
    print_time_info(&result);

    print_dll(anchor);
    if (is_dll_sorted(anchor)) fprintf(stderr, "dll is sorted!\n");
    else fprintf(stderr, "dll is not sorted!\n");
}

// test slab with seqlock
void test6() {
    print_info();
    struct timeval start, end, result;
    int i, j;
    m = mmap(NULL, sizeof(*m),
             PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS,
             -1, 0);
    if (m == MAP_FAILED) {
        fprintf(stderr, "Failed to map memory: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    struct seq_slab *s = mmap(NULL, sizeof(*s), PROT_READ | PROT_WRITE,
                              MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (s == MAP_FAILED) {
        fprintf(stderr, "Failed to map memory: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    struct dll *anchor = seq_slab_alloc(s);
    anchor->fwd = anchor;
    anchor->rev = anchor;

    get_time(&start);
    for (i = 0; i < NPROCS; i++) {
        switch (fork()) {
            case -1:
                perror("Failed to fork");
                exit(EXIT_FAILURE);
            case 0:
                srand(time(0) * i * 123);
                struct dll *tmp;
                for (j = 0; j < NITERS; j++) {
                    int v1 = (rand());
                    int v2 = (rand()) % NRANGE;
                    switch(v1%2) {
                        case 0:
                            tmp = seq_dll_insert(anchor, v2, s);
                            break;
                        case 1:
                            tmp = seq_dll_find(anchor, v2, s);
                            if (tmp != NULL) {
                                seq_dll_delete(anchor, tmp, s);
                            }
//                            seq_dll_find_and_delete(anchor, v2, s);
                            break;
                        default:
                            break;
                    }
                }
                exit(EXIT_SUCCESS);
            default:
                break;
        }
    }
    while ((wait(NULL)) > 0);
    get_time(&end);
    timersub(&end, &start, &result);
    print_time_info(&result);

    print_dll(anchor);
    if (is_dll_sorted(anchor)) fprintf(stderr, "dll is sorted!\n");
    else fprintf(stderr, "dll is not sorted!\n");
    fprintf(stderr, "The number of retry: %d\n", m->data);
}

void print_dll(struct dll *anchor) {
    struct dll *it = anchor->fwd;
    while (it != anchor) {
        fprintf(stderr, "%d ", it->value);
        it = it->fwd;
    }
    fprintf(stderr, "\n");
}

bool is_dll_sorted(struct dll *anchor) {
    struct dll *it = anchor->fwd;
    while (it != anchor) {
        it = it->fwd;
    }
    return true;
}

void spin_lock(int *lock) { while (tas((char *) lock) != 0); }

void spin_unlock(int *lock) { *lock = 0; }

void *naive_slab_alloc(struct naive_slab *slab) {
    int i;
    for (i = 0; i < NSLOTS; i++) {
        if (slab->freemap[i] == 0) {
            slab->freemap[i] = 1;
            return &slab->slots[i];
        }
    }
    return NULL;
}

int naive_slab_dealloc(struct naive_slab *slab, void *object) {
    int idx = (struct dll *) object - slab->slots;
    if (idx >= NSLOTS || slab->freemap[idx] == 0) {
        return -1;
    }
    slab->freemap[idx] = 0;
    return 1;
}

void *slab_alloc(struct slab *slab) {
    int i;
    for (i = 0; i < NSLOTS; i++) {
        spin_lock(&slab->spinlock);
        if (slab->freemap[i] == 0) {
            slab->freemap[i] = 1;
            spin_unlock(&slab->spinlock);
            return slab->slots + i;
        }
        spin_unlock(&slab->spinlock);
    }
    return NULL;
}

int slab_dealloc(struct slab *slab, void *object) {
    spin_lock(&slab->spinlock);
    int idx = (struct dll *) object - slab->slots;
    if (idx >= NSLOTS || slab->freemap[idx] == 0) {
        spin_unlock(&slab->spinlock);
        return -1;
    }
    slab->freemap[idx] = 0;
    spin_unlock(&slab->spinlock);
    return 1;
}

void *seq_slab_alloc(struct seq_slab *slab) {
    int i;
    for (i = 0; i < NSLOTS; i++) {
        spin_lock(&slab->spinlock);
        if (slab->freemap[i] == 0) {
            slab->freemap[i] = 1;
            spin_unlock(&slab->spinlock);
            return slab->slots + i;
        }
        spin_unlock(&slab->spinlock);
    }
    return NULL;
}

int seq_slab_dealloc(struct seq_slab *slab, void *object) {
    spin_lock(&slab->spinlock);
    int idx = (struct dll *) object - slab->slots;
    if (idx >= NSLOTS || slab->freemap[idx] == 0) {
        spin_unlock(&slab->spinlock);
        return -1;
    }
    slab->freemap[idx] = 0;
    spin_unlock(&slab->spinlock);
    return 1;
}

struct dll *dll_insert(struct dll *anchor, int value, struct slab *slab) {
    if (anchor == NULL || slab == NULL) return NULL;
    spin_lock(&anchor->value);
    struct dll *new = slab_alloc(slab);
    if (new == NULL) {
        spin_unlock(&anchor->value);
        return NULL;
    }
    new->value = value;
    struct dll *it = anchor;
    while (it->fwd->value < value && it->fwd != anchor) {
        it = it->fwd;
    }
    struct dll *next = it->fwd;
    it->fwd = new;
    new->rev = it;
    new->fwd = next;
    next->rev = new;
    spin_unlock(&anchor->value);
    return new;
}

void dll_delete(struct dll *anchor, struct dll *node, struct slab *slab) {
    spin_lock(&anchor->value);
    if (slab_dealloc(slab, node) == -1) {
        spin_unlock(&anchor->value);
        return;
    }
    struct dll *prev = node->rev;
    struct dll *next = node->fwd;
    prev->fwd = next;
    next->rev = prev;
    spin_unlock(&anchor->value);
}

void dll_find_and_delete(struct dll *anchor, int value, struct slab *slab) {
    spin_lock(&anchor->value);
    struct dll *it = anchor->fwd;
    while (it->value < value && it->fwd != anchor) {
        it = it->fwd;
    }

    if (it->value == value && it->fwd != anchor) {
        struct dll *prev = it->rev;
        struct dll *next = it->fwd;
        prev->fwd = next;
        next->rev = prev;
        slab_dealloc(slab, it);
    }
    spin_unlock(&anchor->value);
}

struct dll *dll_find(struct dll *anchor, int value) {
    spin_lock(&anchor->value);
    struct dll *it = anchor->fwd;
    while (it->value < value && it->fwd != anchor) {
        it = it->fwd;
    }

    if (it->value == value && it->fwd != anchor) {
        spin_unlock(&anchor->value);
        return it;
    }
    spin_unlock(&anchor->value);
    return NULL;
}

void seq_dll_find_and_delete(struct dll *anchor, int value, struct seq_slab *slab) {
    int seq;
    bool retryFlag = false;
    do {
        seq = read_seqbegin(slab);
        if (retryFlag) {
            spin_lock(&m->lock);
            m->data = m->data + 1;
            spin_unlock(&m->lock);
        }
        struct dll *it = anchor->fwd;
        while (it->value < value && it->fwd != anchor) {
            it = it->fwd;
        }

        if (it->value == value && it->fwd != anchor) {
            write_seqlock(slab);
            struct dll *prev = it->rev;
            struct dll *next = it->fwd;
            prev->fwd = next;
            next->rev = prev;
            seq_slab_dealloc(slab, it);
            write_sequnlock(slab);
            return;
        }
        retryFlag = true;
    } while (read_seqretry(slab, seq));
}

struct dll *seq_dll_insert(struct dll *anchor, int value, struct seq_slab *slab) {
    if (anchor == NULL || slab == NULL) return NULL;
    write_seqlock(slab);
    struct dll *new = seq_slab_alloc(slab);
    if (new == NULL) {
        write_sequnlock(slab);
        return NULL;
    }
    new->value = value;
    struct dll *it = anchor;
    while (it->fwd->value < value && it->fwd != anchor) {
        it = it->fwd;
    }
    struct dll *next = it->fwd;
    it->fwd = new;
    new->rev = it;
    new->fwd = next;
    next->rev = new;
    write_sequnlock(slab);
    return new;
}

void seq_dll_delete(struct dll *anchor, struct dll *node, struct seq_slab *slab) {
    write_seqlock(slab);
    if (seq_slab_dealloc(slab, node) == -1) {
        write_sequnlock(slab);
        return;
    }
    struct dll *prev = node->rev;
    struct dll *next = node->fwd;
    prev->fwd = next;
    next->rev = prev;
    write_sequnlock(slab);
}

struct dll *seq_dll_find(struct dll *anchor, int value, struct seq_slab *slab) {
    int seq;
    bool retryFlag = false;
    do {
        seq = read_seqbegin(slab);
        if (retryFlag) {
            spin_lock(&m->lock);
            m->data = m->data + 1;
            spin_unlock(&m->lock);
        }
        struct dll *it = anchor->fwd;
        while (it->value < value && it->fwd != anchor) {
            it = it->fwd;
        }

        if (it->value == value && it->fwd != anchor) {
            return it;
        }
        retryFlag = true;
    } while (read_seqretry(slab, seq));
    return NULL;
}

void write_seqlock(struct seq_slab *s) {
    spin_lock(&s->dll_spinlock);
    spin_lock(&s->countlock);
    s->count++;
    spin_unlock(&s->countlock);
//    __sync_fetch_and_add(&s->count, 1);
}

void write_sequnlock(struct seq_slab *s) {
//    __sync_fetch_and_add(&s->count, 1);
    spin_lock(&s->countlock);
    s->count++;
    spin_unlock(&s->countlock);
    spin_unlock(&s->dll_spinlock);
}

int read_seqbegin(struct seq_slab *s) {
    int ret;
    while ((ret = s->count) % 2)
        sched_yield();
    return ret;
}

int read_seqretry(struct seq_slab *s, int orig) {
    return s->count != orig;
}

void get_time(struct timeval *time) {
    if (gettimeofday(time, NULL) < 0) {
        fprintf(stderr, "Can't retrieve current time: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void print_info() {
    fprintf(stdout, "The size of slots: %d\n", NSLOTS);
    fprintf(stdout, "The number of processes: %d\n", NPROCS);
    fprintf(stdout, "The number of iteration for each process: %d\n", NITERS);
    fprintf(stdout, "The range of the value inserted to dll: 0~%d\n", NRANGE-1);
}

void print_time_info(struct timeval *result) {
    fprintf(stdout, "Real Time Elapsed: %ld.%06lds\n", result->tv_sec, result->tv_usec);
}