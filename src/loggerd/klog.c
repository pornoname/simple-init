#include<stdlib.h>
#include<string.h>
#include<signal.h>
#include<linux/fs.h>
#include<sys/klog.h>
#include<sys/prctl.h>
#include<sys/select.h>
#include<sys/sysinfo.h>
#define TAG "klog"
#include"str.h"
#include"list.h"
#include"init.h"
#include"logger.h"
#include"system.h"
#include"proctitle.h"
#include"kloglevel.h"
#include"pathnames.h"
#include"logger_internal.h"

static bool run=true;
static int klogfd=-1;
static time_t boot_time=0;
struct log_item*read_kmsg_item(int fd,bool toff){
	char item[8192]={0},*level,*ktime,*content,*p0,*p1,*p2;
	struct log_item*b=malloc(sizeof(struct log_item));
	if(!b)return NULL;
	ssize_t s=read(fd,&item,8191);
	if(s<0){
		if(errno==EAGAIN)errno=0;
		free(b);
		return NULL;
	}

	// log level
	level=item;
	if(!(p0=strchr(level,',')))goto fail;
	p0[0]=0,p0++;
	b->level=klevel2level(parse_int(level,DEFAULT_KERN_LEVEL));

	if(!(ktime=strchr(p0,',')))goto fail;
	ktime++;

	// time
	if(!(p1=strchr(ktime,',')))goto fail;
	p1[0]=0,p1++;
	b->time=toff?(time_t)parse_long(ktime,0)/1000000+boot_time:time(NULL);

	if(!(content=strchr(p1,';')))goto fail;
	content++;

	// content
	if(!(p2=strchr(content,'\n')))goto fail;
	p2[0]=0,p2++;
	strncpy(b->content,content,sizeof(b->content)-1);

	strcpy(b->tag,"kernel");
	b->pid=0;

	return b;

	fail:
	free(b);
	EPRET(EINVAL);
}

static void kmsg_thread_exit(int p __attribute__((unused))){
	if(klogfd>=0)close(klogfd);
	if(logfd>=0){
		close_logfd();
		telog_warn("klog thread finished");
	}
	klogctl(SYSLOG_ACTION_CONSOLE_ON,NULL,0);
	run=false;
}

static int read_kmsg_thread(void*data __attribute__((unused))){
	close_all_fd((int[]){klogfd},1);
	open_socket_logfd_default();

	if(fcntl(klogfd,F_SETFL,0)<0)return terlog_error(-errno,"fcntl klog fd");
	if(lseek(klogfd,0,SEEK_END)<0)return terlog_error(-errno,"lseek klog fd");

	klogctl(SYSLOG_ACTION_CONSOLE_OFF,NULL,0);
	tlog_info("kernel log forwarder start with pid %d",getpid());
	setproctitle("klog");
	prctl(PR_SET_NAME,"Kernel Logger",0,0,0);
	handle_signals((int[]){SIGINT,SIGHUP,SIGQUIT,SIGTERM,SIGUSR1,SIGUSR2},6,kmsg_thread_exit);

	fd_set fs;
	struct log_item*item;
	struct timeval timeout={1,0};
	while(run){
		FD_ZERO(&fs);
		FD_SET(klogfd,&fs);
		FD_SET(logfd,&fs);
		int r=select(MAX(klogfd,logfd)+1,&fs,NULL,NULL,&timeout);
		if(r==-1){
			telog_error("select failed: %m");
			break;
		}else if(r==0)continue;
		else if(FD_ISSET(klogfd,&fs)){
			if((item=read_kmsg_item(klogfd,false))){
				logger_write(item);
				free(item);
				item=NULL;
			}
		}else if(FD_ISSET(logfd,&fs)){
			struct log_msg l;
			int x=logger_internal_read_msg(logfd,&l);
			if(x<0){
				close_logfd();
				break;
			}else if(x==0)continue;
			if(l.size>0)lseek(logfd,l.size,SEEK_CUR);
		}
	}
	kmsg_thread_exit(0);

	return errno;
}

int init_kmesg(){
	struct log_item*log=NULL;
	struct log_buff*buff=NULL;
	list*head=NULL,*conts=NULL,*item=NULL;

	struct sysinfo info;
	sysinfo(&info);
	boot_time=time(NULL)-(time_t)(info.uptime/1000);

	if(logbuffer&&!(head=list_first(logbuffer)))return -errno;
	if((klogfd=open(_PATH_DEV_KMSG,O_RDONLY|O_NONBLOCK))<0)goto fail;
	if(lseek(klogfd,0,SEEK_DATA)<0)goto fail;

	while((log=read_kmsg_item(klogfd,true))){
		if(!(buff=logger_internal_item2buff(log)))goto fail;
		if(!(item=list_new(buff)))goto fail;
		if(conts)list_add(conts,item);
		conts=item;
		item=NULL,log=NULL,buff=NULL;
	}

	if(logbuffer)list_insert(head,conts);
	else logbuffer=conts;
	conts=NULL;

	int x=fork_run("klog",false,NULL,NULL,read_kmsg_thread);
	close(klogfd);
	return x;
	fail:
	if(conts)list_free_all(conts,logger_internal_free_buff);
	if(buff)free(buff);
	if(log)free(log);
	if(klogfd>=0)close(klogfd);
	return -errno;
}

int level2klevel(enum log_level level){
	switch(level){
		case LEVEL_DEBUG:return   KERN_DEBUG;
		case LEVEL_INFO:return    KERN_INFO;
		case LEVEL_NOTICE:return  KERN_NOTICE;
		case LEVEL_WARNING:return KERN_WARNING;
		case LEVEL_ERROR:return   KERN_ERR;
		case LEVEL_CRIT:return    KERN_CRIT;
		case LEVEL_ALERT:return   KERN_ALERT;
		case LEVEL_EMERG:return   KERN_EMERG;
		default:return            KERN_NOTICE;
	}
}

enum log_level klevel2level(int level){
	switch(level){
		case KERN_DEBUG:return   LEVEL_DEBUG;
		case KERN_INFO:return    LEVEL_INFO;
		case KERN_NOTICE:return  LEVEL_NOTICE;
		case KERN_WARNING:return LEVEL_WARNING;
		case KERN_ERR:return     LEVEL_ERROR;
		case KERN_CRIT:return    LEVEL_CRIT;
		case KERN_ALERT:return   LEVEL_ALERT;
		case KERN_EMERG:return   LEVEL_EMERG;
		default:return          LEVEL_NOTICE;
	}
}
