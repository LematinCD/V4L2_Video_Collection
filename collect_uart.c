/********************************************************
*   Copyright (C) 2016 All rights reserved.
*   
*   Filename:myserver_uart.c
*   Author  :Lematin
*   Date    :2017-2-17
*   Describe:
*
********************************************************/
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <pthread.h>
#include "collect_uart.h"
#include "myconfig.h"



int set_opt(int fd,int nSpeed, int nBits, char nEvent, int nStop)
{
    struct termios newtio,oldtio;
    if  ( tcgetattr( fd,&oldtio)  !=  0) 
    { 
        perror("SetupSerial 1");
        return -1;
    }
    bzero( &newtio, sizeof( newtio ) );
    newtio.c_cflag  |=  CLOCAL | CREAD; 
    newtio.c_cflag &= ~CSIZE; 

    switch( nBits )
    {
    case 7:
        newtio.c_cflag |= CS7;
        break;
    case 8:
        newtio.c_cflag |= CS8;
        break;
    }

    switch( nEvent )
    {
    case 'O':                     //奇校验
        newtio.c_cflag |= PARENB;
        newtio.c_cflag |= PARODD;
        newtio.c_iflag |= (INPCK | ISTRIP);
        break;
    case 'E':                     //偶校验
        newtio.c_iflag |= (INPCK | ISTRIP);
        newtio.c_cflag |= PARENB;
        newtio.c_cflag &= ~PARODD;
        break;
    case 'N':                    //无校验
        newtio.c_cflag &= ~PARENB;
        break;
    }

switch( nSpeed )
    {
    case 2400:
        cfsetispeed(&newtio, B2400);
        cfsetospeed(&newtio, B2400);
        break;
    case 4800:
        cfsetispeed(&newtio, B4800);
        cfsetospeed(&newtio, B4800);
        break;
    case 9600:
        cfsetispeed(&newtio, B9600);
        cfsetospeed(&newtio, B9600);
        break;
    case 115200:
        cfsetispeed(&newtio, B115200);
        cfsetospeed(&newtio, B115200);
        break;
    default:
        cfsetispeed(&newtio, B9600);
        cfsetospeed(&newtio, B9600);
        break;
    }
    if( nStop == 1 )
    {
        newtio.c_cflag &=  ~CSTOPB;
    }
    else if ( nStop == 2 )
    {
        newtio.c_cflag |=  CSTOPB;
    }
    newtio.c_cc[VTIME]  = 0;
    newtio.c_cc[VMIN] = 0;
    tcflush(fd,TCIFLUSH);
    if((tcsetattr(fd,TCSANOW,&newtio))!=0)
    {
        perror("com set error");
        return -1;
    }
    printf("set done!\n");
    return 0;
}


int open_port(int fd)
{
    
    fd = open( "/dev/ttyUSB0", O_RDWR|O_NOCTTY|O_NDELAY);
    if (-1 == fd)
    {
        perror("Can't Open Serial Port");
        return(-1);
    }
    else 
    {
        printf("open ttyS0 .....\n");
    }
   
   
    if(fcntl(fd, F_SETFL, 0)<0)
    {
        printf("fcntl failed!\n");
    }
    else
    {
        printf("fcntl=%d\n",fcntl(fd, F_SETFL,0));
    }
    if(isatty(STDIN_FILENO)==0)
    {
        printf("standard input is not a terminal device\n");
    }
    else
    {
        printf("isatty success!\n");
    }
    printf("fd-open=%d\n",fd);
    return fd;
} 



