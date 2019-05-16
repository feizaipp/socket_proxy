/* 
 * rilimsproxy.c
 * zongpeng
 */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#define LOG_TAG "RILIMSPROXYD"
#define LOG_NDDEBUG 0
#include <utils/Log.h>
#include <cutils/container.h>
#include <sys/wait.h>
#include <private/android_filesystem_config.h>
#include "socketproxy.h"
#include <sys/stat.h>

#define VP1      "cell1"
#define VP2      "cell2"

static struct client_socket rild_ims_sock[2];
static struct server_socket rilp_ims_sock;
static struct cache_buf cb;
static struct cache_buf cb1;
static struct cache_buf cb2;

const char *rild_ims_sock_sub_dir1 = "/data/cells/cell1/dev/socket/qmux_radio";
const char *rild_ims_sock_sub_dir2 = "/data/cells/cell2/dev/socket/qmux_radio";
const char *rilp_ims_sock_sub_dir = "/dev/socket/qmux_radio";
const char *ims_debug_sock = "/dev/socket";
const char *ims_debug_sock_dir1 = "/data/cells/cell1/dev/socket";
const char *ims_debug_sock_dir2 = "/data/cells/cell2/dev/socket";
const char *ims_debug_sock_name = "ims_debug";

enum {
    RIL_CARD1 = 0,
    RIL_CARD2,
    RIL_CARD_NUM
};


const char *ril_ims_socket_name[RIL_CARD_NUM] = {
    "rild_ims0",
    "rild_ims1"
};

static struct socket_event s_commands_event1;
static struct socket_event s_listen_event1;
static struct socket_event s_commands_event2;
static struct socket_event s_listen_event2;
static struct socket_event s_connect_event;

static struct socket_event d_commands_event;
static struct socket_event d_listen_event;
static struct socket_event d_commands_event_cell1;
static struct socket_event d_listen_event_cell1;
static struct socket_event d_commands_event_cell2;
static struct socket_event d_listen_event_cell2;

static struct socket_event s_commands_event_inner;
static struct socket_event s_listen_event_inner;

static struct server_debug_info sdi;

static struct client_debug_info cdi[2];

static int i_conn_fd = 0;

static pthread_mutex_t i_Mutex_ims = PTHREAD_MUTEX_INITIALIZER;

struct socket_inner_info *sii_ims = NULL;

struct socket_inner_debug *sid_ims = NULL;

static void debug_disconnect(sdp *s_dp);
static void handle_inner_socket_recv_message_ims(int fd, short flags, void *param);

static void ril_ims_server_disconnect(scp *s_cp)
{
	if (s_cp->fd_connect > 0) {
        close(s_cp->fd_connect);
        s_cp->fd_connect = -1;
        socket_event_del(s_cp->connect_event);
        free_record_stream(s_cp->p_rs);
		if (s_cp->cache) {
			free(s_cp->cache->buf);
			s_cp->cache->buf = NULL;
			s_cp->cache->count = 0;
			s_cp->cache->offset = 0;
		}
		if (!rilp_ims_sock.wait)
			signal_to_connect_server();
    }
}

static void ril_ims_client_disconnect(slp *s_lp)
{
	if (s_lp->fd_command > 0) {
		close(s_lp->fd_command);
        s_lp->fd_command = -1;
		s_lp->accept = 0;
		s_lp->cache_send = 0;
        socket_event_del(s_lp->commands_event);
        free_record_stream(s_lp->p_rs);
        socket_event_add_wakeup(s_lp->listen_event);
		if (s_lp->cache) {
			free(s_lp->cache->buf);
			s_lp->cache->buf = NULL;
			s_lp->cache->count = 0;
			s_lp->cache->offset = 0;
		}
	}
}

static slp s_param_listen_socket_ril_ims1 = {
    -1,                         /* fdListen */
    -1,                         /* fdCommand */
	0,							/* accept */
	0,                          /* cache_send */
    NULL,                       /* processName */
    &s_commands_event1,         /* commands_event */
    &s_listen_event1,           /* listen_event */
    NULL,                       /* RecordStream */
    NULL,                       /* process_commands_callback */
    NULL,                       /* listen_client_callback */
    ril_ims_client_disconnect,  /* client_disconnect */
    &cb1,
    &cdi[0],
};

