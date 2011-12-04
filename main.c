#include <stdio.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <gio/gio.h>

#include <libconfig.h>
#include <mysql.h>

#include "job.h"
#include "backup.h"
#include "host.h"
#include "db.h"

#include "busy.h"

typedef struct {
	GList *hosts;
	GQueue *queue;
	config_t config;
	MYSQL *mysql;
} AppData;

AppData *app_data_aux_ptr;


typedef void (*sighandler_t)(int);

static gboolean network_read(GIOChannel *ch, GIOCondition cond, gpointer udata);
static void queue_backup(Host *host, BusBackupType type, AppData *app_data);
static Host *host_find_by_name(const gchar *name, AppData *app_data);
static void reload(gint nr);
static sighandler_t handle_signal(gint sig_nr, sighandler_t signalhandler);


/* *******************************************************
 *                       CLEANUP                         *
 * *******************************************************
 */

/* Clean up
 */
static void cleanup(){
	AppData *app_data;

	app_data = app_data_aux_ptr;

	config_destroy(&app_data->config);

	g_list_foreach(app_data->hosts, (GFunc)g_object_unref, NULL);
	g_queue_foreach(app_data->queue, (GFunc)g_object_unref, NULL);
	g_list_free(app_data->hosts);
	g_queue_free(app_data->queue);
	g_slice_free(AppData, app_data);

//	g_mem_profile();
}

/* *******************************************************
 *                       CALLBACKS                       *
 * *******************************************************
 */

/* Called when a backup is queued
 */
static void on_backup_queued(Backup *backup, time_t queued, AppData *app_data){
	syslog(LOG_DEBUG, "Received Signal: Backup queued: %s backup for host \"%s\"\n", backup_get_backup_type_str(backup), host_get_name(backup_get_host(backup)));
	db_backup_update(backup, "queued", time_t_to_datetime_str(queued), app_data->mysql);
}

/* Called when a backup starts
 */
static void on_backup_started(Backup *backup, time_t started, AppData *app_data){
	syslog(LOG_DEBUG, "Received Signal: Backup started: %s\n", host_get_name(backup_get_host(backup)));
	db_backup_update(backup, "started", time_t_to_datetime_str(started), app_data->mysql);
}

/* Called when a backup has finished
 */
static void on_backup_finished(Backup *backup, time_t finished, AppData *app_data){
	gchar *s;

	syslog(LOG_DEBUG, "Received Signal: Backup finished: %s\n", host_get_name(backup_get_host(backup)));

	// Update database record
	db_backup_update(backup, "finished", time_t_to_datetime_str(finished), app_data->mysql);

	// Calculate duration and update database record
	s = g_strdup_printf("%u", (gint)finished - (gint)backup_get_started(backup));
	db_backup_update(backup, "duration", s, app_data->mysql);

	// If it was a full backup and it has finished without errors, remove all incremental backups
	if (backup_get_backup_type(backup) == BUS_BACKUP_TYPE_FULL && backup_get_failures(backup) == 0){
		host_remove_incr_backups(backup_get_host(backup));
	}

	// Clean up
	g_free(s);
	g_object_unref(backup);
}

/* Called when a backup's state changes
 */
static void on_backup_state_changed(Backup *backup, gint state, AppData *app_data){
	db_backup_update(backup, "state", (gchar*)backup_get_state_str(backup), app_data->mysql);
}

/* Called when a backup fails
 */
static void on_backup_failure(Backup *backup, gint failures, AppData *app_data){
	gchar *s;
	s = g_strdup_printf("%u", failures);
	db_backup_update(backup, "state", s, app_data->mysql);
	g_free(s);
}

/* Called when a backup starts a job
 */
static void on_backup_job_started(Backup *backup, Job *job, AppData *app_data){
	syslog(LOG_DEBUG, "Received Signal: Job Started: %u (%s on Host \"%s\")\n", job_get_pid(job), job_get_srcdir(job), job_get_hostname(job));
	job_set_mysql_id(job, db_job_add(job, app_data->mysql));
}

/* Called when a job finishes
 */
static void on_backup_job_finished(Backup *backup, Job *job, AppData *app_data){
	syslog(LOG_DEBUG, "Received Signal: Job Finished: %u (%s on Host \"%s\")\n", job_get_pid(job), job_get_srcdir(job), job_get_hostname(job));
	db_job_update(job, app_data->mysql);
}

/* Called onincoming TCP connection
 */
