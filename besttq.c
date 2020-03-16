#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

/* CITS2002 Project 1 2019
 Names:             ABDIHASIB ISSE, ABDIHAFITH ISSE
 Student numbers:   22850086, 22751102
 */

#define MAX_DEVICES             4
#define MAX_DEVICE_NAME         20
#define MAX_PROCESSES           50
#define MAX_EVENTS_PER_PROCESS    100
#define TIME_CONTEXT_SWITCH     5
#define TIME_ACQUIRE_BUS        5

int optimal_time_quantum                    = 0;
int total_process_completion_time           = 0;
// TOTAL NUMBER OF DEVICES
int nDevice                                 = 0;
// TOTAL NUMBER OF PROCESSES
int nProcess                                = 0;
int time_sinceReboot                        = 0;
int device_speed[MAX_DEVICES];
int process_commenceTime[MAX_PROCESSES];
int process_exitTime[MAX_PROCESSES];
// Operations per process
int op_perProcess[MAX_PROCESSES];
int op_commenceTime[MAX_PROCESSES][MAX_EVENTS_PER_PROCESS];
// AMOUNT OF DATA TO BE TRANSFERRED BY EACH OPERATION
int op_transferSize[MAX_PROCESSES][MAX_EVENTS_PER_PROCESS];
int op_Speed[MAX_PROCESSES][MAX_EVENTS_PER_PROCESS];
// OPERATIONS WITH A HIGHER SPEED WILL HAVE A HIGHER PRIORITY
// THIS IS USED TO DETERMINE WHAT QUEUE THE PROCESS WILL BE
// PLACED IN ONCE BLOCKED
int op_priority[MAX_PROCESSES][MAX_EVENTS_PER_PROCESS];
int process_runtime[MAX_PROCESSES];
int priority;
int op_index;
// TRACKS THE NUMBER OF PROCESSES THAT HAVE EXITED
int pExit;
// KEEPS A TRACK OF THE CURRENT TRANSFERRING PROCESS
int blockedProcess;
// HOLDS THE INDEX OF THE CURRENTLY RUNNING PROCESS
int processNumber;
// COUNTS THE NUMBER OF PROCESSES THAT HAVE COMMENCED
int pCount;
int queueCount;
// TIMER THAT TRACKS THE READY->RUNNING DELAY
int state_change;
// TIMER USED TO CHECK WHEN BLOCKED PROCESSES CAN ACQUIRE DATABUS
int state_changeIO;
// SECOND TIMER TO CHECK WHEN UNBLOCKED PROCESS CAN GO FROM READY->RUNNING
double state_changeIOexit;
double transferTime;
bool data_busFree;
bool running;
bool transferring;
bool blocked;
// NUMBER OF BLOCED PROCESS IN EACH DEVICE QUEUE, BASED ON PRIORITY
int blocked_queueCount[MAX_DEVICES];
int blocked_queues[MAX_DEVICES][MAX_PROCESSES];
// THE READY QUEUE
int queue[MAX_PROCESSES];
int timeremaining[MAX_PROCESSES];
int timeElapsed[MAX_PROCESSES];
// KEEPS TRACK OF WHAT OPERATOIN (I/O EVENT) THE CURRENT PROCESS IS UP TO
int operation[MAX_PROCESSES];
char process_name[MAX_PROCESSES][MAX_DEVICE_NAME];
char device[MAX_DEVICES][MAX_DEVICE_NAME];


//  ----------------------------------------------------------------------

#define CHAR_COMMENT            '#'
#define MAXWORD                 20