static slp s_param_listen_socket_ril_ims2 = {
    -1,                              /* fdListen */
    -1,                              /* fdCommand */
	0,							     /* accept */
	0,							     /* cache_send */
    NULL,                            /* processName */
    &s_commands_event2,              /* commands_event */
    &s_listen_event2,                /* listen_event */
    NULL,                            /* RecordStream */
    NULL,                            /* process_commands_callback */
    NULL,                            /* listen_client_callback */
    ril_ims_client_disconnect,       /* client_disconnect */
    &cb2,
    &cdi[1],
};


static scp s_param_connect_socket_ril_ims = {
    -1,                              /* fdConnect */
    NULL,                            /* processName */
    &s_connect_event,                /* connect_event */
    NULL,                            /* RecordStream */
    NULL,                            /* process_server_data_callback */
	ril_ims_server_disconnect,	     /* server_disconnect */
	&cb,						     /* cache_buf */
	&sdi,
};

static sdp s_param_debug_socket_ims = {
    -1,                                  /* fdListen */
    -1,                                  /* fdCommand */
    &d_commands_event,                   /* d_commands_event */
    &d_listen_event,                     /* d_listen_event */
    NULL,                                /* record_stream */
    NULL,                                /* process_commands_callback */
    NULL,                                /* listen_client_callback */
	debug_disconnect,	                 /* disconnect */
	&sdi,
	cdi,
};

static sdp s_param_debug_socket_ims_cell1 = {
    -1,                                  /* fdListen */
    -1,                                  /* fdCommand */
    &d_commands_event_cell1,             /* d_commands_event */
    &d_listen_event_cell1,               /* d_listen_event */
    NULL,                                /* record_stream */
    NULL,                                /* process_commands_callback */
    NULL,                                /* listen_client_callback */
	debug_disconnect,	                 /* disconnect */
	&sdi,
	cdi,
};

static sdp s_param_debug_socket_ims_cell2 = {
    -1,                                  /* fdListen */
    -1,                                  /* fdCommand */
    &d_commands_event_cell2,             /* d_commands_event */
    &d_listen_event_cell2,               /* d_listen_event */
    NULL,                                /* record_stream */
    NULL,                                /* process_commands_callback */
    NULL,                                /* listen_client_callback */
	debug_disconnect,	                 /* disconnect */
	&sdi,
	cdi,
};

static slp s_param_listen_socket_inner_ims = {
    -1,                                  /* fdListen */
    -1,                                  /* fdCommand */
	0,									 /* accept */
	0,							         /* cache_send */
    PHONE_PROCESS,                       /* processName */
    &s_commands_event_inner,             /* commands_event */
    &s_listen_event_inner,               /* listen_event */
    NULL,                                /* record_stream */
    handle_inner_socket_recv_message_ims,/* process_commands_callback */
    NULL,                                /* listen_client_callback */
	NULL,	                             /* client_disconnect */
	NULL,
	NULL,
};

static char cell1_ready[] = {'r', 'e', 'a', 'd', 'y', '-', 'c', 'e', 'l', 'l', '1'};
static char cell2_ready[] = {'r', 'e', 'a', 'd', 'y', '-', 'c', 'e', 'l', 'l', '2'};
static char host_debug[] =  {'h', 'o', 's', 't', '-', 'd', 'e', 'b', 'u', 'g', '0'};
static char cell1_debug[] = {'c', 'e', 'l', 'l', '1', '-', 'd', 'e', 'b', 'u', 'g'};
static char cell2_debug[] = {'c', 'e', 'l', 'l', '2', '-', 'd', 'e', 'b', 'u', 'g'};

