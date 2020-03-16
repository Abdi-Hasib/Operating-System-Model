# CITS2002
# Project 1 Description And Task

# Processes
Each process executes, in turn, until it:
*   completes its execution (at which time the process Exits),
*   executes for a finite time-quantum (after which the process is marked as Ready and is queued until it can again Run), or
*   requests some input or output (I/O) (at which time the process is marked as Blocked and queued until its I/O request is satisfied).
*   only a single process occupies the single CPU at any one time.
*   CPU has a clock speed of 1GHz, enabling it to execute one-billion instructions per second.
*   It takes 5 microseconds to perform a context-switch; to move one process from Ready → Running.
*   No time is consumed deciding that the currently Running process can remaining Running (and start a new time-quantum).
*   All other state transitions occur in zero time (unrealistic, but keeping the project simple).

# I/O
*   CPU is connected to a number of input/output (I/O) devices of differing speeds, using a single high-speed data-bus.
*   Only a single process can use the data-bus at any one time, and it takes 5 microseconds for any process to first acquire the data-bus.
*   Only a single process can access each I/O device (and the data-bus) at any one time.
*   If the data-bus is in use (data is still being transferred) and a second process also needs to access the data-bus, the second process must be queued until the current transfer is complete.
*   When a data transfer completes, all waiting (queued) processes are consider to determine which process can next acquire the data-bus.
*   If multiple processes are waiting to acquire the data-bus, the process that has been waiting the longest for the device with the highest priority will next acquire the data-bus.
*   All processes waiting on higher priority devices are serviced before any processes that are waiting on lower priority devices.

# Tracefiles
*   We may assume that the format of each tracefile is correct, and its data consistent, so we do not need to check for errors in the tracefile.
*   Each line provides a device definition including its name and its data transfer rate (all transfer rates are measured in bytes/second).
*   Devices with higher transfer rates have a higher priority and are serviced first.
*   The final line indicates the end of all device definitions, and that the operating system commences execution setting the system-time to zero microseconds.
*   All times in the tracefile are measured in microseconds.

# Task
*   besttq.c should accept command-line arguments providing the name of the tracefile and three integers representing times measured in microseconds.
*    The program is only required to produce a single line of output reporting the best time-quantum found.
