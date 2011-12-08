#ifndef __BUSY_H__
#define __BUSY_H__

#include <syslog.h>
#include <mysql.h>

//#define USER "root"
#define WAKEUP_INTERVAL 60
#define CONFIG_FILE "/etc/busy/busy.conf"

typedef struct {
	GList *hosts;
	GQueue *queue;
	GList *running_backups;
	config_t config;
	MYSQL *mysql;
} AppData;

#define DEFAULT_BACKUPDIR "/var/backups"
#define DEFAULT_RSYNC_OPTS "-xaHD"
#define DEFAULT_USER "root"
#define DEFAULT_MAX_INCR 10
#define DEFAULT_MAX_AGE_INCR 1
#define DEFAULT_MAX_AGE_FULL 7
#define DEFAULT_MAX_AGE	30

#endif
