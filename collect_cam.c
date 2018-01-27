/********************************************************
*   Copyright (C) 2016 All rights reserved.
*   
*   Filename:myserver_cam.c
*   Author  :Lematin
*   Date    :2017-2-17
*   Describe:
*
********************************************************/
#include<stdio.h>
#include<string.h>
#include<strings.h>
#include<stdlib.h>
#include<fcntl.h>
#include<unistd.h>
#include<dirent.h>
#include<sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/fb.h>
#include <pthread.h>
#include "collect_cam.h"

typedef unsigned long UL;
typedef unsigned int U32;
typedef unsigned short U16;
typedef unsigned char U8;

/* v4l2相关信息的设置  自定义的结构体  主要是为了方便设置相关信息*/
struct videobuffer{      
                // 内核自用的映射地址（主要包含图片的采集信息）
                //映射缓冲帧 
    void *start;    //首地址
    UL length;      //空间长度
};



struct v4l_info{    
    int v4l_num;                         //向内核申请缓冲帧个数
    struct v4l2_buffer v4l2_buf;            //v4l2_buffer(获取缓冲帧)
                    //包含缓冲帧的信息（v4l2文档中的原结构体）
    struct videobuffer *video_buf;   //mmap video_buf的信息
    int wide;                        //默认 320
    int high;                        //     240
    UL size;                         // 大小
};
/* camera 相关结构体  */
struct camera{
    int fd;                         //描述符
    char dev_name[20];              //设备名（默认 /dev/video0）
    struct v4l_info v4l_info;       //v4l相关结构体
    int (*get_capture_raw_data)(struct camera *dev,void *buf);//获取图片
    int (*exit)(struct camera *dev);//退出camera设备
};


