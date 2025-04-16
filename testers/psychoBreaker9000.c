#include "h/localLibumps.h"
#include "h/tconst.h"
#include "h/print.h"

#define NUM_PAGES 28
#define START_PAGE 2
#define ITERATIONS 5

void main() {
    int i, j, idx, corrupt = 0;
    char *pagePtr;
    char inputBuf[20];

    print(WRITETERMINAL, "=== psychSwapstorm.c initiating ===\n");

    for (i = 0; i < ITERATIONS; i++) {
        print(WRITETERMINAL, "Swap iteration cycle: ");
        printInt(WRITETERMINAL, i);
        print(WRITETERMINAL, "\n");

        for (j = START_PAGE; j < START_PAGE + NUM_PAGES; j++) {
            pagePtr = (char *)(SEG2 + j * PAGESIZE);
            pagePtr[(i * 17) % PAGESIZE] = j + i; /* random offset write */
        }

        print(WRITETERMINAL, "Random writes to pages done.\n");

        /* Trigger user I/O interleaved with memory stress */
        print(WRITETERMINAL, "Type a short string (will be echoed back): ");
        SYSCALL(READTERMINAL, (int)&inputBuf[0], 0, 0);
        inputBuf[18] = EOS; /* EOS somewhere */
        print(WRITETERMINAL, "\nEchoing: ");
        print(WRITETERMINAL, &inputBuf[0]);
        print(WRITETERMINAL, "\n");
    }

    print(WRITETERMINAL, "Verifying data integrity...\n");

    for (j = START_PAGE; j < START_PAGE + NUM_PAGES; j++) {
        pagePtr = (char *)(SEG2 + j * PAGESIZE);
        if (pagePtr[(ITERATIONS - 1) * 17 % PAGESIZE] != j + ITERATIONS - 1) {
            print(WRITETERMINAL, "Data corrupted at page: ");
            printInt(WRITETERMINAL, j);
            print(WRITETERMINAL, "\n");
            corrupt = 1;
        }
    }

    if (!corrupt)
        print(WRITETERMINAL, "All page data intact after stress test!\n");
    else
        print(WRITETERMINAL, "Some data was corrupted! Pager fault?\n");

    print(WRITETERMINAL, "Final phase: testing privilege enforcement...\n");
    *((char *)(0x20000000)) = 42;

    print(WRITETERMINAL, "ERROR: was able to write to ksegOS\n");

    SYSCALL(TERMINATE, 0, 0, 0);
}