void parse_tracefile(char program[], char tracefile[])
{
    int nOp = 0;
    //  ATTEMPT TO OPEN OUR TRACEFILE, REPORTING AN ERROR IF WE CAN'T
    FILE *fp    = fopen(tracefile, "r");
    
    if(fp == NULL) {
        printf("%s: unable to open '%s'\n", program, tracefile);
        exit(EXIT_FAILURE);
    }
    
    char line[BUFSIZ];
    int  lc     = 0;
    
    //  READ EACH LINE FROM THE TRACEFILE, UNTIL WE REACH THE END-OF-FILE
    while(fgets(line, sizeof line, fp) != NULL) {
        ++lc;
        
        //  COMMENT LINES ARE SIMPLY SKIPPED
        if(line[0] == CHAR_COMMENT) {
            continue;
        }
        
        //  ATTEMPT TO BREAK EACH LINE INTO A NUMBER OF WORDS, USING sscanf()
        char    word0[MAXWORD], word1[MAXWORD], word2[MAXWORD], word3[MAXWORD];
        int nwords = sscanf(line, "%s %s %s %s", word0, word1, word2, word3);
        
        //  WE WILL SIMPLY IGNORE ANY LINE WITHOUT ANY WORDS
        if(nwords <= 0) {
            continue;
        }
        //  LOOK FOR LINES DEFINING DEVICES, PROCESSES, AND PROCESS EVENTS
        if(nwords == 4 && strcmp(word0, "device") == 0) {
            strcpy(device[nDevice], word1);                 // STORE THE DEVICE NAME
            device_speed[nDevice] = atoi(word2);            // STORE THE DEVICE SPEED
            ++nDevice;                                      // INCREMENT DEVICE COUNTER
        }
        
        else if(nwords == 1 && strcmp(word0, "reboot") == 0) {
            continue;   // NOTHING REALLY REQUIRED, DEVICE DEFINITIONS HAVE FINISHED
        }
        
        // STORE THE NAME, COMMENCENT TIME OF EACH PROCESS
        else if(nwords == 4 && strcmp(word0, "process") == 0) {
            nOp = 0;
            strcpy(process_name[nProcess], word1);
            process_commenceTime[nProcess] = atoi(word2);
            ++nProcess;
            op_perProcess[nProcess-1] = 0;      // INITIALISE THE OPERATIONS PER PROCESS TO 0, FOR EACH PROCESS
        }
        // STORE THE TOTAL OPERATIONS PER PROCESS, THEIR COMMENCEMENT TIME AND THEIR TRANSFER SIZE
        else if(nwords == 4 && strcmp(word0, "i/o") == 0) {
            op_perProcess[nProcess-1] += 1;
            op_commenceTime[nProcess-1][nOp] = atoi(word1);
            op_transferSize[nProcess-1][nOp] = atoi(word3);
            for(int i=0 ; i<nDevice ; ++i)
            {
                // CHECK WHAT DEVICE THE OPERATION USES
                if(strcmp(word2,device[i]) == 0)
                {
                    op_Speed[nProcess-1][nOp] = device_speed[i];        // STORE THE SPEED OF THE OPERATION
                }
            }
            
            ++nOp;
        }
        
        else if(nwords == 2 && strcmp(word0, "exit") == 0) {
            process_exitTime[nProcess-1] = atoi(word1);   //  PRESUMABLY THE LAST EVENT WE'LL SEE FOR THE CURRENT PROCESS
        }
        
        else if(nwords == 1 && strcmp(word0, "}") == 0) {
            continue;   //  JUST THE END OF THE CURRENT PROCESS'S EVENTS
        }
        else {
            printf("%s: line %i of '%s' is unrecognized",
                   program, lc, tracefile);
            exit(EXIT_FAILURE);
        }
    }
    fclose(fp);
}

#undef  MAXWORD
#undef  CHAR_COMMENT

//  ----------------------------------------------------------------------

// SHORT FUNCTION TO INITIALISE ALL GLOBAL VARIABLES. PARTICULARLY USEFUL WHEN
// RUNNING SIMULATION MULTIPLE TIMES

void initialise_process(void)
{
    processNumber        = 0;
    pCount               = 0;
    queueCount           = 0;
    time_sinceReboot     = 0;
    state_change         = -1;
    state_changeIO       = -1;
    running             = false;
    transferring        = false;
    blocked             = false;
    transferTime      = -1;
    priority             = 0;
    op_index             = 0;
    pExit                = 0;
    blockedProcess       = 0;
    state_changeIOexit = -1;
    data_busFree        = true;
    
    for(int i=0 ; i<nDevice ; ++i)
    {
        blocked_queueCount[i] = 0;
        for(int k=0 ; k<nProcess ; ++k)
        {
            blocked_queues[i][k] = -1;      // INITIALISE EVERY FIELD IN THE QUEUE TO -1, USEFUL FOR FLOW CONTROL IN LOOP
        }
        
    }
    // INITIALISING ALL PROCESS DETAILS
    for(int i=0 ; i<nProcess ; ++i)
    {
        timeremaining[i] = process_exitTime[i];
        queue[i] = -1;
        timeElapsed[i] = 0;
        process_runtime[i] = 0;
        if(op_perProcess[i] < 1)
        {
            op_perProcess[i] = 0;       // IF THE PROCESS HAS NO I/O EVENTS, MANUALLY SET IT TO 0 FOR SAFETY
        }
        operation[i] = 0;               // I/O COUNTER
        
    }
}