static void handle_inner_socket_recv_message_ims(int fd, short flags, void *param)
{
	slp *s_lp = (slp *)param;
	int recv_fd = 0;
	char buf[16];
	
	flags = flags;

    RLOGV("%s fd:%d fd:%d", __func__, fd, s_lp->fd_command);

    recv_len_data(fd, (char *)buf, 11);
	buf[11] = '\0';
	
	recv_fd = recv_inner_fd(fd);
	RLOGD("%s %s:%d pid:%d", __func__, buf, recv_fd, getpid());
	if (recv_fd <= 0) {
		RLOGE("%s recv_fd:%d error strerr:%s", __func__, recv_fd, strerror(errno));
		return;
	}

	if (!strncmp(buf, cell1_ready, sizeof(cell1_ready))) {
		rild_ims_sock[0].s_lp->fd_listen = recv_fd;
	    rild_ims_sock[0].s_sock = &rilp_ims_sock;
		start_listen_client(&rild_ims_sock[0]);
	} else if (!strncmp(buf, cell2_ready, sizeof(cell2_ready))) {
		rild_ims_sock[1].s_lp->fd_listen = recv_fd;
	    rild_ims_sock[1].s_sock = &rilp_ims_sock;
		start_listen_client(&rild_ims_sock[1]);
	} else if (!strncmp(buf, host_debug, sizeof(host_debug))) {
		s_param_debug_socket_ims.fd_listen = recv_fd;
		start_listen_debug(&s_param_debug_socket_ims);
	} else if (!strncmp(buf, cell1_debug, sizeof(cell1_debug))) {
		s_param_debug_socket_ims_cell1.fd_listen = recv_fd;
		start_listen_debug(&s_param_debug_socket_ims_cell1);
	} else if (!strncmp(buf, cell2_debug, sizeof(cell2_debug))) {
		s_param_debug_socket_ims_cell2.fd_listen = recv_fd;
		start_listen_debug(&s_param_debug_socket_ims_cell2);
	}
}

static void debug_disconnect(sdp *s_dp)
{
	if (s_dp->fd_command > 0) {
		close(s_dp->fd_command);
        s_dp->fd_command = -1;
        socket_event_del(s_dp->d_commands_event);
        free_record_stream(s_dp->p_rs);
		RLOGV("%s", __func__);
	}
}

static int init_ril_ims(int ril_index, int fission_mode)
{
	int ret = 0;

    rilp_ims_sock.type = RILD_IMS;
    rilp_ims_sock.index = ril_index;
	rilp_ims_sock.wait = 0;
    rilp_ims_sock.sub_dir = rilp_ims_sock_sub_dir;
    rilp_ims_sock.sock_name = ril_ims_socket_name;
    rilp_ims_sock.s_cp = &s_param_connect_socket_ril_ims;
	rilp_ims_sock.s_cp->sdi->pid = getpid();

    rild_ims_sock[0].index = ril_index;
    rild_ims_sock[0].sock_name = ril_ims_socket_name;
    rild_ims_sock[0].sub_dir = rild_ims_sock_sub_dir1;
    rild_ims_sock[0].s_lp = &s_param_listen_socket_ril_ims1;
	rild_ims_sock[0].fission_mode = fission_mode;
    
    rild_ims_sock[1].index = ril_index;
    rild_ims_sock[1].sock_name = ril_ims_socket_name;
    rild_ims_sock[1].sub_dir = rild_ims_sock_sub_dir2;
    rild_ims_sock[1].s_lp = &s_param_listen_socket_ril_ims2;
	rild_ims_sock[1].fission_mode = fission_mode;

	sii_ims = (struct socket_inner_info *)calloc(2, sizeof(struct socket_inner_info));
	if (sii_ims == NULL) {
		RLOGE("calloc socket_inner_info error!");
		ret = -1;
		goto err;
	}
	sid_ims = (struct socket_inner_debug *)calloc(3, sizeof(struct socket_inner_debug));
	if (sid_ims == NULL) {
		RLOGE("calloc socket_inner_debug error!");
		ret = -1;
		goto free_sii;
	}
	return ret;
free_sii:
	free(sii_ims);
err:
	return ret;
}

