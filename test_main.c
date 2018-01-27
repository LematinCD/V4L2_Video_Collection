/********************************************************
*   Copyright (C) 2016 All rights reserved.
*   
*   Filename:main.c
*   Author  :Lematin
*   Date    :2017-2-17
*   Describe:
*
********************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include "collect_cam.h"

#ifdef YUYV
#include "collect_convert.h"
#endif
#include "myconfig.h"
void send_fun(void *args);

int main (int argc, char **argv) 
{ 

	if(argc != 3){
		printf("Usage:./client.Pi server_address camera_path\n");
		printf("example:./client.Pi 139.199.1.108 /dev/video1\n");
		exit(EXIT_FAILURE);
	}
	
	if(0>mycamera_init(&argv[2])){
		printf("camera init failed!\n");
		exit(EXIT_FAILURE);
	}
	printf("camera init success!\n");
#ifdef YUYV        
        if(0>convert_init()){
        	printf("convert init failed!\n");
		exit(EXIT_FAILURE);
        }
        printf("convert init success!\n");
#endif	

/*	
	if(0>uart_init()){
		printf("uart init failed!\n");
		exit(EXIT_FAILURE);
	}
	printf("uart init success!\n");
  */     	
		
	int socket_fd;
	if(0>(socket_fd = socket(AF_INET,SOCK_STREAM,0))){
		perror("socket");
		exit(EXIT_FAILURE);
	}

	struct sockaddr_in myserver_addr;
	memset(&myserver_addr,0,sizeof(myserver_addr));
	myserver_addr.sin_family = AF_INET;
	myserver_addr.sin_port = htons(8888);


	myserver_addr.sin_addr.s_addr = inet_addr(argv[1]);

	if(0>connect(socket_fd,(struct sockaddr *)&myserver_addr,sizeof(myserver_addr))){
		perror("connect");
		exit(EXIT_FAILURE);
			
	}

	int ret = 0;
	char send_buf[5] = {0x0};
	
	char recv_buf[5] = {0x0};
	pthread_t pthread_send;

	
	send_buf[4] = (unsigned char)Open_Camera_Yes_Or_No;
	while(1)
	{
		//memset(send_buf,0,5);
		memset(recv_buf,0,5);
		ret = write(socket_fd,send_buf,5);
		if(ret<0){
			perror("write");
			break;
		}

		printf("request to server!\n");
		ret = read(socket_fd,recv_buf,5);
		if(ret<0){
			perror("read");
			break;
		}
		if(recv_buf[4] == Open_Camera_Yes){
			printf("recv CMD:Open_Camera from serverv!\n");
			send_fun(&socket_fd);
		}else if(recv_buf[4] == Close_Camera_No){
			printf("recv CMD:Close_Camera from serverv!\n");
				
		}

		usleep(50000);
		printf("lalllalal!\n");

	}


	close(socket_fd);

	return 0; 
}

void send_fun(void *args)
{
	char tmp_buf[5]={0};
//	while(1){
	int ret = 0,i=0;
	int Length = 0;
	memset(tmp_buf,0,sizeof(tmp_buf));
#ifdef YUYV
	if(0 != pthread_mutex_lock(&convert_mutex)){
		printf("write cam_mutex locl fail!\n");
		return;
	}

	/*Length = my_jpeg.jpeg_size+9;
	

	memcpy(tmp_buf,(unsigned char *)&Length,4);
	tmp_buf[4] = (unsigned char)Open_Camera_Success;
	write(*(int*)args,tmp_buf,5);
	printf("send over!\n");*/
	
	
	
//	memcpy(my_jpeg.jpeg_buf,(char *)&Length,4);
//	my_jpeg.jpeg_buf[4] = (unsigned char)Open_Camera_Success;
//	ret = write(*(int *)args,my_jpeg.jpeg_buf,Length+4);
	
	Length = my_jpeg.jpeg_size+5;
	
	memcpy(tmp_buf,(unsigned char *)&Length,4);
	tmp_buf[4] = (unsigned char)Open_Camera_Success;
	write(*(int*)args,tmp_buf,5);

	memcpy(my_jpeg.jpeg_buf,(unsigned char *)&Length,4);
	my_jpeg.jpeg_buf[4] = (unsigned char)Open_Camera_Success;
	ret = write(*(int *)args,my_jpeg.jpeg_buf,Length+4);
	printf("send over buf_len:%d:!\n",Length+4);
	printf("ret:%d:!\n",ret);
	printf("jpeg_buf[4]:%x\n",my_jpeg.jpeg_buf[4]);



	if(0 != pthread_mutex_unlock(&convert_mutex)){
		printf("write cam_mutex locl fail!\n");
		return;
	}
#elif	JPEG
	
	if(0 != pthread_mutex_lock(&camera_mutex)){
		printf("write cam_mutex locl fail!\n");
		return;
	}
	
	Length = my_image.image_size+5;
	
	memcpy(tmp_buf,(unsigned char *)&Length,4);
	tmp_buf[4] = (unsigned char)Open_Camera_Success;
	write(*(int*)args,tmp_buf,5);

	memcpy(my_image.image_buf,(unsigned char *)&Length,4);
	my_image.image_buf[4] = (unsigned char)Open_Camera_Success;
	ret = write(*(int *)args,my_image.image_buf,Length+4);
	printf("send over buf_len:%d:!\n",Length+4);
	printf("ret:%d:!\n",ret);
	printf("image_buf[4]:%x\n",my_image.image_buf[4]);
	
	if(0 != pthread_mutex_unlock(&camera_mutex)){
		printf("write cam_mutex locl fail!\n");
		return;
	}
	
#endif
		
	//}
}
