/* 
 * rilqrcproxy.c
 * zongpeng
 */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#define LOG_TAG "RILQRCROXYD"
#define LOG_NDDEBUG 0
#include <utils/Log.h>
#include <cutils/container.h>
#include <sys/wait.h>
#include <private/android_filesystem_config.h>
#include "socketproxy.h"
#include <sys/stat.h>

#define VP1      "cell1"
#define VP2      "cell2"
	 
static struct client_socket rild_qrc_sock[2];
static struct server_socket rilp_qrc_sock;

const char *rild_qrc_sock_sub_dir1 = "/data/cells/cell1/dev/socket/qmux_radio";
const char *rild_qrc_sock_sub_dir2 = "/data/cells/cell2/dev/socket/qmux_radio";
const char *rilp_qrc_sock_sub_dir = "/dev/socket/qmux_radio";
const char *qrc_debug_sock = "/dev/socket";
const char *qrc_debug_sock_dir1 = "/data/cells/cell1/dev/socket";
const char *qrc_debug_sock_dir2 = "/data/cells/cell2/dev/socket";
const char *qrc_debug_sock_name = "qrc_debug";

enum {
    RIL_CARD1 = 0,
    RIL_CARD2,
    RIL_CARD_NUM
};


const char *ril_qrc_socket_name[RIL_CARD_NUM] = {
    "qcril_radio_config0",
    "qcril_radio_config1"
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

static void ril_qrc_server_disconnect(scp *s_cp)
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
		if (!rilp_qrc_sock.wait)
			signal_to_connect_server();
    }
}

static void ril_qrc_client_disconnect(slp *s_lp)
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


static slp s_param_listen_socket_ril_qrc1 = {
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
    ril_qrc_client_disconnect,  /* client_disconnect */
    NULL,
    &cdi[0],
};

static slp s_param_listen_socket_ril_qrc2 = {
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
    ril_qrc_client_disconnect,       /* client_disconnect */
    NULL,
    &cdi[1],
};


static scp s_param_connect_socket_ril_qrc = {
    -1,                              /* fdConnect */
    NULL,                            /* processName */
    &s_connect_event,                /* connect_event */
    NULL,                            /* RecordStream */
    NULL,                            /* process_server_data_callback */
	ril_qrc_server_disconnect,	     /* server_disconnect */
	NULL,						     /* cache_buf */
	&sdi,
};

static sdp s_param_debug_socket_qrc = {
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

static sdp s_param_debug_socket_qrc_cell1 = {
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

static sdp s_param_debug_socket_qrc_cell2 = {
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

static void init_ril_qrc(int ril_index)
{
    rilp_qrc_sock.type = QCRIL_RADIO_CONFIG;
    rilp_qrc_sock.index = ril_index;
	rilp_qrc_sock.wait = 0;
    rilp_qrc_sock.sub_dir = rilp_qrc_sock_sub_dir;
    rilp_qrc_sock.sock_name = ril_qrc_socket_name;
    rilp_qrc_sock.s_cp = &s_param_connect_socket_ril_qrc;
	rilp_qrc_sock.s_cp->sdi->pid = getpid();

    rild_qrc_sock[0].index = ril_index;
    rild_qrc_sock[0].sock_name = ril_qrc_socket_name;
    rild_qrc_sock[0].sub_dir = rild_qrc_sock_sub_dir1;
    rild_qrc_sock[0].s_lp = &s_param_listen_socket_ril_qrc1;
    
    rild_qrc_sock[1].index = ril_index;
    rild_qrc_sock[1].sock_name = ril_qrc_socket_name;
    rild_qrc_sock[1].sub_dir = rild_qrc_sock_sub_dir2;
    rild_qrc_sock[1].s_lp = &s_param_listen_socket_ril_qrc2;
}

static void create_socket_for_ril_qrc(int fission_mode)
{
    if (fission_mode == FISSION_MODE_DOUBLE) {
        create_client_socket_by_path(&rild_qrc_sock[0], &rilp_qrc_sock, 0, 
                                                AID_RADIO, AID_RADIO, fission_mode);
    }
    create_client_socket_by_path(&rild_qrc_sock[1], &rilp_qrc_sock, 0, 
                                            AID_RADIO, AID_RADIO, fission_mode);

}

static void start_listen_rild_qrc_client(int fission_mode)
{
    if (fission_mode == FISSION_MODE_DOUBLE) {
        start_listen_client(&rild_qrc_sock[0]);
    }
    start_listen_client(&rild_qrc_sock[1]);
}

static void connect_listen_rilp_qrc_socket(int fission_mode)
{
    connect_server_socket(&rilp_qrc_sock, rild_qrc_sock, fission_mode);
}


int main(int argc, char **argv)
{
	int fission_mode = get_fission_mode();
	unsigned int ril_index = 0;

	if (argc < 2) {
		RLOGE("rilqrcproxy input error!\n");
		return -1;
	}

	ril_index = atoi(argv[1]);
	if (ril_index >= RIL_CARD_NUM) {
		RLOGE("rilqrcproxy input error! ril_index = %d\n", ril_index);
		return -1;
	}

	RLOGD("rilqrcproxy ril_index = %d\n", ril_index);

	init_ril_qrc(ril_index);

	create_socket_for_ril_qrc(fission_mode);
	s_param_debug_socket_qrc.fd_listen = create_socket_for_debug(qrc_debug_sock, qrc_debug_sock_name, ril_index, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH, AID_ROOT, AID_RADIO);
	if (fission_mode == FISSION_MODE_DOUBLE)
		s_param_debug_socket_qrc_cell1.fd_listen = create_socket_for_debug(qrc_debug_sock_dir1, qrc_debug_sock_name, ril_index, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH, AID_ROOT, AID_RADIO);
	s_param_debug_socket_qrc_cell2.fd_listen = create_socket_for_debug(qrc_debug_sock_dir2, qrc_debug_sock_name, ril_index, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH, AID_ROOT, AID_RADIO);

	setuid(AID_RADIO);

	create_pthread();

	start_listen_rild_qrc_client(fission_mode);
	start_listen_debug(&s_param_debug_socket_qrc);
	if (fission_mode == FISSION_MODE_DOUBLE)
		start_listen_debug(&s_param_debug_socket_qrc_cell1);
	start_listen_debug(&s_param_debug_socket_qrc_cell2);

	connect_listen_rilp_qrc_socket(fission_mode);

	if (!rilp_qrc_sock.wait)
		signal_to_connect_server();

	RLOGE("rilqrcproxy starting sleep loop");
	while (1) {
		sleep(UINT32_MAX);
	}

	return 0;
}