// ------------------------------------------------------------------------------------------------------------------------

// SHORT FUNCTION TO RE-ARRANGE THE DEVICE ARRAY TO SORT BY SPEED
// USEFUL FOR CREATING THE RIGHT DEVICE QUEUES
void sort_device_speed(void)
{
    int maxSpeed = 0, element = 0;
    int temp_deviceSpeed[MAX_DEVICES];
    char temp_deviceName[MAX_DEVICES][MAX_DEVICE_NAME];  // TEMPORARY VARIABLES TO STORE SPEED AND NAME
    if(nDevice > 0)
    {
        int j = 0;
        while(maxSpeed != -1)
        {
            // SET MAXSPEED = TO FIRST ELEMENT AND LOOP THROUGH EACH INDEX COMPARING THEIR VALUES TO INITIAL MAX SPEED
            maxSpeed = device_speed[0];
            for(int i=0 ; i<nDevice ; ++i)
            {
                blocked_queueCount[i] = 0;
                if(device_speed[i] >= maxSpeed)
                {
                    maxSpeed = device_speed[i];
                    element = i;                    // WHEN THE LARGEST SPEED IS FOUND, RETURN THAT INDEX
                }
            }
            if(j<nDevice)
            {
                temp_deviceSpeed[j] = maxSpeed;              // STORE THE DEVICE WITH THE LARGEST SPEED IN THE FIRST INDEX OF THE TEMPORARY ARRAY
                strcpy(temp_deviceName[j],device[element]);  // STORE THAT DEVICES NAME AS WELL
                ++j;
                device_speed[element] = -1;                  // -1 USED AS EXIT STATUS. IF ALL ELEMENTS ARE -1 THEN MAXSPEED = -1 AND LOOP ENDS
            }
        }
        
        // COPY TEMPORARY ARRAY VALUES INTO THE ORIGNAL ARRAYS
        for(int i=0 ; i<nDevice ; ++i)
        {
            device_speed[i] = temp_deviceSpeed[i];
            strcpy(device[i],temp_deviceName[i]);
        }
        // LOOP TO DETERMINE I/O OPERATION PRIORITY
        for(int i=0 ; i<nProcess ; ++i)
        {
            for(int j=0; j<(op_perProcess[i]) ; ++j)
            {
                for(int k=0 ; k<nDevice ; ++k)
                {
                    if (device_speed[k] == op_Speed[i][j])
                    {
                        op_priority[i][j] = k;
                    }
                }
            }
        }
    }
}

// ------------------------------------------------------------------------------------------------------------------------

// BLOCKS A PROCESS THAT HAS MAD AN I/O REQUEST, REMOVES IT FROM THE RUNNING STATE, AND ADDS IT TO THE APPROPRIATE BLOCKED QUEUE (BASED ON PRIORITY)
void block_process(void)
{
    op_index = operation[processNumber];
    running = false;
    blocked = true;
    priority = op_priority[processNumber][op_index]; // DETERMINING THE PRIORITY OF THE CURRENT OPERATION. PRIORITY 0 CORRESPONDS TO DEVICE 0 WHICH WOULD BE
                                                     // THE FASTEST DEVICE.
    ++blocked_queueCount[priority];
    blocked_queues[priority][blocked_queueCount[priority]-1] = processNumber;  // ADDING THE BLOCKED PROCESS TO THE RIGHT QUEUE AS DETERMINED BY PRIORITY
    --queueCount;
    printf("%i process%s.RUNNING->BLOCKED(%s)\t\n",time_sinceReboot, process_name[processNumber], device[priority]);
}

// ------------------------------------------------------------------------------------------------------------------------


