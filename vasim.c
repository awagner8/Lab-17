#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define VM_SIZE (1 << 24)
#define PAGE_SIZE 4096
#define VM_PAGES (VM_SIZE/PAGE_SIZE)
#define MAX_PHYS_PAGES 64

typedef struct {
    unsigned int phys_page;   // the physical page
    char valid;
} pte_t;

int verbose = 0;

void print_replacement(unsigned int vpn, unsigned int ppn) {
    printf("Replacing vpn 0x%d, ppn 0x%x\n",vpn,ppn);
}

unsigned int get_vpn(unsigned int address) {
   return address / PAGE_SIZE;
}

void allocate_phys_page(pte_t *pagetable, unsigned int vpn) {
    static unsigned int next_phys_page = 0; // Track the next available physical page number
    static unsigned int allocation_order[MAX_PHYS_PAGES]; // FIFO queue for allocated pages
    static int fifo_index = 0; // Index for the FIFO replacement

    if (next_phys_page < MAX_PHYS_PAGES) {
        // Initial allocation
        pagetable[vpn].phys_page = next_phys_page;
        allocation_order[next_phys_page] = vpn; // Keep track of the allocation order
        next_phys_page++;
    } else {
        // FIFO replacement
        unsigned int old_vpn = allocation_order[fifo_index];
        unsigned int old_ppn = pagetable[old_vpn].phys_page;
        pagetable[old_vpn].valid = 0; // Invalidate the old mapping
        pagetable[vpn].phys_page = old_ppn; // Reuse the physical page for the new VPN
        allocation_order[fifo_index] = vpn; // Update the allocation order
        fifo_index = (fifo_index + 1) % MAX_PHYS_PAGES; // Move to the next index in the FIFO queue
        if (verbose) {
            print_replacement(old_vpn, old_ppn);
        }
    }
    pagetable[vpn].valid = 1; // Mark the new mapping as valid
}


void usage() {
    printf("Usage: vassim -n <number> -v [-f <name>] \n");
    printf("  -n <number> Use only the first n addresses.\n");
    printf("  -f <name> Trace file name (requried)\n");
    printf("  -v verbose mode, print information about hits, faults, replacements\n");
}

// main program for simple virtual memory simulation

int main(int argc, char **argv) {
    char *tracefile = NULL;
    FILE *tracefd = NULL;
    char c;
    unsigned int address = 0;
    unsigned int hits = 0, faults = 0, total_accesses = 0, access_limit = 0;
    pte_t page_table[VM_PAGES];  // the page table, one entry per virtual page

    // get input arguments
    while ((c = getopt(argc, argv, "vn:f:")) != -1) {
        if (c == 'f') {
            tracefile = strdup(optarg);
        } else if (c == 'n') {
            access_limit = atoi(optarg);
            printf("Running for %d accesses\n",access_limit);
        }  else if (c == 'v') {
            verbose = 1;
        } else {       
            usage();
            exit(1);
        }
    }

    if(!tracefile) {
        usage();
        exit(1);
    }

    tracefd = fopen(tracefile,"r");

    if (tracefd == NULL) {
        perror("Error opening trace file\n");
        exit(1);
    }

    // initialize page table to invalid
    for (int i = 0; i < VM_SIZE/PAGE_SIZE; i++) {
        page_table[i].valid = 0;
    }
 
    // read the address form each line in trace file
    while (fscanf(tracefd, "0x%x\n", &address) != EOF) { 

        unsigned int vpn = get_vpn(address);
        if (verbose)
            printf("0x%x vpn: 0x%x\n",address,vpn);
        if(!page_table[vpn].valid) {  
            faults++;
            allocate_phys_page(page_table,vpn);
            if (verbose)
                printf("Fault: 0x%x -> 0x%x\n",vpn,page_table[vpn].phys_page);
        } else {
            hits++;
            if (verbose)
                printf("Hit: 0x%x -> 0x%x\n",vpn,page_table[vpn].phys_page);
        }

        total_accesses++;
        if (access_limit && (total_accesses >= access_limit)) {
                break;
        }

    }
    printf("\nAccesses %d: Hits: %d Faults: %d\n",total_accesses,hits,faults);
    return 0;
}