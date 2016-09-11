#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include <string.h>
#include <err.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <time.h>
#include <netinet/in.h>
#include <fcntl.h>

#include "mirsdrapi-rsp.h"

#define DEFAULT_SAMPLE_RATE		2048000
#define DEFAULT_BUF_LENGTH		(336 * 2) // (16 * 16384)
#define MINIMAL_BUF_LENGTH		672 // 512
#define MAXIMAL_BUF_LENGTH	        (256 * 16384)
#define DEFAULT_SAMPLES_PER_PACKET      336

#define SOCKET_ERROR -1


#include "sdrplayserver.h"


/* global config parameters */
float frequencyMHZ = 100000000;
int gainReduction = 70;
float sampleRateMHZ = 2048000;
int port = 1234;
int interfaceId = 0;
int debug = 0;


long c = 0; // counter for packets from SDR device
long s = 0; // variable for socket

// the default samples per packet is 336
int samplesPerPacket = 336;

// buffers to store IQ data from device
short *ibuf; 
short *qbuf; 
uint8_t *buffer;
int  connfd = 0;

unsigned int firstSample;
int grChanged, fsChanged, rfChanged;

int bufferFull = 0;

/* not using the callback method of the API
void callback(short *xi, short *xq, unsigned int numSamps, unsigned int reset) {
// put data in a buffer to be used in main loop

    int j = 0;
    if (!connfd) {
        return;
    }
    for (int i=0; i < samplesPerPacket; i++) {
        buffer[j++] = (unsigned char) ((xi[i] >> 8)+128);
        buffer[j++] = (unsigned char) ((xq[i] >> 8)+128);
    }
    write(connfd, buffer, (DEFAULT_BUF_LENGTH * sizeof(uint8_t)));
    c++;
    if (c % 10000 == 0) {
        printf("Packets read : %d\n", c);
    } 
    s += 336;
    bufferFull = 1;
    return;
}


void callbackGC(unsigned int gRdB, unsigned int lnaGRdB) {
    printf("callbackGC gRdB=%d lnaGRdB=%d\n", gRdB, lnaGRdB);
    return;
}
*/

/**
Structure for rtl_tcp command
SDR# sends these commands upon startup
1     frequency
2     sample rate
3     gain mode
5     freq correction
8     agc mode
13    tuner gain by index
*/

struct sdr_command {
    int8_t cmd;
    int32_t param;
} __attribute__((packed));

void setSDRPlayFreq(double f) {
    if (debug) printf("Setting freq to %f hz", f);

    // this method may fail if new frequency is out of current band
    // would need to check for this cas and call unInit init
    //
    // mir_sdr_SetRf(freq, 1, 0);

    mir_sdr_ErrT err;
    int gRdB = 0;
    int gRdBsystem = 0;
    int sps;
    // only the rfFreq will be set
    err = mir_sdr_Reinit(&gRdB, 0.0, f/1000000, 0,
                        0, 0, 0, &gRdBsystem, 0,
                        &sps, mir_sdr_CHANGE_RF_FREQ);
    if (err != mir_sdr_Success) {
        printf("ERROR %d %s\n", err, mir_error_strings[err]);
    }
}

void setSDRPlaySampleRate(double sr) {
    if (debug) printf("Setting sample rate");
    mir_sdr_SetFs(sr, 1, 0, 0);
}