static void on_incoming(GSocketService *service, gpointer udata){
	gint fd;
	GIOChannel *channel;
	GSocketConnection *connection;
	AppData *app_data;

	app_data = udata;

	connection = udata;

/*	syslog(LOG_NOTICE, "Incoming Connection\n");*/
	g_object_ref(connection);

	GSocket *socket = g_socket_connection_get_socket(connection);
	fd = g_socket_get_fd(socket);
	channel = g_io_channel_unix_new(fd);
	g_io_add_watch(channel, G_IO_IN, (GIOFunc)network_read, connection);
}


/*******************************************************
 *                       OTHER                         *
 * *****************************************************/

/* Read from TCP connection
 * A TCP connection on PORT 4000 on localhost can be used to issue commands
 * to the daemon, e.g. start manual backups etc.
 */
static gboolean network_read(GIOChannel *channel, GIOCondition condition, gpointer udata){
	GSocketConnection *connection = udata;
	gchar *str, **token;
	gsize len;
	gint ret, i;

	// Read the line
	ret = g_io_channel_read_line(channel, &str, &len, NULL, NULL);
	if (ret == G_IO_STATUS_ERROR){
		syslog(LOG_ERR, "Error reading from network\n");
		g_object_unref(connection);
		return (FALSE);
	}
	if (ret == G_IO_STATUS_EOF){
		return (FALSE);
	}


	// Trim trailing non-alphanumeric character in line
	for (i = len - 1; i > 0; i--){
		if (!isalnum(str[i])){
			str[i] = '\0';
		}
		else {
			break;
		}
	}

	// Split line into tokens
	token = g_strsplit(str, " ", 0);

	// Act upon command
	if (g_strcmp0(token[0], "reload") == 0){
		// Reload configuration
		reload(0);
	}
	else if (g_strcmp0(token[0], "backup") == 0){
		Host *host;
		if (g_strcmp0(token[1], "full") == 0){
			syslog(LOG_NOTICE, "Requested full backup for \"%s\"\n", token[2]);
			if ((host = host_find_by_name(token[2], app_data_aux_ptr)) == NULL){
				syslog(LOG_WARNING, "No such host: %s\n", token[2]);
			}
			else {
				syslog(LOG_NOTICE, "Calling manual_backup() for %s\n", host_get_hostname(host));
				queue_backup(host, BUS_BACKUP_TYPE_FULL, app_data_aux_ptr);
			}
		}
		else if (g_strcmp0(token[1], "incr") == 0){
			syslog(LOG_NOTICE, "Requested incremental backup for \"%s\"\n", token[2]);
			if ((host = host_find_by_name(token[2], app_data_aux_ptr)) == NULL){
				syslog(LOG_WARNING, "No such host: \"%s\"\n", token[2]);
			}
			else {
				queue_backup(host, BUS_BACKUP_TYPE_INCREMENTAL, app_data_aux_ptr);
			}
		}
		else {
			syslog(LOG_WARNING, "Unknown backup type: %s\n", token[1]);
		}
	}
	else if (g_strcmp0(token[0], "remove") == 0){
		syslog(LOG_NOTICE, "Sorry, but this command is not implemented yet...\n");
		//(token[1], token[2]);
	}
	else if (g_strcmp0(token[0], "stop") == 0){
		exit(0);
	}
	else {
		syslog(LOG_WARNING, "Invalid message: %s\n", str);
	}
	return (TRUE);
}


static gboolean find_by_name(Host *host, const gchar *name){
	return (g_strcmp0(name, host_get_name(host)));
}

static Host *host_find_by_name(const gchar *name, AppData *app_data){
	GList *el;
	g_return_val_if_fail(app_data != NULL, NULL);

	if ((el = g_list_find_custom(app_data->hosts, name, (GCompareFunc)find_by_name)) != NULL){
		return (el->data);
	}
	return (NULL);
}


static gdouble get_age(const gchar *str){
	struct tm ti;
	if (str != NULL){
		if (sscanf(str, "%4u-%2u-%2u_%2u-%2u-%2u", &ti.tm_year, &ti.tm_mon, &ti.tm_mday, &ti.tm_hour, &ti.tm_min, &ti.tm_sec) == 6){
			ti.tm_year -= 1900;
			ti.tm_mon--;
			return (difftime(time(NULL), mktime(&ti)) / 86400);
		}
	}
	return (99999);
}