// UNBLOCKS A PROCESS, FREES THE DATABUS AND ADDS THAT PROCESS BACK TO THE END OF THE READY QUEUE
void unblock_process(void)
{
    priority = op_priority[blockedProcess][operation[blockedProcess]];
    transferring = false;
    queue[queueCount] = blockedProcess;    // STORE THE INDEX OF THE UNBLOCKED PROCESS AT THE END OF THE READY QUEUE
    printf("%i process%s.release_databus, process%s.BLOCKED(%s)->READY\n",time_sinceReboot, process_name[blockedProcess],process_name[blockedProcess],device[priority]);
    ++queueCount;
    blocked = false;
    data_busFree = true;
    transferTime = -1;                   // TRANSFER TIME SET TO < 0 FOR SAFETY REASONS
    
    // IF THE UNBLOCKED PROCESS IS AT THE BEGINNING OF THE READY QUEUE, BEGIN THE TIMER TO GO FROM READY->RUNNING STATE
    if(queueCount == 1)
    {
        state_changeIOexit = time_sinceReboot + TIME_CONTEXT_SWITCH;
        processNumber = blockedProcess;
    }
    // DECEREMENT AND SHIFT ALL ITEMS IN THE BLOCKED QUEUE (WHERE THE UNBLOCKED PROCESS EXITED)
    // 1 INDEX TO THE FRONT
    for(int i=0 ; i<blocked_queueCount[priority] - 1 ; ++i)
    {
        blocked_queues[priority][i] = blocked_queues[priority][i+1];
        
    }
    --blocked_queueCount[priority];
    operation[blockedProcess] += 1;
}

// ------------------------------------------------------------------------------------------------------------------------

// SHORT FUNCTION TO CHECK IF A PROCESS HAS BEGUN
void check_process_commencement(void)
{
    for(int i=0 ; i<nProcess ; i++)
    {
        // IF IT HAS BEGUN, ADD IT TO THE QUEUE AND INCREMENT THE QUEUE COUNTER AND THE PROCESS COUNTER (pCount)
        if(time_sinceReboot == process_commenceTime[i])
        {
            queue[queueCount] = pCount;
            ++queueCount;
            printf("%i process%s.NEW->READY\n", time_sinceReboot,process_name[pCount]);
            if(queueCount == 1)
            {
                processNumber = pCount;
                state_change = time_sinceReboot + TIME_CONTEXT_SWITCH;
            }
            ++pCount;
            process_runtime[pCount] = 0;
        }
    }
}

// ------------------------------------------------------------------------------------------------------------------------

// SHORT FUNCTION THAT BEGINS THE TRANSFER OF AN OPERATION BASED ON THE PRIORITY OF THE DEVICE USED
void begin_transfer(void)
{
    // CHECKS EVERY BLOCKED QUEUE IN ORDER OF PRIORITY AND WAITING TIME
    for(int i=0 ; i<nDevice ; ++i)
    {
        // (DEVICE 0, 1,... = BLOCKED QUEUE 0,1,...)
        if(blocked_queueCount[i] > 0)
        {
            blockedProcess = blocked_queues[i][0];
            op_index = operation[blockedProcess];
            // IF A BLOCKED PROCESS HAS BEEN FOUND, BEGIN THE TIMER TO ACQUIRE THE DATA BUS
            state_changeIO = time_sinceReboot + TIME_CONTEXT_SWITCH;
            priority = op_priority[blockedProcess][operation[blockedProcess]];
            // CALCULATE THE TOTAL TRANSFER TIME BASED ON THE DEVICE SPEED AND DATA SIZE
            transferTime = ceil(((((double)op_transferSize[blockedProcess][op_index])/device_speed[priority])*1000000));
            printf("%i process%s.request_databus\n",time_sinceReboot, process_name[blockedProcess]);
            data_busFree = false;
            i = nDevice;  // IF A BLOCKED PROCESS HAS BEEN FOUND, EXIT THE LOOP
        }
    }
}

// ------------------------------------------------------------------------------------------------------------------------

// SHIFTS ALL PROCESSES IN THE READY QUEUE FORWARD BY 1
void shift_forwardQueue(void)
{
    for(int i=0 ; i<(queueCount) ; ++i)
    {
        queue[i] = queue[i+1];
    }
    queue[queueCount] = -1;
}

// ------------------------------------------------------------------------------------------------------------------------

// MOVES AN EXPIRED PROCESS TO THE END OF THE READY QUEUE
void move_end_queue(void)
{
    int temp = queue[0];
    for(int i=0 ; i<(queueCount-1) ; ++i)
    {
        queue[i] = queue[i+1];
    }
    queue[queueCount-1] = temp;
}

// ------------------------------------------------------------------------------------------------------------------------