/**
  Check if the socket is ready to read
  Reads 5 bytes RTL command
  TODO fix up magic numbers
       loop to ensure reading of whole command
*/
int checkCmd() {
    fd_set readfds;
    int s = connfd;
    struct timeval tv= {1, 0};
    int received = 0;
    int r = 0;
    struct sdr_command cmd;

    FD_ZERO(&readfds);
    FD_SET(s, &readfds);
    tv.tv_sec = 0;
    tv.tv_usec = 5;
    r = select(s+1, &readfds, NULL, NULL, &tv);
    if(r) {
//         printf("RX Data\n");
        received = recv(s, &cmd, 5, 0);
        if (received == -1) {
            printf("Negative read\n");
            return -1;
        }
                            
        if (2) printf("RX CMD : %d PARAM=%d\n", cmd.cmd, ntohl(cmd.param));
//        if (1) printf("CMD PARAM    : %d\n", ntohl(cmd.param));

        long ifgain;
        printf("  ");
        switch(cmd.cmd) {
            case 0x01:
                printf("set freq %u\n", ntohl(cmd.param));
                setSDRPlayFreq(ntohl(cmd.param));
                break;
            case 0x02:
                printf("set sample rate %u\n", ntohl(cmd.param));
                setSDRPlaySampleRate(ntohl(cmd.param));
                break;
            case 0x03:
                printf("NOT IMPLEMENTED set gain mode %u\n", ntohl(cmd.param));
//                rtlsdr_set_tuner_gain_mode(dev, ntohl(cmd.param));
                break;
            case 0x04:
                printf("NOT IMPLEMENTED set gain %u\n", ntohl(cmd.param));
//               rtlsdr_set_tuner_gain(dev, ntohl(cmd.param));
                break;
            case 0x05:
                printf("NOT IMPLEMENTED set freq correction %u\n", ntohl(cmd.param));
//                rtlsdr_set_freq_correction(dev, ntohl(cmd.param));
                break;
            case 0x06:
                ifgain = ntohl(cmd.param);
                printf("NOT IMPLEMENTED set if stage %ld gain %d\n", ifgain >> 16, (short)(ifgain & 0xffff));
//                rtlsdr_set_tuner_if_gain(dev, tmp >> 16, (short)(tmp & 0xffff));
                break;
            case 0x07:
                printf("NOT IMPLEMENTED set test mode %u\n", ntohl(cmd.param));
//                rtlsdr_set_testmode(dev, ntohl(cmd.param));
                break;
            case 0x08:
                printf("NOT IMPLEMENTED set agc mode %u\n", ntohl(cmd.param));
//                rtlsdr_set_agc_mode(dev, ntohl(cmd.param));
                break;
            case 0x09:
                printf("NOT IMPLEMENTED set direct sampling %u\n", ntohl(cmd.param));
//                rtlsdr_set_direct_sampling(dev, ntohl(cmd.param));
                break;
            case 0x0a:
                printf("NOT IMPLEMENTED set offset tuning %u\n", ntohl(cmd.param));
//                rtlsdr_set_offset_tuning(dev, ntohl(cmd.param));
                break;
            case 0x0b:
                printf("NOT IMPLEMENTED set rtl xtal %u\n", ntohl(cmd.param));
//               rtlsdr_set_xtal_freq(dev, ntohl(cmd.param), 0);
                break;
            case 0x0c:
                printf("NOT IMPLEMENTED set tuner xtal %u\n", ntohl(cmd.param));
//                rtlsdr_set_xtal_freq(dev, 0, ntohl(cmd.param));
                break;
            case 0x0d:
                printf("NOT IMPLEMENTED set tuner gain by index %u\n", ntohl(cmd.param));
//                set_gain_by_index(dev, ntohl(cmd.param));
                break;
            default:
                printf("Unknown command %d\n", cmd.cmd);
        }
    } else {
        // socket not ready to read, ignore
    }
    return 0;
}


/**
    Main loop, waits for a connection
    Once connected attempts to initialise the SDR device
    Checks for RTL commands and writes IQ data
*/
int server() {
    int listenfd = 0;
    struct sockaddr_in serv_addr; 

    char sendBuff[4096];
    int connected = 0;
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serv_addr, '0', sizeof(serv_addr));
    memset(sendBuff, '0', sizeof(sendBuff)); 

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port); 
    if (bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        int errorNumber = errno;
        printf("Error binding to port %d errno=%d %s\n", port, errorNumber, strerror(errorNumber));
        exit(1);
    }
    listen(listenfd, 10); 

    while (1) {
        if (debug) printf("Waiting for connection\n");
        connfd = accept(listenfd, (struct sockaddr*)NULL, NULL); 
        if (debug) printf("Connection accepted %d\n", connfd);
        if (initSDRPlay()) {
            if (debug)printf("Failed to initialise SDRPlay\n"); 
        } else {
            connected = 1;
        }

        // add this offset to the sample, to do with 2s complement?
        int offset = 128;
        mir_sdr_ErrT err;
        while (connected) {
            if (checkCmd()) {
                // check command failed or socket was closed, change state to not connected
                connected = 0;
                continue;
            }
            
            err = mir_sdr_ReadPacket(ibuf, qbuf, &firstSample, &grChanged, &rfChanged,&fsChanged);
            if (err > 0) {
                if (debug) printf("MIR ERR=%d\n", err);
                connected = 0;
            } else {
                int j = 0;
                // offset was used in Tony H
                for (int i=0; i < samplesPerPacket; i++) {
                    buffer[j++] = (unsigned char) ((ibuf[i] >> 8)+offset);
                    buffer[j++] = (unsigned char) ((qbuf[i] >> 8)+offset);
                }
                int wr = write(connfd, buffer, (DEFAULT_BUF_LENGTH * sizeof(uint8_t)));
                if (wr == -1) {
                    if (debug) printf("Socket closed\n");
                    connected = 0;
                    break;
                }
                c++;
                if (c % 50000 == 0) {
                    if (debug) printf("Packets read : %ld\n", c);
                } 
                s += 336;
            }
        }
        mir_sdr_Uninit();
        if (debug)printf("Closing socket\n");
        close(connfd);
    }

    // should not get here
    // TODO add sig handlers to exit gracefully
    if (debug) printf("Server returning\n");
    close(connfd);
    return 0;
}
    