static void queue_backup(Host *host, BusBackupType type, AppData *app_data){
	Backup *backup;

	backup = backup_new_for_host(host);
	g_signal_connect(G_OBJECT(backup), "queued", (GCallback)on_backup_queued, app_data);
	g_signal_connect(G_OBJECT(backup), "started", (GCallback)on_backup_started, app_data);
	g_signal_connect(G_OBJECT(backup), "finished", (GCallback)on_backup_finished, app_data);
	g_signal_connect(G_OBJECT(backup), "state_changed", (GCallback)on_backup_state_changed, app_data);
	g_signal_connect(G_OBJECT(backup), "failure", (GCallback)on_backup_failure, app_data);
	g_signal_connect(G_OBJECT(backup), "job_started", (GCallback)on_backup_job_started, app_data);
	g_signal_connect(G_OBJECT(backup), "job_finished", (GCallback)on_backup_job_finished, app_data);

	backup_set_backup_type(backup, type);
	backup_set_queued(backup, time(NULL));
	backup_set_state(backup, BUS_BACKUP_STATE_QUEUED);
	g_signal_emit_by_name(G_OBJECT(backup), "queued", backup_get_queued(backup));

	backup_set_mysql_id(backup, db_backup_insert(backup, app_data->mysql));
	g_queue_push_tail(app_data->queue, backup);
}

static void check_backup(Host *host, AppData *app_data){
	Backup *backup = NULL;
	BusBackupType type;
	ExistingBackup *youngest, *youngest_full;

	youngest_full = NULL;
	youngest = host_get_youngest_backup(host, BUS_BACKUP_TYPE_ANY);

	if (!youngest || youngest->age >= host_get_max_age_incr(host)){
		type = BUS_BACKUP_TYPE_INCREMENTAL;

		youngest_full = host_get_youngest_backup(host, BUS_BACKUP_TYPE_FULL);
		if (!youngest_full || host_get_n_incr(host) >= host_get_max_incr(host) || youngest_full->age >= host_get_max_age_full(host)){
			type = BUS_BACKUP_TYPE_FULL;
		}
		host_free_existing_backup(youngest_full);

		syslog(LOG_DEBUG, "%s needs a %s backup\n", host_get_hostname(host), type == BUS_BACKUP_TYPE_FULL ? "full" : "incremental");
		queue_backup(host, type, app_data);
	}
	else {
		syslog(LOG_DEBUG, "%s doesn't need a backup right now\n", host_get_hostname(host));
	}
	host_free_existing_backup(youngest);
}


static void ping_host(Host *host, AppData *app_data){
	if (host_is_on_schedule(host)){
		if (host_is_online(host)){
			syslog(LOG_DEBUG, "Host \"%s\" is on schedule and online: %s\n", host_get_name(host), host_get_ip(host));
			check_backup(host, app_data);
		}
		else {
			syslog(LOG_DEBUG, "Host \"%s\" is on schedule but offline\n", host_get_name(host));
		}
	}
	else {
		syslog(LOG_DEBUG, "Host \"%s\" is not on schedule\n", host_get_name(host));
	}
}

static gboolean do_backup(AppData *app_data){
	Backup *backup;

	if ((backup = g_queue_pop_head(app_data->queue)) != NULL){
		backup_run(backup);
	}
	return (TRUE);
}

static gboolean wakeup(AppData *app_data){
	syslog(LOG_DEBUG, "Waking up...\n");
	g_list_foreach(app_data->hosts, (GFunc)ping_host, app_data);
	return (TRUE);
}


static gboolean read_config(AppData *app_data){
	Host *host;
	gint i = 0;
	config_setting_t *cs;
	const gchar *name;
	GString *string;
	const gchar *mysql_login, *mysql_password;

	app_data->hosts = NULL;
	string = g_string_new(NULL);

	config_init(&app_data->config);
	if (config_read_file(&app_data->config, CONFIG_FILE) == CONFIG_FALSE){
		syslog(LOG_ERR, "Failed to read configuration\n");
		return (FALSE);
	}

	if (config_lookup_string(&app_data->config, "mysql_login", &mysql_login) && config_lookup_string(&app_data->config, "mysql_password", &mysql_password)){
		app_data->mysql = db_connect(mysql_login, mysql_password);
	}

	for (i = 0; 1 ; i++){
		g_string_printf(string, "hosts.[%u]", i);
		cs = config_lookup(&app_data->config, string->str);

		if (cs){
			config_setting_lookup_string(cs, "name", &name);
			if ((host = host_new_from_config_setting(&app_data->config, cs)) != NULL){
				app_data->hosts = g_list_append(app_data->hosts, host);
			}
			else {
				syslog(LOG_WARNING, "Failed to read Host from config\n");
			}
		}
		else break;
	}
	
	db_hosts_store(app_data->hosts, app_data->mysql);
	
	return (g_list_length(app_data->hosts) > 0);
}