static void *_create_socket_for_ril_ims(void *args)
{
	struct socket_inner_info *si = (struct socket_inner_info *)args;
	int fd;
	int ret = 0;

	thread_started();
	fd = create_client_socket_by_path(si->c_sock, si->s_sock, si->perm, si->uid, si->gid, si->fission_mode);

	pthread_mutex_lock(&i_Mutex_ims);
	/* notify child process */
	if (si->trigger_cmd == CELL1_RIL) {
		ret = send_buf_to_socket(i_conn_fd, cell1_ready, sizeof(cell1_ready));
		RLOGV("%s send_buf %d index:%d fd:%d i_conn_fd:%d", __func__, ret, si->trigger_cmd, fd, i_conn_fd);
	} else if (si->trigger_cmd == CELL2_RIL) {
		ret = send_buf_to_socket(i_conn_fd, cell2_ready, sizeof(cell2_ready));
		RLOGV("%s send_buf %d index:%d fd:%d i_conn_fd:%d", __func__, ret, si->trigger_cmd, fd, i_conn_fd);
	}

	if (ret) {
		RLOGE("%s %d send_buf_to_socket error!", __func__, __LINE__);
		pthread_mutex_unlock(&i_Mutex_ims);
		return (void *)-1;
	}
	send_inner_fd(i_conn_fd, fd);
	pthread_mutex_unlock(&i_Mutex_ims);
	return (void *)0;
}

static void create_socket_for_ril_ims(int fission_mode)
{
    if (fission_mode == FISSION_MODE_DOUBLE) {
		sii_ims[0].c_sock = &rild_ims_sock[0];
		sii_ims[0].s_sock = &rilp_ims_sock;
		sii_ims[0].perm = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP;
		sii_ims[0].uid = AID_ROOT;
		sii_ims[0].gid = AID_RADIO;
		sii_ims[0].fission_mode = fission_mode;
		sii_ims[0].trigger_cmd = CELL1_RIL;
		_create_pthread(_create_socket_for_ril_ims, (void *)&sii_ims[0]);
    }
	sii_ims[1].c_sock = &rild_ims_sock[1];
	sii_ims[1].s_sock = &rilp_ims_sock;
	sii_ims[1].perm = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP;
	sii_ims[1].uid = AID_ROOT;
	sii_ims[1].gid = AID_RADIO;
	sii_ims[1].fission_mode = fission_mode;
	sii_ims[1].trigger_cmd = CELL2_RIL;
	_create_pthread(_create_socket_for_ril_ims, (void *)&sii_ims[1]);
	RLOGV("%s %d", __func__, __LINE__);
}

static void connect_listen_rilp_ims_socket(int fission_mode)
{
    connect_server_socket(&rilp_ims_sock, rild_ims_sock, fission_mode);
}

static void *__create_socket_for_ims_debug(void *args)
{
	struct socket_inner_debug *si = (struct socket_inner_debug *)args;
	int fd;
	int ret = 0;

	thread_started();
	fd = create_socket_for_debug(si->path, si->name, si->index, si->perm, si->uid, si->gid);
	RLOGV("%s %d fd:%d!", __func__, __LINE__, fd);

	pthread_mutex_lock(&i_Mutex_ims);
	/* notify child process */
	if (si->trigger_cmd == HOST_DEBUG) {
		ret = send_buf_to_socket(i_conn_fd, host_debug, sizeof(host_debug));
		RLOGV("%s send_buf %d index:%d fd:%d i_conn_fd:%d", __func__, ret, si->trigger_cmd, fd, i_conn_fd);
	} else if (si->trigger_cmd == CELL1_DEBUG) {
		ret = send_buf_to_socket(i_conn_fd, cell1_debug, sizeof(cell1_debug));
		RLOGV("%s send_buf %d index:%d fd:%d i_conn_fd:%d", __func__, ret, si->trigger_cmd, fd, i_conn_fd);
	} else if (si->trigger_cmd == CELL2_DEBUG) {
		ret = send_buf_to_socket(i_conn_fd, cell2_debug, sizeof(cell2_debug));
		RLOGV("%s send_buf %d index:%d fd:%d i_conn_fd:%d", __func__, ret, si->trigger_cmd, fd, i_conn_fd);
	}

	if (ret) {
		RLOGE("%s %d send_buf_to_socket error!", __func__, __LINE__);
		pthread_mutex_unlock(&i_Mutex_ims);
		return (void *)-1;
	}
	send_inner_fd(i_conn_fd, fd);
	pthread_mutex_unlock(&i_Mutex_ims);
	return (void *)0;
}

