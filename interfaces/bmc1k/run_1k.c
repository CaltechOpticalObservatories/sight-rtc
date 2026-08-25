
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

#include <ImageStruct.h>        // cacao data structure definition
#include <ImageStreamIO.h>      // function ImageStreamIO_read_sharedmem_image_toIMAGE()
#include "hvaluts.h"            // this includes bmc_mdlib.h

static int end_signaled = 0;    // termination flag for funcs

// CLI set options
static int inithva         = 0;
static int setspindelay    = 0;
static int spindelay       = 0;
static int startloop       = 0;

#define DMVOLT_FILENAME "dmvolt"    // Name of DM shared memory
double dt_update_lim = 10800.0;      // [seconds] if no command is received during this time, set DM to zero V


static void end_me(int dummy) {
    // Termination function for SIGINT callback
    end_signaled = 1;
}
static int argparse(int argc, char **argv);
static void print_help(char *progname, char *errmsg);

int main(int argc, char **argv) {
    // register interrupt signal to terminate the main loop
    signal(SIGINT, end_me);

    // Process command line arguments
    if(argparse(argc, argv) != 0) {
        exit(1);
    }

    // Declare some needed variables
    long ActIndex[num_act_height][num_act_width];    // Mapping from [y,x] to actuator number
    int Index = 0;                      //
    short s;                            //
    int sx, sy, s1;                     // for-loop counters
    long framecnt;                      // Number of DM images applied
    long cntloop;                       // main loop counter
    IMAGE *SMdmvolt;                    // shared mem structure
    long cnt0;                          // stores the ctn0 from the shared memory struct
    static uint16_t sActValue[size_1k]; // buffer for actuator values
    struct timespec tnow;               //
    double tlastupdatedouble = 0;       // time when we last updated the DM
    double tlastsleepdouble = -1;       // time when the DM last went to sleep
    double dt_update;                   // time since last update

    /*
    * open the BMC interface
    *
    * See function definitions in bmc_mdlib.h
    */
    if(kBMCEnoErr != (err = BMCopen(sNdx, &sBMC))) {
        printf("ERR: BMCopen: %d gave %s\n", sNdx, BMCgetErrStr(err));
        return 1;
    }

    if(inithva) {
        if(kBMCEnoErr != (err = BMCsetUpHVA(sBMC, hvamode))) {
            printf("ERR: BMCsetUpHVA %p/%d gave %s\n", sBMC, hvamode, BMCgetErrStr(err));
            return 1;
        }
        else {
            printf("BMCsetUpHVA: to %d OK\n", hvamode);
        }

        if(kBMCEnoErr != (err = BMCwriteHVALUT(sBMC, size_1k, lut_1k, amap_1k, minlut_1k))) {
            printf("ERR: BMCwriteHVALUT gave %s\n", BMCgetErrStr(err));
            return 1;
        }
        printf("BMCwriteHVALUT: OK\n");
    }

    if(setspindelay) {
        if(kBMCEnoErr != (err = BMCsetSpinDelay(sBMC, spindelay))) {
            printf("ERR: BMCsetSpinDelay %p/%d gave %s\n", sBMC, spindelay, BMCgetErrStr(err));
            return 1;
        }
    }

    // build up mapping
    s = 0;
    for(sy = 0; sy < num_act_height; sy++) {
        // printf("RANGE %3ld:  %ld - %ld\n", sy, DMStartRow[sy], DMEndRow[sy]);
        for(sx = 0; sx < num_act_width; sx++) {
            if((sx >= DMStartRow[sy]) && (sx <= DMEndRow[sy])) {
                ActIndex[sy][sx] = s;
                //printf("[%4ld %4ld -> %4ld]", (long)sx, (long)sy, (long)ActIndex[sx][sy]);
                s++;
            }
            else {
                ActIndex[sy][sx] = -1;
            }
        }
        //printf("\n");
    }
    //printf("\n");


    // CONNECT TO VOLT MAP SHARED MEMORY
    SMdmvolt = (IMAGE *)malloc(sizeof(IMAGE));
    errno_t ioerr;
    ioerr = ImageStreamIO_read_sharedmem_image_toIMAGE(DMVOLT_FILENAME, &SMdmvolt[0]);
    if (ioerr != IMAGESTREAMIO_SUCCESS) {
        exit(1);
    }
    unsigned short int temp[SMdmvolt[0].md[0].nelement];      // temporarily hold the 38x38 DM commands

    // MAIN LOOP
    printf("ENTERING MAIN DM LOOP\n");
    fflush(stdout);
    framecnt = -1;
    cntloop = 0;
    while((end_signaled == 0) && (SMdmvolt[0].md[0].status < 100)) {  //&&(cntloop<10000)) {
        cnt0 = SMdmvolt[0].md[0].cnt0;
        if((cnt0 > framecnt) && (SMdmvolt[0].md[0].write == 0)) {
            // new frame is here... apply on DM

            // For 2D image with pixel indices ii (x-axis) and jj (y-axis), the pixel values are stored as array.<TYPE>[ jj * md[0].size[0] + ii ] \n
            // image md[0].size[0] is x-axis size, md[0].size[1] is y-axis size
            memcpy(temp, SMdmvolt[0].array.UI16, SMdmvolt[0].md[0].nelement*sizeof(unsigned short int));

            s1 = 0;
            for(sy = 0; sy < num_act_height; sy++) {
                for(sx = 0; sx < num_act_width; sx++) {
                    Index = ActIndex[sy][sx];
                    if(Index >= 0) {
                        // Maps from 38x38 image to 952 element array
                        sActValue[Index] = (uint16_t) temp[s1];
                    }
                    s1++;
                }
            }

            /*
            **	BMCburstHVA(bmc,count,data)
            **  Write 32b words to the burst window
            **		count is the number of 16b words to write
            **			NOTE: count _must_ be even
            **		data[n] is the value for the nth actuator
            */
            if(kBMCEnoErr != (err = BMCburstHVA(sBMC, size_1k, sActValue))) {
                printf("ERR: decode_args:BMCburstHVA %p gave %s\n", sBMC, BMCgetErrStr(err));
                fflush(stdout);
            }

            framecnt = cnt0;
            SMdmvolt[0].md[0].cnt1++;
            clock_gettime(CLOCK_REALTIME, &tnow);
            // tnowdouble = 1.0*tnow.tv_sec + 1.0e-9*tnow.tv_nsec;
            //       dt_update = tnowdouble - tlastupdatedouble;

            tlastupdatedouble = 1.0*tnow.tv_sec + 1.0e-9*tnow.tv_nsec;
            cntloop++;
        }

        else {
            // do not apply anything... wait 10us
            usleep(10); // 10 us
            clock_gettime(CLOCK_REALTIME, &tnow);
            dt_update = 1.0*tnow.tv_sec + 1.0e-9*tnow.tv_nsec - tlastupdatedouble;
            if((dt_update > dt_update_lim) && (tlastupdatedouble > tlastsleepdouble) && (SMdmvolt[0].md[0].write==0)) {
                // set DM to zero
                SMdmvolt[0].md[0].write = 1;
                for(s1 = 0; s1 < SMdmvolt[0].md[0].nelement; s1++) {
                    SMdmvolt[0].array.UI16[s1] = 0;
                }
                SMdmvolt[0].md[0].write = 0;
                SMdmvolt[0].md[0].cnt0++;
                tlastsleepdouble = tlastupdatedouble;
                printf("DM going to sleep... \n");
            }
        }

    }

    printf("EXITING MAIN LOOP\n");
    printf("[%10ld] SENT TO DM : %ld / %ld  (%6.2f%%)\n", cntloop,
           SMdmvolt[0].md[0].cnt1, SMdmvolt[0].md[0].cnt0,
           100.0 * SMdmvolt[0].md[0].cnt1 / SMdmvolt[0].md[0].cnt0);
    fflush(stdout);


    // Close DM and exit
    if(NULL != sBMC) {
        (void)BMCclose(sBMC);
    }
    exit(0);

}

