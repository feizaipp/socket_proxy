/* 
 * rilulproxy.c
 * zongpeng
 */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#define LOG_TAG "RILULROXYD"
#define LOG_NDDEBUG 0
#include <utils/Log.h>
#include <cutils/container.h>
#include <sys/wait.h>
#include <private/android_filesystem_config.h>
#include "socketproxy.h"
#include <sys/stat.h>

#define VP1      "cell1"
#define VP2      "cell2"
	 
static struct client_socket rild_ul_sock[2];
static struct server_socket rilp_ul_sock;

const char *rild_ul_sock_sub_dir1 = "/data/cells/cell1/dev/socket/qmux_radio";
const char *rild_ul_sock_sub_dir2 = "/data/cells/cell2/dev/socket/qmux_radio";
const char *rilp_ul_sock_sub_dir = "/dev/socket/qmux_radio";
const char *ul_debug_sock = "/dev/socket";
const char *ul_debug_sock_dir1 = "/data/cells/cell1/dev/socket";
const char *ul_debug_sock_dir2 = "/data/cells/cell2/dev/socket";
const char *ul_debug_sock_name = "ul_debug";

enum {
    RIL_CARD1 = 0,
    RIL_CARD2,
    RIL_CARD_NUM
};


const char *ril_ul_socket_name[RIL_CARD_NUM] = {
    "uim_lpa_socket0",
    "uim_lpa_socket1"
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

static struct server_debug_info sdi;

static struct client_debug_info cdi[2];

static void debug_disconnect(sdp *s_dp);

static void ril_ul_server_disconnect(scp *s_cp)
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
		if (!rilp_ul_sock.wait)
			signal_to_connect_server();
    }
}

static void ril_ul_client_disconnect(slp *s_lp)
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


static slp s_param_listen_socket_ril_ul1 = {
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
    ril_ul_client_disconnect,  /* client_disconnect */
    NULL,
	&cdi[0],
};

static slp s_param_listen_socket_ril_ul2 = {
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
    ril_ul_client_disconnect,       /* client_disconnect */
    NULL,
	&cdi[1],
};


static scp s_param_connect_socket_ril_ul = {
    -1,                              /* fdConnect */
    NULL,                            /* processName */
    &s_connect_event,                /* connect_event */
    NULL,                            /* RecordStream */
    NULL,                            /* process_server_data_callback */
	ril_ul_server_disconnect,	     /* server_disconnect */
	NULL,						     /* cache_buf */
	&sdi,
};

static sdp s_param_debug_socket_ul = {
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

static sdp s_param_debug_socket_ul_cell1 = {
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

static sdp s_param_debug_socket_ul_cell2 = {
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

static void init_ril_ul(int ril_index)
{
    rilp_ul_sock.type = UIM_LPA_SOCKET;
    rilp_ul_sock.index = ril_index;
	rilp_ul_sock.wait = 0;
    rilp_ul_sock.sub_dir = rilp_ul_sock_sub_dir;
    rilp_ul_sock.sock_name = ril_ul_socket_name;
    rilp_ul_sock.s_cp = &s_param_connect_socket_ril_ul;
	rilp_ul_sock.s_cp->sdi->pid = getpid();

    rild_ul_sock[0].index = ril_index;
    rild_ul_sock[0].sock_name = ril_ul_socket_name;
    rild_ul_sock[0].sub_dir = rild_ul_sock_sub_dir1;
    rild_ul_sock[0].s_lp = &s_param_listen_socket_ril_ul1;
    
    rild_ul_sock[1].index = ril_index;
    rild_ul_sock[1].sock_name = ril_ul_socket_name;
    rild_ul_sock[1].sub_dir = rild_ul_sock_sub_dir2;
    rild_ul_sock[1].s_lp = &s_param_listen_socket_ril_ul2;
}

static void create_socket_for_ril_ul(int fission_mode)
{
    if (fission_mode == FISSION_MODE_DOUBLE) {
        create_client_socket_by_path(&rild_ul_sock[0], &rilp_ul_sock, 0, 
                                                AID_RADIO, AID_RADIO, fission_mode);
    }
    create_client_socket_by_path(&rild_ul_sock[1], &rilp_ul_sock, 0, 
                                            AID_RADIO, AID_RADIO, fission_mode);

}

static void start_listen_rild_ul_client(int fission_mode)
{
    if (fission_mode == FISSION_MODE_DOUBLE) {
        start_listen_client(&rild_ul_sock[0]);
    }
    start_listen_client(&rild_ul_sock[1]);
}

static void connect_listen_rilp_ul_socket(int fission_mode)
{
    connect_server_socket(&rilp_ul_sock, rild_ul_sock, fission_mode);
}


int main(int argc, char **argv)
{
	int fission_mode = get_fission_mode();
	unsigned int ril_index = 0;

	if (argc < 2) {
		RLOGE("rilulproxy input error!\n");
		return -1;
	}

	ril_index = atoi(argv[1]);
	if (ril_index >= RIL_CARD_NUM) {
		RLOGE("rilulproxy input error! ril_index = %d\n", ril_index);
		return -1;
	}

	RLOGD("rilulproxy ril_index = %d\n", ril_index);

	init_ril_ul(ril_index);

	create_socket_for_ril_ul(fission_mode);
	s_param_debug_socket_ul.fd_listen = create_socket_for_debug(ul_debug_sock, ul_debug_sock_name, ril_index, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH, AID_ROOT, AID_RADIO);
	if (fission_mode == FISSION_MODE_DOUBLE)
		s_param_debug_socket_ul_cell1.fd_listen = create_socket_for_debug(ul_debug_sock_dir1, ul_debug_sock_name, ril_index, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH, AID_ROOT, AID_RADIO);
	s_param_debug_socket_ul_cell2.fd_listen = create_socket_for_debug(ul_debug_sock_dir2, ul_debug_sock_name, ril_index, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH, AID_ROOT, AID_RADIO);

	setuid(AID_RADIO);

	create_pthread();

	start_listen_rild_ul_client(fission_mode);
	start_listen_debug(&s_param_debug_socket_ul);
	if (fission_mode == FISSION_MODE_DOUBLE)
		start_listen_debug(&s_param_debug_socket_ul_cell1);
	start_listen_debug(&s_param_debug_socket_ul_cell2);

	connect_listen_rilp_ul_socket(fission_mode);

	if (!rilp_ul_sock.wait)
		signal_to_connect_server();

	RLOGE("rilulproxy starting sleep loop");
	while (1) {
		sleep(UINT32_MAX);
	}

	return 0;
}