static void _create_socket_for_ims_debug(int fission_mode, int ril_index)
{
	if (fission_mode == FISSION_MODE_DOUBLE) {
		sid_ims[1].path = ims_debug_sock_dir1;
		sid_ims[1].name = ims_debug_sock_name;
		sid_ims[1].perm = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
		sid_ims[1].uid = AID_ROOT;
		sid_ims[1].gid = AID_RADIO;
		sid_ims[1].index = ril_index;
		sid_ims[1].trigger_cmd = CELL1_DEBUG;
		_create_pthread(__create_socket_for_ims_debug, (void *)&sid_ims[1]);
	}

	sid_ims[2].path = ims_debug_sock_dir2;
	sid_ims[2].name = ims_debug_sock_name;
	sid_ims[2].perm = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
	sid_ims[2].uid = AID_ROOT;
	sid_ims[2].gid = AID_RADIO;
	sid_ims[2].index = ril_index;
	sid_ims[2].trigger_cmd = CELL2_DEBUG;
	_create_pthread(__create_socket_for_ims_debug, (void *)&sid_ims[2]);

	sid_ims[0].path = ims_debug_sock;
	sid_ims[0].name = ims_debug_sock_name;
	sid_ims[0].perm = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
	sid_ims[0].uid = AID_ROOT;
	sid_ims[0].gid = AID_RADIO;
	sid_ims[0].index = ril_index;
	sid_ims[0].trigger_cmd = HOST_DEBUG;
	_create_pthread(__create_socket_for_ims_debug, (void *)&sid_ims[0]);
	RLOGV("%s %d", __func__, __LINE__);
}

int main(int argc, char **argv)
{
	int fission_mode = get_fission_mode();
	unsigned int ril_index = 0;
	pid_t pid = 0;
	int ret = 0;

	if (argc < 2) {
		RLOGE("rilimsproxy input error!\n");
		return -1;
	}

	ril_index = atoi(argv[1]);
	if (ril_index >= RIL_CARD_NUM) {
		RLOGE("rilimsproxy input error! ril_index = %d\n", ril_index);
		return -1;
	}

	RLOGD("rilimsproxy ril_index = %d\n", ril_index);

	ret = init_ril_ims(ril_index, fission_mode);
	if (ret < 0) {
		RLOGE("init_ril_ims error");
		return -1;
	}
	handle_sigchld_sig();

	pid = fork();
	if (pid < 0) {
		RLOGE("failed to fork.");
		exit(EXIT_FAILURE);
	} else if (pid == 0) {
		s_param_listen_socket_inner_ims.fd_listen = create_inner_socket(ril_index, "inner_ims");
		setuid(AID_RADIO);
		create_pthread();
		start_listen_inner(&s_param_listen_socket_inner_ims);
		connect_listen_rilp_ims_socket(fission_mode);
		if (!rilp_ims_sock.wait)
			signal_to_connect_server();
		RLOGD("rilproxyd child process starting sleep loop");
		while (1) {
			sleep(UINT32_MAX);
		}
	} else if (pid > 0) {
		RLOGD("father process");
		set_parent_pid(pid);
		i_conn_fd = connect_inner_socket(ril_index, "inner_ims");
		create_socket_for_ril_ims(fission_mode);
		_create_socket_for_ims_debug(fission_mode, ril_index);
		while (1) {
			sleep(UINT32_MAX);
		}
	}

	return 0;
}