// 打开camera设备 111111111
static int camera_init(struct camera *dev)
{
    int ret;
    dev->fd = open(dev->dev_name,O_RDWR);
    if(dev->fd == -1){
        dev->fd = open(dev->dev_name,O_RDWR);
        if(dev->fd == -1){
            perror("open camera is error!");
            close(dev->fd);
           // exit(EXIT_FAILURE);
	    return -1;
        }
    }
    /* 查询设备所支持的格式 */
    struct v4l2_fmtdesc fmtdesc;    //这个结构体包含一些格式信息
    memset(&fmtdesc,0,sizeof(struct v4l2_fmtdesc)); //先清空
    fmtdesc.index = 0;   //代表要查询的格式序号从0开始
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                //设置数据流类型，必须永远是V4L2_BUF_TYPE_VIDEO_CAPTURE
    while((ret = ioctl(dev->fd,VIDIOC_ENUM_FMT,&fmtdesc)) == 0){
        //VIDIOC_ENUM_FMT 获取当前视频设备支持的视频格式
        fmtdesc.index ++;
#ifdef DEBUG       
        printf("{ pixelformat = %c%c%c%c, description = %s",\
                fmtdesc.pixelformat & 0xFF,\
                (fmtdesc.pixelformat >> 8) & 0xFF,\
                (fmtdesc.pixelformat >> 16) & 0xFF,\
                (fmtdesc.pixelformat >> 24) & 0xFF, fmtdesc.description);
#endif                
        // pixelformat 是格式 
        // description 是格式名称
    }
    struct v4l2_capability cap;
    //v4l2_capability结构体描述v里相关能力
    if(ioctl(dev->fd,VIDIOC_QUERYCAP,&cap) < 0){
        //VIDIOC_QUERYCAP 是查询驱动功能
        perror("VIDIOC_QUERYCAP failed\n");
        close(dev->fd);
        //exit(EXIT_FAILURE);
	return -1;
    }
    if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)){
        perror("V4L2_CAP_VIDEO_CAPTURE failed\n");
        close(dev->fd);
        //exit(EXIT_FAILURE);
	return -1;
        //capabilities是设备支持的操作
        //常用值：V4L2_CAP_VIDEO_CAPTURE 代表是否支持图片获取
    }
    if(!(cap.capabilities & V4L2_CAP_STREAMING)){
        perror("V4L2_CAP_STREAMING failed\n");
        close(dev->fd);
        //exit(EXIT_FAILURE);
	return -1;
        //是否支持视频流
    }
    //设置采集格式，先设置成V4L2_PIX_FMT_MJPEG
    struct v4l2_format fmt;
    //v4l2_format结构体 帧的格式 大小等
    memset(&fmt,0,sizeof(fmt)); 
    //结构体清零，设置属性！
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    //选用图片类型 
    fmt.fmt.pix.width = dev->v4l_info.wide;
    //pix也是个结构体，里面是设置帧宽，帧高，等
    fmt.fmt.pix.height = dev->v4l_info.high; 

    //fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.pixelformat = MY_CAMERA_FMAT;
    //帧的格式，采用JPEG的格式进行采集
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
    //设置扫描方式 交错的

    if(ioctl(dev->fd, VIDIOC_S_FMT, &fmt) == -1){
        // VIDIOC_S_FMT 是设置当前驱动的频捕获格式
        //若不支持若不支持V4L2_PIX_FMT_MJPEG， 则设置成 V4L2_PIX_FMT_YUYV
        perror( "ioctl : VIDIOC_S_FMT failed\n" );
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        //重新设置采集格式 V4L2_PIX_FMT_YUYV
        fmt.fmt.pix.field = V4L2_FIELD_INTERLACED; 
        if (ioctl(dev->fd, VIDIOC_S_FMT, &fmt) == -1){
            perror("ioctl ：VIDIOC_S_FMT filed\n");
            close(dev->fd);
            //exit(EXIT_FAILURE);
	    return -1;
        }
#ifdef DEBUG        
        DBUG_WAR("you want set camera :V4L2_PIX_FMT_MJPEG \n");
        DBUG_WAR("formatbut :set camera capture true format:"\
                 "v4L2_PIX_FMT_YUYV \n");
#endif                 
    }
    if(fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_MJPEG){
#ifdef DEBUG    
        DBUG_INFO("set camera V4L2_PIX_FMT_MJPEG  success\n");
#endif        
    }
    //得到摄像头采集的图片的真实分辨率，若与之前不同，及时纠正
    if((dev->v4l_info.wide != fmt.fmt.pix.width) || \
        (dev->v4l_info.high != fmt.fmt.pix.height)){
#ifdef DEBUG        
        DBUG_WAR("the camera pix is: W %d , H %d\n",\
                dev->v4l_info.wide,dev->v4l_info.high);
#endif                
        dev->v4l_info.wide = fmt.fmt.pix.width;
        dev->v4l_info.high = fmt.fmt.pix.height;
#ifdef DEBUG        
        DBUG_WAR("but camera pix is: W %d , H %d\n",\
                dev->v4l_info.wide,dev->v4l_info.high);
#endif                
    }
    
    else{
#ifdef DEBUG     
        DBUG_INFO("the camera pix is : W %d , H %d\n",\
                dev->v4l_info.wide,dev->v4l_info.high); 
#endif                   
    }
    
    if(fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV || \
        fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_MJPEG){
        dev->v4l_info.size=fmt.fmt.pix.width * fmt.fmt.pix.height*2;
    }
    
    else if(fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_NV12 || \
            fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_NV21){
        dev->v4l_info.size=fmt.fmt.pix.width * fmt.fmt.pix.height*3/2; 
    }
    //申请缓存区，用来存放摄像头采集好的图像帧
    struct v4l2_requestbuffers reqbuff;
    //v4l2_requestbuffers结构体是向设备申请缓冲区
    reqbuff.count =dev->v4l_info.v4l_num;
    // count 是设置缓冲区中缓冲帧数目
    reqbuff.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    //type 设置缓冲帧数据格式   数据流类型，
    reqbuff.memory = V4L2_MEMORY_MMAP;
    //memory区别是内存映射还是用户指针方式
    //V4L2_MEMORY_MMAP  或 V4L2_MEMORY_USERPTR
    if(ioctl(dev->fd, VIDIOC_REQBUFS, &reqbuff) < 0){
        //VIDIOC_REQBUFS 是分配内存
#ifdef DEBUG        
        DBUG_ERR("ioctl : VIDIOC_REQBUFS failed\n");
#endif        
        close(dev->fd);
        //exit(EXIT_FAILURE);
        return -1;
    }
    dev->v4l_info.v4l_num = reqbuff.count;
#ifdef DEBUG     
    DBUG_INFO("request the v4l reqbuff is %d\n",dev->v4l_info.v4l_num);
