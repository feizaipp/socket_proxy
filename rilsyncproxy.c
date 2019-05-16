/* 
 * rilsyncproxy.c
 * zongpeng
 */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#define LOG_TAG "RILSYNCPROXYD"
#define LOG_NDDEBUG 0
#include <utils/Log.h>
#include <cutils/container.h>
#include <sys/wait.h>
#include <private/android_filesystem_config.h>
#include "socketproxy.h"
	 
#define VP1      "cell1"
#define VP2      "cell2"
	 
static struct client_socket rild_sync_sock[2];
static struct server_socket rilp_sync_sock;

const char *rild_sync_sock_sub_dir1 = "/data/cells/cell1/dev/socket/qmux_radio";
const char *rild_sync_sock_sub_dir2 = "/data/cells/cell2/dev/socket/qmux_radio";
const char *rilp_sync_sock_sub_dir = "/dev/socket/qmux_radio";

enum {
    RIL_CARD1 = 0,
    RIL_CARD2,
    RIL_CARD_NUM
};


const char *ril_sync_socket_name[RIL_CARD_NUM] = {
    "rild_sync_0",
    "rild_sync_1"
};

static struct socket_event s_commands_event1;
static struct socket_event s_listen_event1;
static struct socket_event s_commands_event2;
static struct socket_event s_listen_event2;
static struct socket_event s_connect_event;

static void ril_sync_server_disconnect(scp *s_cp)
{
	if (s_cp->fd_connect > 0) {
        close(s_cp->fd_connect);
        s_cp->fd_connect = -1;
        socket_event_del(s_cp->connect_event);
        free_record_stream(s_cp->p_rs);
		if (s_cp->cache) {
			free(s_cp->cache->buf);
			s_cp->cache->count = 0;
			s_cp->cache->offset = 0;
		}
		signal_to_connect_server();
    }
}

static void ril_sync_client_disconnect(slp *s_lp)
{
	if (s_lp->fd_command > 0) {
		close(s_lp->fd_command);
        s_lp->fd_command = -1;
		s_lp->accept = 0;
		s_lp->cache_send = 0;
        socket_event_del(s_lp->commands_event);
        free_record_stream(s_lp->p_rs);
        socket_event_add_wakeup(s_lp->listen_event);
	}
}


static slp s_param_listen_socket_ril_sync1 = {
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
    ril_sync_client_disconnect,  /* client_disconnect */
};

static slp s_param_listen_socket_ril_sync2 = {
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
    ril_sync_client_disconnect,       /* client_disconnect */
};


static scp s_param_connect_socket_ril_sync = {
    -1,                              /* fdConnect */
    NULL,                            /* processName */
    &s_connect_event,                /* connect_event */
    NULL,                            /* RecordStream */
    NULL,                            /* process_server_data_callback */
	ril_sync_server_disconnect,	     /* server_disconnect */
	NULL,						     /* cache_buf */
};



static void init_ril_sync(int ril_index)
{
    rilp_sync_sock.type = RILD_SYNC;
    rilp_sync_sock.index = ril_index;
    rilp_sync_sock.sub_dir = rilp_sync_sock_sub_dir;
    rilp_sync_sock.sock_name = ril_sync_socket_name;
    rilp_sync_sock.s_cp = &s_param_connect_socket_ril_sync;

    rild_sync_sock[0].index = ril_index;
    rild_sync_sock[0].sock_name = ril_sync_socket_name;
    rild_sync_sock[0].sub_dir = rild_sync_sock_sub_dir1;
    rild_sync_sock[0].s_lp = &s_param_listen_socket_ril_sync1;
    
    rild_sync_sock[1].index = ril_index;
    rild_sync_sock[1].sock_name = ril_sync_socket_name;
    rild_sync_sock[1].sub_dir = rild_sync_sock_sub_dir2;
    rild_sync_sock[1].s_lp = &s_param_listen_socket_ril_sync2;
}

static void create_socket_for_ril_sync(int fission_mode)
{
    if (fission_mode == FISSION_MODE_DOUBLE) {
        create_client_socket_by_path(&rild_sync_sock[0], &rilp_sync_sock, 0, 
                                                AID_RADIO, AID_RADIO, fission_mode);
    }
    create_client_socket_by_path(&rild_sync_sock[1], &rilp_sync_sock, 0, 
                                            AID_RADIO, AID_RADIO, fission_mode);

}

static void start_listen_rild_sync_client(int fission_mode)
{
    if (fission_mode == FISSION_MODE_DOUBLE) {
        start_listen_client(&rild_sync_sock[0]);
    }
    start_listen_client(&rild_sync_sock[1]);
}

static void connect_listen_rilp_sync_socket(int fission_mode)
{
    connect_server_socket(&rilp_sync_sock, rild_sync_sock, fission_mode);
}


int main(int argc, char **argv)
{
	int fission_mode = get_fission_mode();
	unsigned int ril_index = 0;

	if (argc < 2) {
		RLOGE("rilsyncproxy input error!\n");
		return -1;
	}

	ril_index = atoi(argv[1]);
	if (ril_index >= RIL_CARD_NUM) {
		RLOGE("rilsyncproxy input error! ril_index = %d\n", ril_index);
		return -1;
	}

	RLOGD("rilsyncproxy ril_index = %d\n", ril_index);

	init_ril_sync(ril_index);

	create_socket_for_ril_sync(fission_mode);

	setuid(AID_RADIO);

	create_pthread();

	start_listen_rild_sync_client(fission_mode);

	connect_listen_rilp_sync_socket(fission_mode);

	//signal_to_connect_server();

	RLOGE("rilsyncproxy starting sleep loop");
	while (1) {
		sleep(UINT32_MAX);
	}

	return 0;
}