static int argparse(int argc, char **argv) {
    char *progname = argv[0];

    --argc;
    ++argv;
    while(argc && ((argv[0][0] == '-') || (argv[0][0] == '/'))) {
        switch(argv[0][1]) {
            case 'D':
                ++argv;
                --argc;
                if(argc < 1) {
                    print_help(progname, "Error: option 'D' requires a numeric argument\n");
                }
                if((argv[0][0] >= '0') && (argv[0][0] <= '9')) {
                    setspindelay=1;
                    spindelay = atoi(argv[0]);
                    puts("Got D\n");
                }
                else {
                    print_help(progname, "Error: option 'D' requires a numeric argument\n");
                }
                break;

            case 'K':
                inithva = 1;
                puts("Got K\n");
                break;

            case 'L':
                startloop = 1;
                puts("Got L\n");
                break;

            case 'A':
                setspindelay=1;
                spindelay=0;
                inithva = 1;
                startloop = 1;
                puts("Got A\n");
                return 0;
                //break;

            case '-':
                if(strcmp(argv[0], "--help") == 0) {
                    print_help(progname, "");
                }
                else {
                    fprintf(stderr, "Unknown option: %s\n", argv[0]);
                    print_help(progname, "");
                }
                return 1;

            case '?':
            case 'h':
                print_help(progname, "");
                return 1;
            default:
                fprintf(stderr, "Unknown flag -'%c'\n", argv[0][1]);
                print_help(progname, "");
                return 1;
        }
        argc--;
        argv++;
    }
    return 0;
}

static void print_help(char *progname, char *errmsg) {
    puts(errmsg);
    printf("%s: Simple example program that acquires voltage commands and writes them to a 1K BMC DM\n", progname);
    puts("");
    printf("Usage: %s [-D count] [-K]\n", progname);
    printf("   -D count         Set driver spin delay count\n");
    printf("   -K               Initialize board for 1k DM\n");
    printf("   -L               Real time control loop\n");
    printf("   -A               Run -K, -D 0, -L: init+start main loop\n");
    printf("   -h               This help message\n");
    //exit(1);
}