#endif
    //mmap
    dev->v4l_info.video_buf = calloc(reqbuff.count ,\
                                     sizeof(*dev->v4l_info.video_buf) );
    //开辟空间
    if(dev->v4l_info.video_buf == NULL){
#ifdef DEBUG    
        DBUG_ERR( "video calloc  buffers failed\n" );
#endif           
        close(dev->fd);
        //exit(EXIT_FAILURE);
	return -1;
    }
    
    int numBufs;
    //把申请好的图片缓存区，映射到用户空间，方便程序开发者使用
    for (numBufs = 0; numBufs < reqbuff.count; numBufs++){    
        memset(&dev->v4l_info.v4l2_buf, 0, sizeof(struct v4l2_buffer));
        //将缓冲帧的结构体清零，再赋值
        dev->v4l_info.v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        //采集类型格式，
        dev->v4l_info.v4l2_buf.memory = V4L2_MEMORY_MMAP; 
        //选择方式
        dev->v4l_info.v4l2_buf.index = numBufs;
        //设置序号
        if (ioctl(dev->fd, VIDIOC_QUERYBUF, &dev->v4l_info.v4l2_buf)<0){
            //VIDIOC_QUERYBUF是将VIDIOC_REQBUFS分配的缓存转为物理地址
#ifdef DEBUG            
            DBUG_ERR( "video init VIDIOC_QUERYBUF error\n" );
#endif            
            close(dev->fd);
            //exit(EXIT_FAILURE);
	    return -1;
        }
        dev->v4l_info.video_buf[numBufs].length = \
        dev->v4l_info.v4l2_buf.length;
        //开辟的空间的长度，v4l2_buf缓冲帧的信息length代表缓冲帧长度
        dev->v4l_info.video_buf[numBufs].start = \
        mmap(NULL, dev->v4l_info.v4l2_buf.length, \
            PROT_READ | PROT_WRITE,MAP_SHARED,\
            dev->fd, dev->v4l_info.v4l2_buf.m.offset);
			//// offset 缓冲帧地址，只对MMAP 有效
        //地址映射
        if (dev->v4l_info.video_buf[numBufs].start == MAP_FAILED){ 
#ifdef DEBUG               
            DBUG_ERR( "video init mmap failed\n" );
#endif            
            close(dev->fd);
            //exit(EXIT_FAILURE);
	    return -1;
        }
        if (ioctl(dev->fd, VIDIOC_QBUF, &dev->v4l_info.v4l2_buf) <0){
            //VIDIOC_QBUF 是空的缓冲帧放入队列
#ifdef DEBUG            
            DBUG_ERR( "video init ioctl VIDIOC_QBUF failed\n" );
#endif            
            close(dev->fd);
            //exit(EXIT_FAILURE);
	    return -1;
        }
    }
#ifdef DEBUG     
    DBUG_INFO("camera init success\n");
#endif    
    return 0;
}
//允许摄像头采集222222222222
static int camera_start(struct camera * dev)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        //设置格式 数据流
    if(ioctl(dev->fd, VIDIOC_STREAMON, &type) == -1 ){
        //VIDIOC_STREAMON开始视频显示函数
#ifdef DEBUG        
        DBUG_ERR("ioctl stream start failed\n");
#endif        
        return -1;
            
    }
#ifdef DEBUG     
    DBUG_INFO("start camera success\n");
#endif    
    return 0;

}

//停止摄像头采集333333333333
static int camera_stop(struct camera * dev)
{
    enum v4l2_buf_type  type= V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int ret = ioctl(dev->fd, VIDIOC_STREAMOFF, &type);
    //VIDIOC_STREAMOFF结束视频显示函数
        if(ret){ 
#ifdef DEBUG        
                DBUG_ERR("ioctl stream stop failed\n");
#endif                
                return -1;
            }
#ifdef DEBUG              
    DBUG_INFO("stop the  camera \n");
#endif    
    return 0;

}

//关闭设备44444444444
static int camera_exit(struct camera * dev)
{
    int numBufs ;
    for(numBufs = 0;numBufs < dev->v4l_info.v4l_num; numBufs++ )
        munmap(dev->v4l_info.video_buf[numBufs].start, \
                   dev->v4l_info.video_buf[numBufs].length);
    //断开映射
    free(dev->v4l_info.video_buf);
    close(dev->fd);
#ifdef DEBUG    
    DBUG_INFO("camera closed...\n");
#endif    
    return 0;
}

