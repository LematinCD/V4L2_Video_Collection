#ifndef __MYSERVER_UART_H
#define __MYSERVER_UART_H
#include <semaphore.h>

pthread_mutex_t uart_read_mutex;
pthread_mutex_t uart_write_mutex;
sem_t sem_uart_write_full;
sem_t sem_uart_write_empty;


typedef struct __uart_buf{
        unsigned int uart_size;
        unsigned char uart_buf[73];        
}uart_buf;

uart_buf uart_read;
uart_buf uart_write;

int uart_init();
#endif
