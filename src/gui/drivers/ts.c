#define _GNU_SOURCE
#include<stdio.h>
#include<fcntl.h>
#include<errno.h>
#include<unistd.h>
#include<stddef.h>
#include<stdlib.h>
#include<stdbool.h>
#include<pthread.h>
#include<sys/epoll.h>
#include<sys/ioctl.h>
#include<linux/input.h>
#define TAG "input"
#include"logger.h"
#include"lvgl.h"
#include"gui.h"
struct in_data{
	bool enabled;
	int fd,flags,type;
	char path[64],name[256];
	lv_indev_drv_t indrv;
	lv_indev_t*indev;
	bool down;
	int16_t last_x,last_y,last_x_tmp,last_y_tmp;
	uint32_t key;
	char _pad[104];
};
static struct epoll_event*evs;
static struct in_data*indatas[32]={0};
static size_t
	es=sizeof(struct epoll_event),
	is=sizeof(struct input_event),
	ds=sizeof(struct in_data);
static pthread_t tsp=0;
static int efd=-1;
void*ts_handler(void*args __attribute__((unused))){
	for(;;){
		int r=epoll_wait(efd,evs,64,-1);
		if(r<0){
			if(errno==EINTR)continue;
			telog_error("epoll");
			break;
		}else for(int i=0;i<r;i++){
			struct in_data*d=evs[i].data.ptr;
			struct input_event event;
			ssize_t c=read(d->fd,&event,is);
			if(c<=0){
				telog_warn("read %s failed",d->path);
				epoll_ctl(efd,EPOLL_CTL_DEL,d->fd,NULL);
				close(d->fd);
				d->enabled=false;
			}
			if(!gui_sleep)switch(d->indrv.type){
				case LV_INDEV_TYPE_POINTER:switch(event.type){
					case EV_SYN:if(event.code==SYN_REPORT){
						if((d->flags&0x03)==0x03){
							d->flags=0x00;
							d->last_x=d->last_x_tmp;
							d->last_y=d->last_y_tmp;
							if(!d->down)d->down=true;
						}else d->down=false;
					}break;
					case EV_REL:switch(event.code){
						case REL_X:d->last_x_tmp+=event.value,d->flags|=0x01;break;
						case REL_Y:d->last_y_tmp+=event.value,d->flags|=0x02;break;
					}break;
					case EV_ABS:switch(event.code){
						case ABS_X:case ABS_MT_POSITION_X:d->last_x_tmp=event.value,d->flags|=0x01;break;
						case ABS_Y:case ABS_MT_POSITION_Y:d->last_y_tmp=event.value,d->flags|=0x02;break;
					}break;
				}break;
				case LV_INDEV_TYPE_KEYPAD:switch(event.type){
					case EV_KEY:
						d->down=event.value>0?LV_INDEV_STATE_PR:LV_INDEV_STATE_REL;
						switch(event.code){
							case KEY_POWER:d->key=LV_KEY_ENTER;break;
							case KEY_VOLUMEUP:d->key=LV_KEY_PREV;break;
							case KEY_VOLUMEDOWN:d->key=LV_KEY_NEXT;break;
							default:d->key=0,d->down=LV_INDEV_STATE_REL;break;
						}
					break;
					case EV_SW:;break;
				}break;
			}
			gui_quit_sleep();
		}
	}
	return NULL;
}
bool ts_read(lv_indev_drv_t*indev_drv,lv_indev_data_t*data){
	struct in_data*d=indev_drv->user_data;
	if(!d->enabled)return false;
	if(indev_drv->user_data==d)switch(indev_drv->type){
		case LV_INDEV_TYPE_POINTER:
			data->point.x=d->last_x;
			data->point.y=d->last_y;
			data->state=d->down?LV_INDEV_STATE_PR:LV_INDEV_STATE_REL;
		break;
		case LV_INDEV_TYPE_KEYPAD:
			data->key=d->key;
			data->state=d->down?LV_INDEV_STATE_PR:LV_INDEV_STATE_REL;
		break;
	}
	return false;
}
int init_epoll(){
	if(efd>=0)return 0;
	if((efd=epoll_create(64))<0){
		telog_error("epoll_create failed");
		return -1;
	}
	if(!(evs=malloc(es*64))){
		telog_error("malloc failed");
		close(efd);
		return -1;
	}
	memset(evs,0,es*64);
	return 0;
}
struct in_data*get_unused_in_data(){
	int x;
	struct in_data*d;
	for(x=0;x<32;x++){
		d=indatas[x];
		if(d){
			if(d->enabled)continue;
			memset(d,0,ds);
		}else{
			indatas[x]=d=malloc(ds);
			if(!d)telog_error("malloc failed");
		}
		return d;
	}
	telog_warn("too many input device open");
	return NULL;
}
int ts_init(char*dev,int fd){
	if(fd<0||!dev)return -1;
	bool support=false;
	unsigned char mask[EV_MAX/8+1];
	struct in_data*d=get_unused_in_data();
	if(!d)return -1;
	if(init_epoll()<0)return -1;
	lv_indev_drv_init(&d->indrv);
	if(ioctl(fd,EVIOCGNAME(255),d->name)<0){
		telog_warn("failed to %s ioctl EIOCGNAME",dev);
		strcpy(d->name,"unknown");
	}
	if(ioctl(fd,EVIOCGBIT(0,sizeof(mask)),mask)<0)telog_warn("failed to %s ioctl EVIOCGBIT",dev);
	else for(int j=0;j<EV_MAX;j++)if(mask[j/8]&(1<<(j%8)))switch(j){
		case EV_ABS:
			support=true;
			d->indrv.type=LV_INDEV_TYPE_POINTER;
		break;
		case EV_SW:
		case EV_KEY:
			support=true;
			d->indrv.type=LV_INDEV_TYPE_KEYPAD;
		break;
	}
	if(!support)return -1;
	tlog_debug("found input device %s (%s)",dev,d->name);
	d->indrv.read_cb=ts_read;
	d->indrv.user_data=d;
	d->indev=lv_indev_drv_register(&d->indrv);
	lv_indev_set_group(d->indev,gui_grp);
	strncpy(d->path,dev,63);
	d->fd=fd;
	d->enabled=true;
	errno=0;
	epoll_ctl(efd,EPOLL_CTL_ADD,d->fd,&(struct epoll_event){.events=EPOLLIN,.data.ptr=d});
	if(tsp!=0)return 0;
	tlog_info("starting input device thread");
	if(pthread_create(&tsp,NULL,ts_handler,NULL)!=0)telog_error("create thread failed");
	else pthread_setname_np(tsp,"TouchScreen Thread");
	return 0;
}
int ts_scan_init(void){
	tlog_info("probing input devices");
	bool found=false;
	char path[32]={0};
	int fd;
	for(int i=0;i<32;i++){
		memset(path,0,32);
		snprintf(path,31,_PATH_DEV"/input/event%d",i);
		if((fd=open(path,O_RDONLY|O_CLOEXEC))<0){
			if(errno!=ENOENT)telog_warn("failed to open %s",path);
			continue;
		}
		if(ts_init(path,fd)<0)close(fd);
		else found=true;
	}
	if(!found)tlog_error("no input devices found");
	return found?0:-1;
}
void ts_register(char*dev){ts_init(dev,open(dev,O_RDONLY|O_CLOEXEC));}
void ts_scan_register(void){ts_scan_init();}