int read_n_bytes(int fd,char* buf,int n)
{

/*read_len用来记录每次读到的长度，len用来记录当前已经读取的长度*/
    int read_len = 0,len = 0;
    int flag = 0;    

/*read_buf用来存储读到的数据*/
    char* read_buf = buf;
    while(len < n)
    {
        if((read_len = read(fd,read_buf,n)) > 0)
		{
#ifdef MY_ZIGBEE        
			/*根据帧头校验,如果不是0x24,则跳过*/    
            if(read_buf[0] == MY_UART_HEAD1 ){    
#elif M0
            if(read_buf[0] == MY_UART_HEAD2 || read_buf[0] == MY_UART_HEAD3){    
#endif				
                    flag = 1;    
             }
             if(flag){
                    len += read_len;
                    read_buf += read_len;
                    read_len = 0;
             }
                
        }

/*休眠时间视情况而定，可以参考波特率来确定数量级*/
        usleep(5);

    }
    return n;
}

void *pthread_uart_read_fun(void *args)
{
    static buff_byte_count = 0;    
    
    int i;
    int count;    
    unsigned char buff[MY_UART_BUF_SIZE];
    while(1){
        if(0 != pthread_mutex_lock(&uart_read_mutex)){
	        printf("uart_read_mutex lock fail!\n");
		break;
        }
        memset(&uart_read,0,sizeof(uart_read));
       	uart_read.uart_size=read_n_bytes(*(int *)args,&uart_read.uart_buf[MY_UART_BUF_DATA_POS],MY_UART_BUF_SIZE);
       	memcpy(&uart_read.uart_buf[MY_UART_BUF_SIZE_POS],&uart_read.uart_size,4);
#ifdef DEBUG      	
        for(i=9;i<uart_read.uart_size+MY_UART_BUF_DATA_POS;i++)
        printf("%x ",uart_read.uart_buf[i]);
        printf("\nsize:%d\n",uart_read.uart_size);
        printf("***********************count:%d*****************************\n",((++count)*MY_UART_BUF_SIZE)); 
#endif      
	if(0 != pthread_mutex_unlock(&uart_read_mutex)){
		printf("uart_read_mutex unlock fail!\n");
		break;
	}
        usleep(5000);    
    }	
    close(*(int *)args);   
}

void *pthread_uart_write_fun(void *args)
{
	int ret = 0,i=0;
	while(1){
		if (0 > sem_wait(&sem_uart_write_full)) {
			perror("sem_wait");
			pthread_exit("pthread_exit unnormally!");
		}
			ret=write(*(int *)args,&uart_write.uart_buf[MY_UART_BUF_DATA_POS],MY_UART_BUF_SIZE);
        for(i=9;i<45;i++)
        printf("%x ",uart_write.uart_buf[i]);
        printf("\nsize:%d\n",ret);
#ifdef DEBUG			
			printf("uart_write ret= %d\n",ret);
#endif			
		if (0 > sem_post(&sem_uart_write_empty)) {
			perror("sem_post");
			pthread_exit("pthread_exit unnormally!");
		}
	}
}

int uart_init()
{
	
    	int *uart_fd = (int *)malloc(sizeof(int));
	if((*uart_fd=open_port(*uart_fd))<0)
    	{
        	perror("open_port error");
        	return -1;
    	}
    	if(set_opt(*uart_fd,MY_UART_BAUD,MY_UART_NBITS,MY_UART_NEVENT,MY_UART_NSTOP)<0)
    	{
        	perror("set_opt error");
        	return -1;
    	}
#ifdef DEBUG    	
    	printf("uart_fd=%d\n",*uart_fd);
#endif        
        
        pthread_t pthread_uart_read;
       	pthread_t pthread_uart_write;

        if (0 > sem_init(&sem_uart_write_full, 0, 0)) {
		perror("sem_init convert");
		return -1;
	}

        
        if (0 > sem_init(&sem_uart_write_empty, 0, 1)) {
		perror("sem_init send");
		return -1;
	}
        
        if(0 != pthread_mutex_init(&uart_read_mutex,NULL)){
		printf("uart_read_mutex error!\n");
		return -1;
	}
        
        
        if(0 != pthread_create(&pthread_uart_read,NULL,pthread_uart_read_fun,uart_fd)){
                printf("pthread_create uart read error!\n");
                return -1;
        }
        if(0 != pthread_create(&pthread_uart_write,NULL,pthread_uart_write_fun,uart_fd)){
                printf("pthread_create uart write error!\n");
                return -1;
        }
        
        return 0;
}




