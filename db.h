#ifndef __DB_H__
#define __DB_H__

#include <glib.h>
#include <syslog.h>
#include <string.h>
#include <mysql.h>
#include <stdlib.h>
#include "backup.h"
#include "job.h"

MYSQL *db_connect(const gchar *username, const gchar *password);
void db_close(MYSQL *mysql);
gint db_backup_insert(Backup *backup, MYSQL *mysql);
gint db_backup_update(Backup *backup, const gchar *key, gchar *value, MYSQL *mysql);
gint db_job_add(Job *job, MYSQL *mysql);
gint db_job_update(Job *job, MYSQL *mysql);
gint db_hosts_store(GList *hosts, MYSQL *mysql);

gchar *time_t_to_datetime_str(time_t time);
gint db_remove_backup(gchar *name);
#endif
