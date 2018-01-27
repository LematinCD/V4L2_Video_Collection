#ifndef __MYSERVER_CAM_H
#define __MYSERVER_CAM_H
#include <semaphore.h>
#include "myconfig.h"
#define DBUG_INFO(...) do{ fprintf(stdout,"\n\033[41;32m[DBUG_INFO]:"\
                "\033[0m");fprintf(stdout,__VA_ARGS__);}while(0)
#define DBUG_ERR(...) do{fprintf(stdout,"\n[DBUG_ERR]:");\
                        fprintf(stdout,__VA_ARGS__);}while(0)
#define DBUG_WAR(...) do{ fprintf(stdout,"\n[DBUG_WAR]:");\
                        fprintf(stdout,__VA_ARGS__); }while(0)

typedef struct my_image_tag{
        unsigned int image_size;
        unsigned char image_buf[MY_WIDE*MY_HIGH*3];
}test_my_image;

test_my_image my_image;


#ifdef YUYV
sem_t sem_camera_full;
sem_t sem_camera_empty;
#elif  JPEG
pthread_mutex_t camera_mutex;
#endif
int mycamera_init(char **dev_path);
#endif
