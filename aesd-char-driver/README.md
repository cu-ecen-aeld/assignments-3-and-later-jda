# AESD Char Driver

Template source code for the AESD char driver used with assignments 8 and later

## Instructions
_from https://www.coursera.org/learn/linux-kernel-programming-yocto-project/supplement/Zk1gc/assignment-8-instructions_

. Add source code, makefile and init scripts needed to install a /dev/aesdchar device.  Ultimately this device should be installed in your qemu instance, however you may also wish to test on your host virtual machine during development.  If you are testing on your virtual machine, please make sure you have a snapshot taken before starting work on the assignment so you can recover in case you corrupt your virtual machine disk.  Your /dev/aesdchar device should use the circular buffer implementation you developed in assignment 7 plus new code you add to your driver to:

        a. Allocate memory for each write command as it is received, supporting any length of write request (up to the length of memory which can be allocated through kmalloc), and saving the write command content within allocated memory.  

                i. The write command will be terminated with a \n character as was done with the aesdsocket application.

                         1. Write operations which do not include a \n character should be saved and appended by future write operations and should not be written to the circular buffer until terminated.

                ii. The content for the most recent 10 write commands should be saved.  

                iii. Memory associated with write commands more than 10 writes ago should be freed.

                iv. Write position and write file offsets can be ignored on this assignment, each write will just write to the most recent entry in the history buffer or append to any unterminated command.

                v. For the purpose of this assignment you can use kmalloc for all allocations regardless of size, and assume writes will be small enough to work with kmalloc.

        b. Return the content (or partial content) related to the most recent 10 write commands, in the order they were received, on any read attempt.

                1. You should use the position specified in the read to determine the location and number of bytes to return.

                2. You should honor the count argument by sending only up to the first “count” bytes back of the available bytes remaining.

        c. Perform appropriate locking to ensure safe multi-thread and multi-process access and ensure a full write file operation from a thread completes before accepting a new write file operation.

        d. Your implementation should print the expected contents when running the drivertest.sh script at 
https://github.com/cu-ecen-aeld/assignment-autotest/blob/master/test/assignment8/drivertest.sh
.

5. Modify your socket server application developed in assignments 5 and 6 to support and use a build switch USE_AESD_CHAR_DEVICE, set to 1 by default, which:

        a. Redirects reads and writes to /dev/aesdchar instead of /var/tmp/aesdsocketdata

        b. Removes timestamp printing.

        c. Ensure you do not remove the  /dev/aesdchar endpoint after exiting the aesdsocket application.