int initSDRPlay() {

    printf("Starting SDRPlay\n");

    // data declarations
    mir_sdr_ErrT err;
    float ver;
    // check API version
    err = mir_sdr_ApiVersion(&ver);
    if (debug) printf("SDRPLAY VERSION = %2.2f\n", ver);
    if (ver != MIR_SDR_API_VERSION)
    {
        printf("API Version mismatch %2.2f != %2.2f\n", ver, MIR_SDR_API_VERSION);
        close(connfd);
        exit(1);
    }

    // enable SDR API debug output
    mir_sdr_DebugEnable(debug);    

/* this is the API call to setup callback stream, not used yet
mir_sdr_ErrT mir_sdr_StreamInit(int *gRdB, double fsMHz, double rfMHz, 
                                mir_sdr_Bw_MHzT bwType, mir_sdr_If_kHzT ifType, 
                                int LNAEnable, int *gRdBsystem, int useGrAltMode, 
                                int *samplesPerPacket, mir_sdr_StreamCallback_t StreamCbFn, 
                                mir_sdr_GainChangeCallback_t GainChangeCbFn , void *cbContext)
*/


    /* This is the callback version */
//    err = mir_sdr_StreamInit(&newGr , 2.048, 1.5, 
//                             mir_sdr_BW_0_600, mir_sdr_IF_Zero, 0, 
//                             &sysGr, 1, &sps, callback, callbackGC , (void *)NULL);

    err = mir_sdr_Init(gainReduction, sampleRateMHZ, frequencyMHZ, mir_sdr_BW_1_536, mir_sdr_IF_Zero, &samplesPerPacket );
    if (err != mir_sdr_Success) {
        printf("ERROR %d %s\n", err, mir_error_strings[err]);
        return 1;
    }
    return 0;
}


/**
SDRPlay requires reintialising if the new RF is outside the current RF band
This function checks which band the current and new frequency lie in, and 
returns true if they are in different bands
Used to determine if need to call unInit and init
But, easier to call reInit

*/
int initRequired(double freqCurrent, double freqNew) {

    int i, indexCurrent, indexNew;

    for (i=ARRAYLEN(frequencyAllocations)-1; i >=0; i--) {
        if (freqCurrent < frequencyAllocations[i]) {
            indexCurrent = i;
        }
        if (freqNew < frequencyAllocations[i]) {
            indexNew = i;
        }
    }
   
    printf("FC=%f FN=%f C=%d N=%d\n", freqCurrent, freqNew, indexCurrent, indexNew);
    if (indexCurrent != indexNew) {
        return 1;
    } else {
        return 0;
    } 


    
    

}

void usage(void) {
    printf("mysdrplay, an I/Q spectrum server for SDRPlay receiver\n\n"
           "Usage:\t[-p listen port (default: 1234)]\n"
           "\t[-f frequency to tune to [Hz]]\n"
           "\t[-r gain reduction (default: 60)]\n"
           "\t[-s samplerate in Hz (default: 2048000 Hz)]\n"
           "\t[-i interface index (default: 0)]\n"
           "\t[-d enable debug output]\n");
    int i;
    for (i=0; i<ARRAYLEN(frequencyAllocations); i++) {
        printf("Freq range %f\n", frequencyAllocations[i]);
    }
    exit(1);
}


int main(int argc, char **argv) {

//    signal(SIGTERM, sighandler);
//    signal(SIGINT, sighandler);


    // add all signals while testing
    for (int i=0; i<32; i++) {
        signal(i, sighandler);
    }

    int freq = 100000000;
    int samp_rate = 2048000;

    char opt;
    while ((opt = getopt(argc, argv, "p:f:s:r:i:d")) != -1) {
        switch (opt) {
            case 'i':
                printf("Specifying interface id %s not supported\n", optarg);
                interfaceId = atoi(optarg);
                break;
            case 'f':
                freq = (uint32_t)atof(optarg);
                break;
            case 'r':
                gainReduction = (int)(atof(optarg)) ; 
                break;
            case 's':
                samp_rate = (uint32_t)atof(optarg);
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'd':
                debug = 1;
                break;
            default:
                usage();
                break;
        }
    }

    sampleRateMHZ = (float)samp_rate / 1000000;
    frequencyMHZ = (float)freq / 1000000;

    printf("Interface ID=%d\n", interfaceId);
    printf("Frequency=%3.6f MHz\n", frequencyMHZ);
    printf("Gain Reduction=%d\n", gainReduction);
    printf("Sample Rate=%3.6f MHz\n", sampleRateMHZ);
    printf("Port=%d\n", port);
    printf("Debug=%d\n", debug);

    ibuf = malloc(samplesPerPacket * sizeof(short));
    qbuf = malloc(samplesPerPacket * sizeof(short));
    buffer = malloc(DEFAULT_BUF_LENGTH * sizeof(uint8_t));

    server();

    return 0;
}

void sighandler(int signum) {
    printf("Signal caught %d\n", signum);
    close(connfd);
    exit(1);
}
