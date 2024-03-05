/* KallistiOS ##version##

   devroot.c
   Copyright (C) 2024 Donald Haase
   
   This example demonstrates the expected behavior of reading from the 
   / and /dev dirs, as well as attempting to open a non-existant device 
   for the same.
*/

#include <kos.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <dirent.h>

KOS_INIT_FLAGS(INIT_DEFAULT);

void printdir(char* fn) {   
    DIR *d;
    struct dirent *entry;

    if(!(d = opendir(fn))) {
        printf("Could not open %s: %s\n", fn, strerror(errno));
    }
    else {
        printf("Opened %s and found these: \n", fn);
        while((entry = readdir(d))) {
            printf("    %s\n", entry->d_name);
        }        
    }
}

int main(int argc, char **argv) {
  
    /* Open the root of dev's filesystem and list the contents. */
    /* This should *not* show /dev subdirs */
    printdir("/");
   
    /* Now do the same with /dev. This should list out the devices under it. */
    printdir("/dev");
    
    /* Now try the same with a fake dev. It should fail */
    printdir("/dev/quzar");    

   return 0;
}