void simulate_job_mix(int time_quantum)
{
    sort_device_speed();
    initialise_process();
    printf("running simulate_job_mix( time_quantum = %i usecs )\n",
           time_quantum);
    printf("0 reboot with TQ=%i\n", time_quantum);
    
    while(pExit != nProcess)
    {
        // CHECK IF THERE IS A PROCESS THAT IS RUNNING AND IF IT HAS COMMENCED AN I/O EVENT
        if((process_runtime[processNumber] == op_commenceTime[processNumber][operation[processNumber]]) && running == true && op_perProcess[processNumber] > 0)
        {
            // BLOCK AND REMOVE THAT PROCESS FROM THE RUNNING STATE (QUEUE[0] IS USED AS THE RUNNING STATE)
            block_process();
            // IF THERE ARE OTHER PROCESSES IN THE QUEUE, SHIFT THEM ALL FORWARD AND MAKE THE PROCESS IN QUEUE[0] THE NEW RUNNING PROCESS
            if(queueCount >= 1)
            {
                shift_forwardQueue();
                processNumber = queue[0];
                state_change = time_sinceReboot + TIME_CONTEXT_SWITCH;  // ACTIVATE TIMER TO GO FROM READY-> RUNNING
            }
            else
            {
                queue[0]= -1;
            }
        }
        
        // IF A PROCESS HAS FINISHED TRANSFERRING, UNBLOCK IT, FREE THE DATA BUS AND ADD IT BACK TO THE READY QUEUE
        if(transferTime == 0)
        {
            unblock_process();
        }
        
        if(data_busFree == true && transferring == false)
        {
            begin_transfer();
        }
        
        // IF A PROCESS HAS COMMENCED, THIS FUNCTION WILL ALSO ADD IT TO THE END OF THE READY QUEUE
        check_process_commencement();
        
        if(queueCount >= 1 && running == true)
        {
            // CHECK IF THE TIME QUANTUM HAS EXPIRED AND THE PROCESS IS NOT DONE YET
            if(timeElapsed[processNumber] == time_quantum && timeremaining[processNumber] != 0)
            {
                // IF THERE ARE OTHER PROCESSES IN THE READY QUEUE, CURRENT PROCESS EXPIRES AND IS ADDED BACK TO THE END OF THE QUEUE
                if(queueCount > 1)
                {
                    running = false;
                    printf("%i process%s.expired,\t\n",time_sinceReboot, process_name[processNumber]);
                    move_end_queue();
                    printf("%i process%s.RUNNING->READY\n",time_sinceReboot, process_name[processNumber]);
                    processNumber = queue[0];           // THE NEW PROCESS NUMBER (INDEX) IS SET TO THE PROCESS AT THE FRONT OF THE QUEUE
                    state_change = time_sinceReboot + TIME_CONTEXT_SWITCH;
                }
                // IF THE CURRENT PROCESS IS THE ONLY PROCESS IN THE QUEUE, LET IT KEEP RUNNING AND REFRESH THE TQ
                if(queueCount == 1)
                {
                    printf("%i process%s.freshTQ, RUNNING->RUNNING\n", time_sinceReboot,process_name[processNumber]);
                    queue[0]= processNumber;
                }
                timeElapsed[processNumber] = 0;
            }
            // CHECK IF THE PROCESS IS DONE
            if(timeremaining[processNumber] == 0)
            {
                printf("%i process%s.RUNNING->EXIT\n",time_sinceReboot, process_name[processNumber]);
                // IF THERE ARE STILL SOME PROCESSES THAT HAVEN'T FINISHED YET, KEEP THE SIMULATION GOING
                if(pExit != nProcess)
                {
                    // IF THERE ARE MULTIPLE PROCESSES IN THE QUEUE, SHIFT THEM FORWARD
                    if(queueCount > 1)
                    {
                        --queueCount;
                        shift_forwardQueue();
                        processNumber = queue[0];
                        timeElapsed[processNumber] = 0;
                        state_change = time_sinceReboot + TIME_CONTEXT_SWITCH;
                        time_sinceReboot += 1;
                        running = false;
                        ++pExit;
                        if(transferring == true)
                        {
                            --transferTime;
                        }
                        continue;
                    }
                    // IF THE CURRENT PROCESS IS THE ONLY ONE IN THE QUEUE, GO AHEAD AND SET THE RUNNING INDEX (QUEUE[0]) TO -1
                    else if(queueCount == 1)
                    {
                        queue[0] = -1;
                        --queueCount;
                        running = false;
                        ++pExit;
                        time_sinceReboot += 1;
                        if(transferring == true)
                        {
                            --transferTime;
                        }
                        continue;
                    }
                }
                // EXIT THE LOOP IF ALL THE PROCESSES ARE FINISHED
                else if(pExit == nProcess)
                {
                    continue;
                }
            }
            if(running == true)
            {
                timeremaining[processNumber]        -= 1;
                timeElapsed[processNumber]          += 1;
                process_runtime[processNumber]      += 1;
            }
        }
        
        time_sinceReboot += 1;
        
        // CHECK IF THE TIME DEYAL FROM READY->RUNNING IS OVER AND ACTIVATE "RUNNING STATE"
        if(time_sinceReboot == state_change)
        {
            running = true;
            printf("%i process%s.READY->RUNNING\n",time_sinceReboot, process_name[processNumber]);
            timeElapsed[processNumber] = 0;
        }
        
        if(transferring == true)
        {
            --transferTime;
        }
        
        // CHECK IF THE DATABUS HAS BEEN ACQUIRED YET. IF IT HAS, SET TRANSFERRING TO TRUE
        if(state_changeIO == time_sinceReboot)
        {
            transferring = true;
        }
        
        // CHECK IF AN UNBLOCKED PROCESS CAN GO FROM READY->RUNNING YET
        if(time_sinceReboot == state_changeIOexit)
        {
            printf("%i process%s.READY->RUNNING\n",time_sinceReboot, process_name[processNumber]);
            running = true;
            timeElapsed[processNumber] = 0;
        }
        
    }
    total_process_completion_time = time_sinceReboot - (process_commenceTime[0] + 1);
    printf("total_process_completion_time %i %i\n",time_quantum, total_process_completion_time);
}

