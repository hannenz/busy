#ifndef __BUSY_H__
#define __BUSY_H__

#include <syslog.h>
#include <mysql.h>

#define USER "root"
#define WAKEUP_INTERVAL 60
#define CONFIG_FILE "/etc/busy/busy.conf"

typedef struct {
	GList *hosts;
	GQueue *queue;
	GList *running_backups;
	config_t config;
	MYSQL *mysql;
} AppData;


#endif