static void reload(gint nr){
	AppData *app_data = app_data_aux_ptr;
	GList *ptr;
	Host *host;
	
	syslog(LOG_NOTICE, "Reloading configuration");
	
	for (ptr = app_data->hosts; ptr != NULL; ptr = ptr->next){
		host = ptr->data;
		g_object_unref(host);
	}
	
	if (!read_config(app_data)){
		syslog(LOG_ERR, "Failed to (re)load configuration! Sorry, but this means i mus exit now!");
		exit(-1);
	}
	syslog(LOG_NOTICE, "Configuration has been successfully reloaded");
	return;
}

static sighandler_t handle_signal(gint sig_nr, sighandler_t signalhandler){
	struct sigaction old_sig, handler_cleanup, handler_hup;

	handler_hup.sa_handler = reload;
	sigemptyset(&handler_hup.sa_mask);
	handler_hup.sa_flags = SA_RESTART;
	if (sigaction(SIGHUP, &handler_hup, NULL) < 0){
		return (SIG_ERR);
	}
	handler_cleanup.sa_handler = cleanup;
	sigemptyset(&handler_cleanup.sa_mask);
	handler_cleanup.sa_flags = SA_RESTART;
	if (sigaction(SIGTERM, &handler_cleanup, NULL) < 0){
		return (SIG_ERR);
	}
	return (old_sig.sa_handler);
}

static void start_daemon(const gchar *log_name, gint facility){
	gint i;
	pid_t pid;

	if ((pid = fork()) != 0){
		// this is the parent process.
		exit (pid > 0 ? EXIT_SUCCESS : EXIT_FAILURE);
	}

	//Child...
	// Create a new SID (Session) for the child process
	if (setsid() < 0){
		syslog(facility | LOG_ERR, "setsid() failed");
		exit (EXIT_FAILURE);
	}

	handle_signal(SIGHUP, SIG_IGN);

	// Terminate the child
	if ((pid = fork()) != 0){
		exit (EXIT_FAILURE);
	}

	// Change current working directory
	if ((chdir("/")) < 0){
		syslog(facility | LOG_ERR, "chdir() failed");
		exit(EXIT_FAILURE);
	}

	// Change the file mode mask
	umask(0);

	// Close file descriptors
	for (i = sysconf(_SC_OPEN_MAX); i > 0 ; i--){
		close(i);
	}

	// Open log
	openlog(log_name, (LOG_PID | LOG_CONS | LOG_NDELAY), facility);

	GString *lockfilename;

/*
	pid = getpid();
	lockfilename = g_string_new(NULL);
	g_string_printf(lockfilename, "/var/lock/busy/%u", pid);
	g_file_set_contents(lockfilename->str, "busy", -1, NULL);
	g_string_free(lockfilename, TRUE);
*/
}


int main(int argc, char **argv){
	GMainLoop *main_loop;
	GSocketService *service;
	GInetAddress *iaddr;
	GSocketAddress *saddr;
	AppData *app_data;
	GError *error = NULL;

	g_set_prgname(argv[0]);
	g_mem_set_vtable(glib_mem_profiler_table);
	g_type_init();


	start_daemon(g_get_prgname(), LOG_LOCAL0);
	syslog(LOG_NOTICE, "-----------------------------------\n");
	syslog(LOG_NOTICE, "Daemon has been started\n");


	atexit(cleanup);

	app_data = g_slice_new0(AppData);
	app_data->queue = g_queue_new();
	app_data->hosts = NULL;
	app_data_aux_ptr = app_data;

	if (!read_config(app_data)){
		syslog(LOG_ERR, "Failed to read config file \"%s\" or no hosts configured.\n", CONFIG_FILE);
		exit(-1);
	}

	service = g_socket_service_new();
	iaddr = g_inet_address_new_from_string("127.0.0.1");
	saddr = g_inet_socket_address_new(iaddr, 4000);
	g_socket_listener_add_address(G_SOCKET_LISTENER(service), saddr, G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_TCP, NULL, NULL, &error);

	g_object_unref(iaddr);
	g_object_unref(saddr);

	g_socket_service_start(service);
	g_signal_connect(service, "incoming", (GCallback)on_incoming, app_data);

	wakeup(app_data);

	g_timeout_add_seconds(WAKEUP_INTERVAL, (GSourceFunc)wakeup, app_data);
//	g_idle_add((GSourceFunc)do_backup, app_data);
	g_timeout_add_seconds(1, (GSourceFunc)do_backup, app_data);

	main_loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(main_loop);

	return (EXIT_SUCCESS);
}