//  ----------------------------------------------------------------------

void usage(char program[])
{
    printf("Usage: %s tracefile TQ-first [TQ-final TQ-increment]\n", program);
    exit(EXIT_FAILURE);
}

// ------------------------------------------------------------------------------------------------------------------------

int main(int argcount, char *argvalue[])
{
    int TQ0 = 0, TQfinal = 0, TQinc = 0, lowest_total_completion_time = 0;

    
    //  CALLED WITH THE PROVIDED TRACEFILE (NAME) AND THREE TIME VALUES
    if(argcount == 5) {
        TQ0     = atoi(argvalue[2]);
        TQfinal = atoi(argvalue[3]);
        TQinc   = atoi(argvalue[4]);
        
        if(TQ0 < 1 || TQfinal < TQ0 || TQinc < 1) {
            usage(argvalue[0]);
        }
    }
    //  CALLED WITH THE PROVIDED TRACEFILE (NAME) AND ONE TIME VALUE
    else if(argcount == 3) {
        TQ0     = atoi(argvalue[2]);
        if(TQ0 < 1) {
            usage(argvalue[0]);
        }
        TQfinal = TQ0;
        TQinc   = 1;
    }
    //  CALLED INCORRECTLY, REPORT THE ERROR AND TERMINATE
    else {
        usage(argvalue[0]);
    }
    
    //  READ THE JOB-MIX FROM THE TRACEFILE, STORING INFORMATION IN DATA-STRUCTURES
    parse_tracefile(argvalue[0], argvalue[1]);
    
    //  SIMULATING THE JOB-MIX FROM THE TRACEFILE, VARYING THE TIME-QUANTUM EACH TIME.
    //  ATTEMPT TO FIND THE BEST (SHORTEST) TOTAL-PROCESS-COMPLETION-TIME
    //  ACROSS EACH OF THE TIME-QUANTA BEING CONSIDERED
    for(int time_quantum=TQ0 ; time_quantum<=TQfinal ; time_quantum += TQinc) {
        simulate_job_mix(time_quantum);
        // INITIALISE THE LOWEST TOTAL COMPLETION TIME AND OPTIMAL TIME QUANTUM TO THE INITIAL TQ AND TOTAL PROCESS COMPLETION TIME
        if(time_quantum == TQ0)
        {
            lowest_total_completion_time = total_process_completion_time;
            optimal_time_quantum = time_quantum;
        }
        else if(total_process_completion_time<=lowest_total_completion_time)
        {
            lowest_total_completion_time = total_process_completion_time;
            optimal_time_quantum = time_quantum;
        }
        printf("-------------------------------------------\n");
    }
    
    printf("best %i %i\n", optimal_time_quantum, lowest_total_completion_time);
    
    
    exit(EXIT_SUCCESS);
}

//  vim: ts=8 sw=4

