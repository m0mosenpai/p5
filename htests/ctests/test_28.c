#include "tester.h"

// ====================================================================
// TEST_28
// Summary: COW: 2 layers of forking: allocated array has diff pa in parent and child after modification
// ====================================================================

char *test_name = "TEST_28";

int sum(char *arr, int n) {
    int s = 0;
    for (int i = 0; i < n; i++) {
        s += arr[i];
    }
    return s;
}

int verify_unmod_childarr(char* arr1, int n1, int parent_pa1, int parent_pid) {
    int child_pa1 = va2pa((uint)arr1);
    if (child_pa1 == FAILED) {
        printerr("child: arr: va2pa(0x%x) failed\n", arr1);
        failed();
    }
    printf(1, "INFO: Child: arr pa = 0x%x\n", child_pa1);
    if (parent_pa1 != child_pa1) {
        printerr("Parent %d and child %d have diff phyaddr for arr\n", parent_pid, getpid());
        failed();
    }
    return child_pa1;
}

int verify_mod_childarr(char* arr1, int n1, int parent_pa1, int parent_pid) {
    int child_newpa1 = va2pa((uint)arr1);
    if (child_newpa1 == FAILED) {
        printerr("child: newarr: va2pa(0x%x) failed\n", arr1);
        failed();
    }
    printf(1, "INFO: Child: arr new_pa = 0x%x\n", child_newpa1);
    if (parent_pa1 == child_newpa1) {
        printerr("Parent %d and child %d have same phyaddr for arr after modification\n", parent_pid, getpid());
        failed();
    }
    return child_newpa1;
}

int main(int argc, char *argv[]) {
    printf(1, "\n\n%s\n", test_name);

    // create an array of 5 pages in parent
    int n1 = 5 * PGSIZE;
    int n2 = 50 * PGSIZE;
    char *arr1 = malloc(n1);
    char *arr2 = malloc(n2);
    for (int i = 0; i < n1; i++) {
        arr1[i] = i % 100;
    }
    for (int i = 0; i < n2; i++) {
        arr2[i] = (i+5) % 120;
    }
    int parent_sum1 = sum(arr1, n1);
    int parent_sum2 = sum(arr2, n2);
    printf(1, "Parent: sum1 = %d, sum2 = %d\n", parent_sum1, parent_sum2);

    int parent_pa1 = va2pa((uint)arr1);
    if (parent_pa1 == FAILED) {
        printerr("arr1: va2pa(0x%x) failed\n", arr1);
        failed();
    }
    printf(1, "INFO: Parent: arr1 pa = 0x%x\n", parent_pa1);
    int parent_pa2 = va2pa((uint)arr2);
    if (parent_pa2 == FAILED) {
        printerr("arr2: va2pa(0x%x) failed\n", arr2);
        failed();
    }
    printf(1, "INFO: Parent: arr2 pa = 0x%x\n", parent_pa2);
    int parent_pid = getpid();


    int n_child = 3;
    for (int i = 0; i < n_child; i++) {
        int pid = fork();
        if (pid < 0) {
            printerr("fork() failed\n");
            failed();
        } else if (pid == 0) {
            // 
            // Child process
            //
            int child_sum1 = sum(arr1, n1);
            if (parent_sum1 != child_sum1) {
                printerr("Parent sum %d != child sum %d\n", parent_sum1, child_sum1);
                failed();
            }
            printf(1, "Child: sum1 = %d\n", sum(arr1, n1));
            int child_sum2 = sum(arr2, n2);
            if (parent_sum2 != child_sum2) {
                printerr("Parent sum %d != child sum %d\n", parent_sum2, child_sum2);
                failed();
            }
            printf(1, "Child: sum2 = %d\n", sum(arr2, n2));
            int child_pa1 = verify_unmod_childarr(arr1, n1, parent_pa1, parent_pid);
            int child_pa2 = verify_unmod_childarr(arr2, n2, parent_pa2, parent_pid);
            
            // modify arr1
            for (int i = 0; i < n1; i++) {
                arr1[i] = i % 50;
            }
            child_sum1 = sum(arr1, n1);
            printf(1, "Child: modified sum1 = %d\n", child_sum1);
            child_pa1 = verify_mod_childarr(arr1, n1, parent_pa1, parent_pid);

            // fork again
            int parent_sum1 = child_sum1;
            int parent_pa1 = child_pa1;
            int parent_sum2 = child_sum2;
            int parent_pa2 = child_pa2;
            int pid2 = fork();
            if (pid2 < 0) {
                printerr("inner fork() failed\n");
            } else if (pid2 == 0) {
                // 
                // Child process
                //
                int child_sum1 = sum(arr1, n1);
                if (parent_sum1 != child_sum1) {
                    printerr("Parent sum %d != Innchild sum %d\n", parent_sum1, child_sum1);
                    failed();
                }
                printf(1, "InnChild: sum1 = %d\n", sum(arr1, n1));
                int child_sum2 = sum(arr2, n2);
                if (parent_sum2 != child_sum2) {
                    printerr("Parent sum %d != Innchild sum %d\n", parent_sum2, child_sum2);
                    failed();
                }
                printf(1, "InnChild: sum2 = %d\n", sum(arr2, n2));
                verify_unmod_childarr(arr1, n1, parent_pa1, parent_pid);
                verify_unmod_childarr(arr2, n2, parent_pa2, parent_pid);
                // modify arr2
                for (int i = 0; i < n2; i++) {
                    arr2[i] = (i+5) % 12;
                }
                printf(1, "InnChild: modified sum2 = %d\n", sum(arr2, n2));
                verify_mod_childarr(arr2, n2, parent_pa2, parent_pid);
                exit();
            }
            wait();
            exit();
        }
    }
    // wait for all child processes to finish
    while (wait() >= 0)
        ;

    // verify that parent arr1 is intacts
    int new_parent_pa = va2pa((uint)arr1);
    if (new_parent_pa == FAILED) {
        printerr("va2pa(0x%x) failed after forking\n", arr1);
        failed();
    }
    if (parent_pa1 != new_parent_pa) {
        printerr("Parent has different pa after forking\n");
        failed();
    }
    if (sum(arr1, n1) != parent_sum1) {
        printerr("Parent sum changed after forking\n");
        failed();
    }
    success();
}