//得到摄像头采集的一帧图像5555555
static  int dev_capture(struct camera * dev,void *buf)
{
    if(ioctl(dev->fd, VIDIOC_DQBUF, &dev->v4l_info.v4l2_buf) == -1){
        //VIDIOC_DQBUF将含有数据的缓冲帧从队列中取出
#ifdef DEBUG
        DBUG_ERR("failed:get RAW data ioctl: VIDIOC_DQBUF\n");
#endif        
        return -1;
    }
    memcpy(buf,dev->v4l_info.video_buf[dev->v4l_info.v4l2_buf.index].start,\
           dev->v4l_info.v4l2_buf.bytesused);
    //bytesused  buffer中已经使用的字节数

    if (ioctl(dev->fd, VIDIOC_QBUF, &dev->v4l_info.v4l2_buf) == -1){
        //将空的缓冲帧放入队列
#ifdef DEBUG        
        DBUG_ERR("failed:get RAW data ioctl: VIDIOC_QBUF\n");
#endif        
        return -1;
    }
    return dev->v4l_info.v4l2_buf.bytesused;
    //返回使用的字节数
}
//6666666666666666666
int camera_info_init(struct camera * dev,char *dev_path){
	printf("***********************dev_path:%s\n",dev_path);
    //camera 结构体是  摄像头设备结构体，进行数据配置
    if(strlen(dev->dev_name) <= 0 )
        strncpy(dev->dev_name,dev_path,strlen(dev_path)+1);
        //设备名字的设定
    if(dev->v4l_info.wide <= 0 )
        dev->v4l_info.wide = 320;
        //采集的宽度和高度设定
    if(dev->v4l_info.high <=0 )
        dev->v4l_info.high =240;
    if(dev->v4l_info.v4l_num<=0)
        dev->v4l_info.v4l_num =4;
        //缓冲帧个数
    if(NULL == dev->exit)
        dev->exit = &camera_exit;
        //给函数指针赋值，设置关闭设备函数
    if(NULL == dev->get_capture_raw_data)
        dev->get_capture_raw_data = &dev_capture;
        //函数指针赋值，采集图片函数；
    if(camera_init(dev) <0)//打开设备
        return -1;
    return camera_start(dev) ; //开始采集

}

//设备信息节点
static struct camera  dev;
//设备结构体7777777777777
static  int dev_camera_init(char *dev_path)//调用设备打开函数
{
    dev.v4l_info.wide=MY_WIDE;
    dev.v4l_info.high=MY_HIGH;
    int ret = camera_info_init(&dev,dev_path);//设备打开
    if(ret == -1){
        printf("camera_info_init failed\n");
        return ret;
    }
    return 0;
}

//7777777
int dev_camera_capture(char *buf){
    
    return  dev.get_capture_raw_data(&dev,buf);
    //图片采集
}


void *pthread_camera_fun(void *arg)
{
	
	while(1){
		
#ifdef YUYV		
		if (0 > sem_wait(&sem_camera_empty)) {
			perror("sem_wait");
			pthread_exit("pthread_exit unnormally!");
		}
		
		my_image.image_size = dev_camera_capture(my_image.image_buf);
#ifdef DEBUG
		printf("my_image.image_size:%d\n",my_image.image_size);
#endif				
		if (0 > sem_post(&sem_camera_full)) {
			perror("sem_post");
			pthread_exit("pthread_exit unnormally!");
		}
		
#elif JPEG
		if(0 != pthread_mutex_lock(&camera_mutex)){
	        printf("uart_read_mutex lock fail!\n");
		break;
        	}
		my_image.image_size = dev_camera_capture(&my_image.image_buf[9]);
		memcpy(&my_image.image_buf[5],&my_image.image_size,4);
		
#ifdef DEBUG
	//	printf("my_image.image_size:%d\n",my_image.image_size);
#endif		
		if(0 != pthread_mutex_unlock(&camera_mutex)){
	        printf("uart_read_mutex lock fail!\n");
		break;
        	}		

		usleep(10000);

#endif		
		
	
	}
}


int mycamera_init(char **dev_path)
{
	
        if(0>dev_camera_init(*dev_path)){
        	return -1;
        }
        	 
    	pthread_t pthread_camera;
#ifdef YUYV    	
	if (0 > sem_init(&sem_camera_full, 0, 0)) {
		perror("sem_init1");
		return -1;
	}
        
        if (0 > sem_init(&sem_camera_empty, 0, 1)) {
		perror("sem_init1");
		return -1;
	}
#elif JPEG
	if(0 != pthread_mutex_init(&camera_mutex,NULL)){
		printf("uart_read_mutex error!\n");
		return -1;
	}
#endif
        
	if(0 != pthread_create(&pthread_camera,NULL,pthread_camera_fun,NULL)){
		printf("pthread_create camera error!\n");
		return -1;
	}
#ifdef DEBUG	
        printf("camera_pthread start...!\n");
#endif
	return 0;
}

