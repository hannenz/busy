#include "db.h"
#include <syslog.h>


gchar *time_t_to_datetime_str(time_t time){
	gchar str[24];
	time_t t = time;
	strftime(str, sizeof(str), "%Y-%m-%d %H:%M:%S", localtime(&t));
	return (g_strdup(str));
}

MYSQL *db_connect(const gchar *login, const gchar *password){
	MYSQL *mysql;
	mysql = mysql_init(NULL);
	if (mysql){
		mysql = mysql_real_connect(mysql, "localhost", login, password, "busy", 0, NULL, CLIENT_IGNORE_SIGPIPE);
		if (mysql){
			my_bool reconnect = 1;
			mysql_options(mysql, MYSQL_OPT_RECONNECT, &reconnect);
			syslog(LOG_NOTICE, "Connected to MySQL Database.\n");
			return (mysql);
		}
	}

	mysql = NULL;
	syslog(LOG_ERR, "Connection to MySQL Database failed.\n");
	return (NULL);
}

void db_close(MYSQL *mysql){
	if (mysql){
		mysql_close(mysql);
	}
}

gint db_backup_insert(Backup *backup, MYSQL *mysql){
	gint id = -1;
	gchar *query;
	gchar *queued, *started, *finished;

	if (mysql){
		queued = time_t_to_datetime_str(backup->queued);
		started = time_t_to_datetime_str(backup->started);
		finished = time_t_to_datetime_str(backup->finished);
		Host *host = backup_get_host(backup);
		
		query = g_strdup_printf(
			"INSERT INTO `backups` (`type`, `host_name`, `host_hostname`, `host_ip`, `state`, `name`, `path`, `queued`, `started`, `finished`, `host_backupdir`, `host_rsync_opts`, `failures`) VALUES ('%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%u')",
			backup_get_backup_type_str(backup),
			host_get_name(host),
			host_get_hostname(host),
			host_get_ip(host),
			backup_get_state_str(backup),
			backup_get_name(backup),
			backup_get_path(backup),
			queued,
			started,
			finished,
			host_get_backupdir(host),
			host_get_rsync_opts(host),
			backup_get_failures(backup)
		);
		if (mysql_real_query(mysql, query, strlen(query)) != 0){
			syslog(LOG_ERR, "MySQL Query failed: %s\n%s\n", query, mysql_error(mysql));
		}
		else {
			id = mysql_insert_id(mysql);
		}
		g_free(query);
		g_free(queued);
		g_free(started);
		g_free(finished);
	}
	return (id);
}

gint db_backup_update(Backup *backup, const gchar *key, gchar *value, MYSQL *mysql){
	gchar *query;
	gint id, ret = 1;

	if ((id = backup_get_mysql_id(backup)) > 0){
		if (mysql){
			query = g_strdup_printf(
				"UPDATE `backups` SET `%s`='%s' WHERE `id`='%u'",
					key,
					value,
					id
			);
			if ((ret = mysql_real_query(mysql, query, strlen(query))) != 0){
				syslog(LOG_ERR, "MySQL Query failed: %s (%d: %s)", query, ret, mysql_error(mysql));
			}
			g_free(query);
		}
	}
	return (ret == 0);
}

gint db_job_add(Job *job, MYSQL *mysql){
	gchar *query, *started, *finished;
	gint id = -1;
	
	if (mysql){
		started = time_t_to_datetime_str(job->started);
		finished = time_t_to_datetime_str(job->finished);
		query = g_strdup_printf(
			"INSERT INTO `jobs` (`pid`, `srcdir`, `backup_id`, `rsync_cmd`, `started`, `finished`, `exit_code`, `state`, `destdir`) VALUES ('%d', '%s', '%d', '%s', '%s', '%s', '%d', '%s', '%s')",
			job->pid, job->srcdir->str, job->backup->mysql_id, job->rsync_cmd->str, started, finished, job->exit_code, job_get_state_str(job), job->destdir->str
		);
		if (mysql_real_query(mysql, query, strlen(query)) != 0){
			syslog(LOG_ERR, "MySQL Query failed: %s (%s)", query, mysql_error(mysql));
		}
		else {
			id = mysql_insert_id(mysql);
		}
		g_free(query);
	}
	return (id);
}

gint db_job_update(Job *job, MYSQL *mysql){
	gchar *query;
	gint id, ret = 1;

	time_t finished;
	gint exit_code, state;

	g_object_get(G_OBJECT(job), "finished", &finished, "exit_code", &exit_code, "state", &state, NULL);
	
	if ((id = job_get_mysql_id(job)) > 0){
		if (mysql){
			query = g_strdup_printf(
				"UPDATE `jobs` SET `finished`='%s', `exit_code`='%d', `state`='%s' WHERE `id`='%d'",
				time_t_to_datetime_str(finished),
				exit_code,
				job_get_state_str(job),
				id
			);
			if ((ret = mysql_real_query(mysql, query, strlen(query))) != 0){
				syslog(LOG_ERR, "MySQL Query failed: %s (%d: %s)", query, ret, mysql_error(mysql));
			}
			g_free(query);
		}
	}
	return (ret == 0);
}

