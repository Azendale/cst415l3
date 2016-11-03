#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <sys/time.h>

#include "rsp.h"

//*********************************************
// compute the msecs after the epoc
static uint64_t timestamp()
{
    struct timeval time;
    uint64_t timestamp;

    gettimeofday(&time, NULL);

    timestamp = time.tv_sec;
    timestamp *= 1000;
    timestamp += time.tv_usec/1000;

    return timestamp;
}

//*********************************************
int main(int argc, char **argv)
{
    char buff[200], buff2[200];
    int length, length2;
    int total_bytes = 0;
    uint64_t timestamp1, timestamp2;

    timestamp1 = timestamp();

    rsp_init(2000);

    rsp_connection_t rsp1, rsp2;

    rsp1 = rsp_connect("pwh_send2a");
    rsp2 = rsp_connect("pwh_send2b");
    if (rsp1 != NULL && rsp2 != NULL) 
    {
        rsp_write(rsp1, &timestamp1, sizeof(timestamp1));
        rsp_read(rsp1, &timestamp2, sizeof(timestamp2));

        if (timestamp1 < timestamp2)
        {
            printf("Sender: Connections established\n");

            while ((length = fread(buff, 1, sizeof(buff), stdin)) != 0)
            {
                fwrite(buff, length, 1, stderr);
                rsp_write(rsp1, buff, length);
                rsp_write(rsp2, buff, length);
                total_bytes += length;
            }
        }
        else
        {
            printf("Receiver: Connections established\n");

            while ((length = rsp_read(rsp1, buff, sizeof(buff))) > 0)
            {
                fwrite(buff, 1, length, stderr);
                length2 = rsp_read(rsp2, buff2, sizeof(buff2));
                total_bytes += length;
                if (length != length2) 
                {
                    printf("******* size mismatch *****\n");
                }
                else if (memcmp(buff, buff2, length) != 0)
                {
                    printf("******* data mismatch *****\n");
                }
            }
        }

        rsp_close(rsp1);
        rsp_close(rsp2);
        printf("sent %d transferred\n", total_bytes);
    }

    rsp_shutdown();

    return 0;
}
