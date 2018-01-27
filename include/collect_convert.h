#ifndef __COLLECT_CONVERT_H
#define __COLLECT_CONVERT_H

#ifdef YUYV

#include <semaphore.h>
#include "myconfig.h"
#define ROUND_0_255(v)	((v) < 0 ? 0 : ((v) > 255 ? 255 : (v)))
typedef struct my_jpeg_tag{
        unsigned int jpeg_size;
        unsigned char jpeg_buf[MY_WIDE*MY_HIGH];
}test_my_jpeg;

test_my_jpeg my_jpeg;
pthread_mutex_t convert_mutex;
sem_t sem_convert_full;
sem_t sem_convert_empty;


#endif


#endif
