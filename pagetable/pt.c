#include "os.h"
#include <stdio.h>
/* OS.H GIVES:
uint64_t alloc_page_frame(void);
void* phys_to_virt(uint64_t phys_addr); */

#define NUM_OF_NODES 1024
#define NUM_OF_LEVELS 5
#define MASK 0b1111111111
#define OFFSET 13

void page_table_update(uint64_t pt, uint64_t vpn, uint64_t ppn);
uint64_t page_table_query(uint64_t pt, uint64_t vpn);

void page_table_update(uint64_t pt, uint64_t vpn, uint64_t ppn)
{
    uint64_t* table = phys_to_virt(pt << OFFSET); // offset 0 to start of the page
    uint64_t hold_vpn_temp, shift, entry, next_pt;

    for (int level = 0; level < NUM_OF_LEVELS; level++) {
        shift = (NUM_OF_LEVELS - 1 - level) * 10;
        hold_vpn_temp = (vpn >> shift) & MASK;
        if (level == NUM_OF_LEVELS - 1) { // Handle last level
            if (ppn == NO_MAPPING) {
                // printf("unmapping chill..");
                table[hold_vpn_temp] &= ~1U; // Clear valid bit which means unmap
            } else { // Set mapping
                // printf("table[%lu]: 0x%lx\n", hold_vpn_temp, table[hold_vpn_temp]);
                // printf("great success: 0x%lx\n", table[hold_vpn_temp]);
                table[hold_vpn_temp] = ((ppn << OFFSET) | 1);
            }
            return;
        }
        entry = table[hold_vpn_temp];
        if ((entry & 1)==0) {
            if (ppn == NO_MAPPING) {
                // No mapping exists to unmap
                return;
            }
            // Allocate new page table
            next_pt = alloc_page_frame();
            table[hold_vpn_temp] = (next_pt << OFFSET) | 1;
        } else {
            next_pt = entry >> OFFSET;
        }
        table = phys_to_virt(next_pt << OFFSET);
        // printf("Current table address: %p\n", (void*)table);
    }
}

uint64_t page_table_query(uint64_t pt, uint64_t vpn)
{
    uint64_t* table_vpn = phys_to_virt(pt << OFFSET); // offset 0 to start of the page
    uint64_t hold_vpn_temp; int shift; uint64_t to_return;

    for (int level = 0; level < NUM_OF_LEVELS; level++){
        shift = (NUM_OF_LEVELS - 1 - level) * 10;
        // printf("shift: %d\n", shift);
        // printf("hold_vpn_temp: 0x%lx\n", (vpn >> shift));
        // printf("hold_vpn_temp masked: 0x%lx\n", (vpn >> shift) & MASK);
        hold_vpn_temp = (vpn >> shift) & MASK; // take current relevant 10 bits of vpn
        to_return = table_vpn[hold_vpn_temp]; // TODO gets stuck here
        // printf("to_return: 0x%li\n", to_return);
        if ((to_return & 1)==0) // check valid bit = 0, if so there's no mapping
        {
            // printf("no mappi\n");
            return NO_MAPPING;
        }
        else{
            if (level != NUM_OF_LEVELS - 1){ // if not the last level, get deeper to trie
                table_vpn = phys_to_virt((to_return >> OFFSET) << OFFSET);
            }
        }
    }
    // printf("good to_return: 0x%li\n", to_return);
    return to_return >> OFFSET; // last cell holds the actual tranlation
}