gint db_hosts_store(GList *hosts, MYSQL *mysql){
	gchar *query;
	GList *ptr, *ptr2;
	Host *host;
	gint ret;
	
	GString
		*schedule,
		*excludes,
		*ips,
		*srcdirs;
	;
	
	for (ptr = hosts; ptr != NULL; ptr = ptr->next){
		host = ptr->data;
		
		schedule = g_string_new(NULL);
		for (ptr2 = host->schedule ; ptr2 != NULL ; ptr2 = ptr2->next){
			schedule = g_string_append(schedule, ptr2->data);
			schedule = g_string_append(schedule, "\n");
		}
		
		excludes = g_string_new(NULL);
		for (ptr2 = host->excludes ; ptr2 != NULL ; ptr2 = ptr2->next){
			excludes = g_string_append(excludes, ptr2->data);
			excludes = g_string_append(excludes, "\n");
		}

		srcdirs = g_string_new(NULL);
		for (ptr2 = host->srcdirs ; ptr2 != NULL ; ptr2 = ptr2->next){
			srcdirs = g_string_append(srcdirs, ptr2->data);
			srcdirs = g_string_append(srcdirs, "\n");
		}

		ips = g_string_new(NULL);
		for (ptr2 = host->ips ; ptr2 != NULL ; ptr2 = ptr2->next){
			ips = g_string_append(ips, ptr2->data);
			ips = g_string_append(ips, "\n");
		}
		
		MYSQL_RES *result;
		MYSQL_ROW row;

		query = g_strdup_printf("SELECT * FROM `hosts` WHERE `name`=\"%s\";", host_get_name(host));

		ret = mysql_real_query(mysql, query, strlen(query));

		g_free(query);

		if (ret != 0){
			syslog (LOG_ERR, "MySQL Query failed: %s (%d: %s)", query, ret, mysql_error(mysql));
			continue;
		}

		result = mysql_store_result(mysql);

		gint n;
		n = mysql_num_rows(result);
		if (n > 1){
			query = g_strdup_printf("DELETE FROM `hosts` WHERE `name`=\"%s\"", host_get_name(host));
			mysql_real_query(mysql, query, strlen(query));
			g_free(query);
			n = 0;
		}
		
		switch (n){
			case 1:
				row = mysql_fetch_row(result);
				query = g_strdup_printf("UPDATE `hosts` SET `name`='%s', `hostname`='%s', `max_incr`='%u', `max_age_full`='%f', `max_age_incr`='%f', `backupdir`='%s', `schedule`='%s', `excludes`='%s', `ips`='%s', `srcdirs`='%s' WHERE id='%u'",
					host_get_name(host),
					host_get_hostname(host),
					host_get_max_incr(host),
					host_get_max_age_full(host),
					host_get_max_age_incr(host),
					host_get_backupdir(host),
					schedule->str,
					excludes->str,
					ips->str,
					srcdirs->str,
					atoi(row[0])
				);
				break;
				
			case 0:
				query = g_strdup_printf("INSERT INTO `hosts` (`name`, `hostname`, `max_incr`, `max_age_full`, `max_age_incr`, `backupdir`, `schedule`, `excludes`, `ips`, `srcdirs`) VALUES ('%s', '%s', '%d', '%.5f', '%.5f', '%s', '%s', '%s', '%s', '%s')",
					host_get_name(host),
					host_get_hostname(host),
					host_get_max_incr(host),
					host_get_max_age_full(host),
					host_get_max_age_incr(host),
					host_get_backupdir(host),
					schedule->str,
					excludes->str,
					ips->str,
					srcdirs->str
				);
				break;
				
			default:
				break;
		
		}
		mysql_free_result(result);
		
		if ((ret = mysql_real_query(mysql, query, strlen(query))) != 0){
			syslog (LOG_ERR, "MySQL Query failed: %s (%d: %s)", query, ret, mysql_error(mysql));
		}
		g_free(query);
	}
	return (0);
}
	
	
/*
gboolean db_remove_backup(gchar *destdir){
	gchar *query;
	gint ret = 1;
	
	if (mysql){
		query = g_strdup_printf("SELECT id FROM `backups` WHERE `destdir`='%s'", destdir);
		if ((ret = mysql_real_query(mysql, query, strlen(query))) != 0){
			syslog(LOG_ERR, "MySQL Query failed: %s (%d: %s)", query, ret, mysql_error(mysql));
		}
		else {
			g_free(query);
			
			MYSQL_RES *result;
			MYSQL_ROW row;
			
			result = mysql_store_result(mysql);
			row = mysql_fetch_row(result);
			gint id;
			id = atoi(row[0]);
		
			query = g_strdup_printf("DELETE FROM `backups` WHERE `id`='%d'", id);
			if ((ret = mysql_real_query(mysql, query, strlen(query))) != 0){
				syslog(LOG_ERR, "MySQL Query failed: %s (%d: %s)", query, ret, mysql_error(mysql));
			}
			g_free(query);
			query = g_strdup_printf("DELETE FROM `jobs` WHERE `backup_id`='%d'", id);
			if ((ret = mysql_real_query(mysql, query, strlen(query))) != 0){
				syslog(LOG_ERR, "MySQL Query failed: %s (%d: %s)", query, ret, mysql_error(mysql));
			}
		}
		g_free(query);
	}
	return (ret == 0);
}
*/
