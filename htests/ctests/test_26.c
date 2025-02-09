#include "tester.h"

// ====================================================================
// TEST_26
// Summary: MAP PLACEMENT: Check wmap, wunmap and then wmap again works
// Place 3 maps, delete map1 and map3, place 2 more maps 
// ====================================================================

char *test_name = "TEST_26";

int main() {
    printf(1, "\n\n%s\n", test_name);
    validate_initial_state();

    uint addrs[] = {MMAPBASE, MMAPBASE+3*PGSIZE, MMAPBASE+10*PGSIZE, MMAPBASE, MMAPBASE+12*PGSIZE};
    uint lengths[] = {3*PGSIZE, 7*PGSIZE, 5*PGSIZE, 2*PGSIZE, 2*PGSIZE};
    int anon = MAP_FIXED | MAP_ANONYMOUS | MAP_SHARED;


    // place first 3 maps
    for (int idx = 0; idx < 3; idx++) {
        uint addr = addrs[idx];
        uint length = lengths[idx];
        uint map = wmap(addr, length, anon, 0);
        if (map != addr) {
            printerr("wmap() returned %d\n", (int)map);
            failed();
        }
        // validate mid state
        struct wmapinfo winfo;
        get_n_validate_wmap_info(&winfo, idx+1); // idx+1 maps exist
        for (int j=0; j<=idx; j++) {
            map_exists(&winfo, addrs[j], lengths[j], TRUE); // all previous maps exist
        }
        printf(1, "INFO: Map %d at 0x%x with length %d. \tOkay.\n", idx+1, map, length);
    }

    // delete map 1
    if (wunmap(addrs[0]) != 0) {
        printerr("munmap() failed\n");
        failed();
    }
    // validate mid state
    struct wmapinfo winfo2;
    get_n_validate_wmap_info(&winfo2, 2); // 2 maps exist
    map_exists(&winfo2, addrs[0], lengths[0], FALSE); // map 1 does not exist
    map_exists(&winfo2, addrs[1], lengths[1], TRUE);  // map 2 exists
    map_exists(&winfo2, addrs[2], lengths[2], TRUE);  // map 3 exists
    printf(1, "INFO: Map 1 deleted. \tOkay.\n");

    // delete map 3
    if (wunmap(addrs[2]) != 0) {
        printerr("munmap() failed\n");
        failed();
    }
    // validate mid state
    struct wmapinfo winfo3;
    get_n_validate_wmap_info(&winfo3, 1); // 1 map exists
    map_exists(&winfo3, addrs[0], lengths[0], FALSE); // map 1 does not exist
    map_exists(&winfo3, addrs[1], lengths[1], TRUE);  // map 2 exists
    map_exists(&winfo3, addrs[2], lengths[2], FALSE); // map 3 does not exist
    printf(1, "INFO: Map 3 deleted. \tOkay.\n");

    // place 2 more maps
    for (int idx=3; idx<5; idx++) {
        uint addr = addrs[idx];
        uint length = lengths[idx];
        uint map = wmap(addr, length, anon, 0);
        if (map != addr) {
            printerr("wmap() returned %d\n", (int)map);
            failed();
        }
        get_n_validate_wmap_info(&winfo3, idx-2+1); // idx+1 maps exist
        map_exists(&winfo3, addrs[0], lengths[0], FALSE); // map 1 does not exist
        map_exists(&winfo3, addrs[1], lengths[1], TRUE);  // map 2 exists
        map_exists(&winfo3, addrs[2], lengths[2], FALSE); // map 3 does not exist
        for (int j=3; j<=idx; j++) {
            map_exists(&winfo3, addrs[j], lengths[j], TRUE); 
        }
        printf(1, "INFO: Map %d at 0x%x with length %d. \tOkay.\n", idx+1, map, length);
    }

    // unmap all maps
    for (int idx=0; idx<5; idx++) {
        if (idx == 0 || idx == 2) {
            continue;
        }
        if (wunmap(addrs[idx]) != 0) {
            printerr("munmap() failed\n");
            failed();
        }
    }
    // validate final state
    struct wmapinfo winfo4;
    get_n_validate_wmap_info(&winfo4, 0); // no maps exist
    printf(1, "INFO: All maps deleted. \tOkay.\n");

    // test ends
    success();
}
