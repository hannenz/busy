#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <glib/gstdio.h>

#include "backup.h"
#include "busy.h"
#include "db.h"

enum{
	PROP_0,
	PROP_TYPE,
	PROP_STATE,
	PROP_NAME,
	PROP_PATH,
	PROP_HOST,
	PROP_QUEUED,
	PROP_STARTED,
	PROP_FINISHED,
	PROP_FAILURES,
	PROP_N_JOBS,
	PROP_JOBS,
	PROP_LAST,
	PROP_MYSQL_ID

};

enum{
	QUEUED_SIGNAL,
	STARTED_SIGNAL,
	FINISHED_SIGNAL,
	STATE_SIGNAL,
	FAILURE_SIGNAL,
	JOB_STARTED,
	JOB_FINISHED,
	LAST_SIGNAL
};

static guint backup_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (Backup, backup, G_TYPE_OBJECT);

static void backup_init (Backup *object){
	Backup *backup = BUS_BACKUP(object);

	backup->type = BUS_BACKUP_TYPE_NONE;
	backup->state = BUS_BACKUP_STATE_UNDEFINED;
	backup->host = NULL;
	backup->name = g_string_new(NULL);
	backup->path = g_string_new(NULL);
	backup->queued = 0;
	backup->started = 0;
	backup->finished = 0;
	backup->mysql_id = 0;
	backup->n_jobs = 0;
	backup->failures = 0;
	backup->jobs = NULL;

	gchar *logfile_name;
	backup->logfile_fd = g_file_open_tmp(NULL, &logfile_name, NULL);
	backup->logfile_name = g_string_new(logfile_name);
	g_free(logfile_name);

	backup->logfile_ch = g_io_channel_unix_new(backup->logfile_fd);
}

static void backup_finalize (GObject *object) {
	Backup *backup = BUS_BACKUP(object);


	close(backup->logfile_fd);
	gchar *new_logfile = g_build_filename(backup->path->str, backup->name->str, "backup.log", NULL);


	GFile *src, *dest;
	src = g_file_new_for_path(backup->logfile_name->str);
	dest = g_file_new_for_path(new_logfile);
	g_file_copy(src, dest, 0, FALSE, NULL, NULL, NULL);

	//~ if (g_rename(backup->logfile_name->str, new_logfile) != 0){
		//~ g_print("--- ERROR: g_rename(%s, %s) failed: %s\n", backup->logfile_name->str, new_logfile, g_strerror(errno));
	//~ }
	if (g_chmod(new_logfile, 0644) != 0){
		syslog(LOG_WARNING, "g_chmod(%s, 0644) failed: %s\n", new_logfile, g_strerror(errno));
	}
	g_free(new_logfile);

	g_string_free(backup->name, TRUE);
	g_string_free(backup->path, TRUE);

	GList *j;
	for (j = backup->jobs; j != NULL; j++){
		if (BUS_IS_JOB(j->data)){
			g_object_unref(j->data);
		}
	}

	G_OBJECT_CLASS (backup_parent_class)->finalize (object);
}

void backup_log(Backup *backup, gchar *fmt, ...){
	time_t now;
	va_list arg;
	gchar buf[1024], head[32], *out;
	gsize len;

	g_return_if_fail(BUS_IS_BACKUP(backup));

	va_start(arg, fmt);
	g_vsnprintf(buf, sizeof(buf), fmt, arg);
	va_end(arg);

	now = time(NULL);
	strftime(head, sizeof(head), "%x %H:%M:%S", localtime(&now));

	out = g_strconcat(head, ": ", buf, "\n", NULL);

	if (backup->logfile_ch){
		g_io_channel_write(backup->logfile_ch, out, strlen(out), &len);
	}
	g_free(out);
}

void backup_dump(Backup *backup){
	g_return_if_fail(BUS_IS_BACKUP(backup));

	gchar *types[] = { "None", "Full", "Incremental", "Any" };
	gchar *states[] = { "Undefined", "Queued", "In Progess", "Success", "Failed", "Failed partially" };
	gchar queued[32], started[32], finished[32];

	strftime(queued, sizeof(queued), "%x %H:%M:%S", localtime(&backup->queued));
	strftime(started, sizeof(started), "%x %H:%M:%S", localtime(&backup->started));
	strftime(finished, sizeof(finished), "%x %H:%M:%S", localtime(&backup->finished));

	g_print("***** Backup Dump *****\n");
	g_print("Type:             %s\n", types[backup->type]);
	g_print("State:            %s\n", states[backup->state]);
	g_print("Host:             %s\n", backup->host->name->str);
	g_print("Name:             %s\n", backup->name->str);
	g_print("Path:             %s\n", backup->path->str);
	g_print("Queued:           %s\n", queued);
	g_print("Started:          %s\n", started);
	g_print("Finished:         %s\n", finished);
	g_print("MySQL Id:         %d\n", backup->mysql_id);
	g_print("Nr. of Jobs:      %d\n", backup->n_jobs);
	g_print("Failures:         %d\n", backup->failures);
	g_print("---------------------------------------------\n");
}

Backup *backup_new(void){
	Backup *backup;

	backup = g_object_new(BUS_TYPE_BACKUP, NULL);
	return (backup);
}

Backup *backup_new_for_host(Host *host){
	Backup *backup;

	backup = backup_new();
	backup->host = host;

	gchar *path;
	path = g_build_filename(backup->host->backupdir->str, backup->host->name->str, NULL);
	g_string_assign(backup->path, path);
	g_object_notify(G_OBJECT(backup), "path");
	g_free(path);

	return (backup);
}

static void on_job_started(Job *job, gpointer udata){
	Backup *backup;

	backup = job_get_backup(job);
	g_signal_emit_by_name(G_OBJECT(backup), "job_started", job);
}

static void on_job_finished(Job *job, gint status, gpointer udata){
	GPid pid;
	Backup *backup;
	gchar *srcdir;

	g_object_get(G_OBJECT(job), "pid", &pid, "srcdir", &srcdir, "backup", &backup, NULL);

	backup_log(backup, "The job with PID %d (%s) has finished and returned %d", pid, srcdir, status);

	g_signal_emit_by_name(G_OBJECT(backup), "job_finished", job);

	if (status != 0){
		backup->failures++;
		g_object_notify(G_OBJECT(backup), "failures");
	}

	backup->jobs = g_list_remove(backup->jobs, job);
	g_object_unref(G_OBJECT(job));

	backup->n_jobs--;
	g_object_notify(G_OBJECT(backup), "n_jobs");
	if (backup->n_jobs <= 0){
		backup_log(backup, "Backup finished, %d failures", backup_get_failures(backup));
		backup_set_state(backup, backup->failures == 0 ? BUS_BACKUP_STATE_SUCCESS : BUS_BACKUP_STATE_FAILED);
		backup_set_finished(backup, time(NULL));
		g_signal_emit_by_name(G_OBJECT(backup), "finished", backup_get_finished(backup));
	}
}

static void on_job_state_changed(Job *job, gint status, gpointer udata){
	AppData *app_data = udata;
	db_job_update(job, app_data->mysql);
}

void backup_cancel(Backup *backup){
	g_return_if_fail(BUS_IS_BACKUP(backup));

	GList *jobs, *p;

	jobs = backup_get_jobs(backup);
	for (p = jobs; p != NULL; p = p->next){
		job_cancel(p->data);
	}
	backup_set_state(backup, BUS_BACKUP_STATE_CANCELLED);
}

void backup_run(Backup *backup, AppData *app_data){
	g_return_if_fail(BUS_IS_BACKUP(backup));

	GList *lptr;
	backup_set_started(backup, time(NULL));
	backup_set_state(backup, BUS_BACKUP_STATE_IN_PROGRESS);
	backup_log(backup, "Backup started for Host: %s", backup->host->name->str);

	for (lptr = backup->host->srcdirs; lptr != NULL; lptr = lptr->next){
		Job *job;
		gchar *srcdir = lptr->data;
		if ((job = job_new(backup, srcdir))){
			g_signal_connect(G_OBJECT(job), "started", (GCallback)on_job_started, NULL);
			g_signal_connect(G_OBJECT(job), "finished", (GCallback)on_job_finished, backup);
			g_signal_connect(G_OBJECT(job), "state-changed", (GCallback)on_job_state_changed, app_data);

			backup_add_job(backup, job);
			job_run(job);

			backup_log(backup, "Running Job for srcdir \"%s\"", srcdir);
			backup_log(backup, "Rsync Cmd: %s", job_get_rsync_cmd(job));
		}
	}
}

void backup_set_state(Backup *backup, BusBackupState state){
	g_return_if_fail(BUS_IS_BACKUP(backup));
	backup->state = state;
	g_object_notify(G_OBJECT(backup), "state");
	g_signal_emit_by_name(G_OBJECT(backup), "state_changed", backup_get_state(backup));
}

void backup_set_backup_type(Backup *backup, BusBackupType type){
	g_return_if_fail(BUS_IS_BACKUP(backup));

	backup->type = type;

	if (type == BUS_BACKUP_TYPE_FULL || type == BUS_BACKUP_TYPE_INCREMENTAL){
		gchar name[32];
		time_t now = time(NULL);
		strftime(name, sizeof(name), "%Y-%m-%d_%H-%M-%S", localtime(&now));
		g_string_assign(backup->name, g_strconcat(name, type == BUS_BACKUP_TYPE_FULL ? "f" : "i", NULL));
		g_object_notify(G_OBJECT(backup), "name");
	}
	g_object_notify(G_OBJECT(backup), "type");
}

void backup_set_host(Backup *backup, Host *host){
	g_return_if_fail(BUS_IS_BACKUP(backup));

	backup->host = host;
	g_object_notify(G_OBJECT(backup), "host");
}

void backup_set_queued(Backup *backup, time_t queued){
	g_return_if_fail(BUS_IS_BACKUP(backup));

	backup->queued = queued;
	g_object_notify(G_OBJECT(backup), "queued");
}

void backup_set_started(Backup *backup, time_t started){
	g_return_if_fail(BUS_IS_BACKUP(backup));
	backup->started = started;
	g_signal_emit_by_name(G_OBJECT(backup), "started", backup_get_started(backup));
	g_object_notify(G_OBJECT(backup), "started");
}

void backup_set_finished(Backup *backup, time_t finished){
	g_return_if_fail(BUS_IS_BACKUP(backup));
	backup->finished = finished;
	g_object_notify(G_OBJECT(backup), "finished");
}

void backup_set_failures(Backup *backup, gint n){
	g_return_if_fail(BUS_IS_BACKUP(backup));

	backup->failures = n;
	g_object_notify(G_OBJECT(backup), "failures");
}

void backup_add_job(Backup *backup, Job *job){
	g_return_if_fail(BUS_IS_BACKUP(backup));
	g_return_if_fail(BUS_IS_JOB(job));

	backup->jobs = g_list_append(backup->jobs, job);
	backup->n_jobs = g_list_length(backup->jobs);
	g_object_notify(G_OBJECT(backup), "jobs");
	g_object_notify(G_OBJECT(backup), "n_jobs");
}

void backup_set_mysql_id(Backup *backup, gint id){
	g_return_if_fail(BUS_IS_BACKUP(backup));
	backup->mysql_id = id;
	g_object_notify(G_OBJECT(backup), "mysql_id");
}

void backup_remove_job(Backup *backup, Job *job){
}

void backup_remove_job_by_pid(Backup *backup, GPid pid){
}

BusBackupType backup_get_backup_type(Backup *backup){
	g_return_val_if_fail(BUS_IS_BACKUP(backup), BUS_BACKUP_TYPE_NONE);
	return (backup->type);
}

const gchar *backup_get_backup_type_str(Backup *backup){
	g_return_val_if_fail(BUS_IS_BACKUP(backup), NULL);
	static const gchar *strings[] = {
		"none",
		"full",
		"incremental",
		"any"
	};
	return (strings[backup->type]);
}

BusBackupState backup_get_state(Backup *backup){
	g_return_val_if_fail(BUS_IS_BACKUP(backup), BUS_BACKUP_STATE_UNDEFINED);
	return (backup->state);
}

const gchar *backup_get_state_str(Backup *backup){
	g_return_val_if_fail(BUS_IS_BACKUP(backup), NULL);
	static const gchar *strings[] = {
		"undefined",
		"queued",
		"in progress",
		"success",
		"failed",
		"failed partially",
		"cancelled"
	};
	return (strings[backup->state]);
}

Host *backup_get_host(Backup *backup){
	g_return_val_if_fail(BUS_IS_BACKUP(backup), NULL);
	return (backup->host);
}

const gchar *backup_get_name(Backup *backup){
	g_return_val_if_fail(BUS_IS_BACKUP(backup), NULL);
	return (backup->name->str);
}

const gchar *backup_get_path(Backup *backup){
	g_return_val_if_fail(BUS_IS_BACKUP(backup), NULL);
	return (backup->path->str);
}

gint backup_get_mysql_id(Backup *backup){
	g_return_val_if_fail(BUS_IS_BACKUP(backup), 0);
	return (backup->mysql_id);
}


const gchar *backup_get_full_path(Backup *backup){
	g_return_val_if_fail(BUS_IS_BACKUP(backup), NULL);
	return (g_build_filename(backup->path->str, backup->name->str, NULL));
}

gint backup_get_failures(Backup *backup){
	g_return_val_if_fail(BUS_IS_BACKUP(backup), -1);
	return (backup->failures);
}

time_t backup_get_queued(Backup *backup){
	g_return_val_if_fail(BUS_IS_BACKUP(backup), 0);
	return (backup->queued);
}
time_t backup_get_started(Backup *backup){
	g_return_val_if_fail(BUS_IS_BACKUP(backup), 0);
	return (backup->started);
}
time_t backup_get_finished(Backup *backup){
	g_return_val_if_fail(BUS_IS_BACKUP(backup), 0);
	return (backup->finished);
}

GList *backup_get_jobs(Backup *backup){
	g_return_val_if_fail(BUS_IS_BACKUP(backup), NULL);
	return (backup->jobs);
}


gdouble backup_get_duration(Backup *backup){
	gdouble duration;
	g_return_val_if_fail(BUS_IS_BACKUP(backup), -1.0);

	duration = difftime(backup->finished, backup->started);
	return (duration);
}

gint backup_get_n_jobs(Backup *backup){
	g_return_val_if_fail(BUS_IS_BACKUP(backup), -1);
	return (backup->n_jobs);
}

GList *backup_get_running_jobs(Backup *backup){
	g_return_val_if_fail(BUS_IS_BACKUP(backup), NULL);
	return (backup->jobs);
}

void watch_archive(GPid pid, gint status, gchar *backup){
	if (WIFEXITED(status)){
		if (WEXITSTATUS(status) == 0){
			syslog(LOG_NOTICE, "Backup \"%s\" has been archived successfully", backup);

			// Remove backup
			GError *error;
			gchar *argv[] = { "/bin/rm", "-rf", backup, NULL };
			g_spawn_async(NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid, &error);
		}
		else {
			syslog(LOG_ERR, "Archiving backup \"%s\" failed. Tar exited with %u", backup, WEXITSTATUS(status));
		}
	}
	else {
		syslog(LOG_ERR, "Archiving backup \"%s\" failed. Tar exited unnormally", backup);
	}
	g_free(backup);
}

//~ static gboolean on_gio_out(GIOChannel *source, GIOCondition condition, gpointer udata){
	//~ gchar *line;
	//~ gsize len;
	//~ g_io_channel_read_line(source, &line, &len, NULL, NULL);
	//~ syslog(LOG_NOTICE, ">>>>>>>>>>>>>> %s", line);
	//~ return (TRUE);
//~ }

void backup_archive(ExistingBackup *eback, Host *host){
	const gchar *archivedir;
	gchar *argv[1024], *archive, *backup;
	gint argc, stdin, stdout, stderr;
	GPid pid;
	GError *error;


	archivedir = host_get_archivedir(host);
	if (archivedir != NULL && g_file_test(archivedir, G_FILE_TEST_IS_DIR)){
		archive = g_strdup_printf("%s/%s-%s.tar.gz", archivedir, host_get_name(host), eback->name->str);
		backup = g_strdup_printf("%s/%s/%s", host_get_backupdir(host), host_get_name(host), eback->name->str);

		if (!g_file_test(archive, G_FILE_TEST_EXISTS)){

			argc = 0;
			argv[argc++] = "/bin/tar";
			argv[argc++] = "czf";
			argv[argc++] = archive;
			argv[argc++] = backup;
			argv[argc++] = NULL;


			if (!g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid, &stdin, &stdout, &stderr, &error)){
				syslog(LOG_WARNING, "Spawning process failed: %s\n", error->message);
			}
			syslog(LOG_NOTICE, "Archiving Backup: \"%s\" (tar pid=%u)", backup, pid);
			g_child_watch_add(pid, (GChildWatchFunc)watch_archive, g_strdup(backup));
			//~ GIOChannel *out, *err;
			//~ out = g_io_channel_unix_new(stdout);
			//~ err = g_io_channel_unix_new(stderr);
			//~ g_io_add_watch(out, G_IO_IN, on_gio_out, NULL);
			//~ g_io_add_watch(err, G_IO_IN, on_gio_out, NULL);
		}

		g_free(archive);
		g_free(backup);

	}
	else {
		syslog(LOG_ERR, "Cannot archive backup since %s is not a directory", archivedir);
	}
}

static void backup_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec){
	g_return_if_fail (BUS_IS_BACKUP (object));

	Backup *backup = BUS_BACKUP(object);

	switch (prop_id)
	{
	case PROP_TYPE:
		backup_set_backup_type(backup, g_value_get_int(value));
		break;
	case PROP_HOST:
		backup_set_host(backup, g_value_get_pointer(value));
		break;
	case PROP_QUEUED:
		backup_set_queued(backup, g_value_get_int(value));
		break;
	case PROP_FAILURES:
		backup_set_failures(backup, g_value_get_int(value));
		break;
	case PROP_MYSQL_ID:
		backup_set_mysql_id(backup, g_value_get_int(value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void backup_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec){
	g_return_if_fail (BUS_IS_BACKUP (object));

	Backup *backup = BUS_BACKUP(object);

	switch (prop_id) {
	case PROP_TYPE:
		g_value_set_int(value, backup_get_backup_type(backup));
		break;
	case PROP_STATE:
		g_value_set_int(value, backup_get_state(backup));
		break;
	case PROP_NAME:
		g_value_set_string(value, backup_get_name(backup));
		break;
	case PROP_HOST:
		g_value_set_pointer(value, backup_get_host(backup));
		break;
	case PROP_QUEUED:
		g_value_set_int(value, backup_get_queued(backup));
		break;
	case PROP_STARTED:
		g_value_set_int(value, backup_get_started(backup));
		break;
	case PROP_FINISHED:
		g_value_set_int(value, backup_get_finished(backup));
		break;
	case PROP_FAILURES:
		g_value_set_int(value, backup->failures);
		break;
	case PROP_N_JOBS:
		g_value_set_int(value, backup_get_n_jobs(backup));
		break;
	case PROP_JOBS:
		g_value_set_pointer(value, backup_get_jobs(backup));
		break;
	case PROP_MYSQL_ID:
		g_value_set_int(value, backup_get_mysql_id(backup));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void backup_queued(Backup *self){
}
static void backup_started(Backup *self){
}
static void backup_finished(Backup *self){
}
static void backup_failure(Backup *self){
}
static void backup_state_changed(Backup *self){
}

static void backup_job_started(Backup *self){
}
static void backup_job_finished(Backup *self){
}

static void backup_class_init (BackupClass *klass){
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
/*
	GObjectClass* parent_class = G_OBJECT_CLASS (klass);
*/

	object_class->finalize = backup_finalize;
	object_class->set_property = backup_set_property;
	object_class->get_property = backup_get_property;

	klass->queued = backup_queued;
	klass->started = backup_started;
	klass->finished = backup_finished;
	klass->failure = backup_failure;
	klass->state_changed = backup_state_changed;
	klass->job_started = backup_job_started;
	klass->job_finished = backup_job_finished;

	g_object_class_install_property (object_class,
	                                 PROP_TYPE,
	                                 g_param_spec_int("type", "Type", "The backup's type", BUS_BACKUP_TYPE_NONE, BUS_BACKUP_TYPE_ANY, BUS_BACKUP_TYPE_NONE, G_PARAM_READWRITE)
									);
	g_object_class_install_property (object_class,
	                                 PROP_STATE,
	                                 g_param_spec_int("state", "State", "The backup's current state", BUS_BACKUP_STATE_UNDEFINED, BUS_BACKUP_STATE_FAILED_PARTIALLY, BUS_BACKUP_STATE_UNDEFINED, G_PARAM_READWRITE)
									);
	g_object_class_install_property(object_class,
									PROP_NAME,
									g_param_spec_string("name", "Name", "The backup's name", "", G_PARAM_READABLE)
									);
	g_object_class_install_property(object_class,
									PROP_PATH,
									g_param_spec_string("path", "Path", "The backup's path", "", G_PARAM_READWRITE)
									);
	g_object_class_install_property (object_class,
	                                 PROP_HOST,
	                                 g_param_spec_pointer("host", "Host", "The Host of the backup", G_PARAM_READWRITE)
									);
	g_object_class_install_property (object_class,
	                                 PROP_JOBS,
	                                 g_param_spec_pointer("jobs", "Jobs", "Jobs", G_PARAM_READWRITE)
									);
	g_object_class_install_property (object_class,
	                                 PROP_QUEUED,
	                                 g_param_spec_int("queued", "Queued", "Time when the backup has been queued", 0, G_MAXINT, 0, G_PARAM_READWRITE)
									);
	g_object_class_install_property (object_class,
	                                 PROP_STARTED,
	                                 g_param_spec_int("started", "Started", "Time when the backup has been started", 0, G_MAXINT, 0, G_PARAM_READWRITE)
									);
	g_object_class_install_property (object_class,
	                                 PROP_FINISHED,
	                                 g_param_spec_int("finished", "Finished", "Time when the backup has been finished", 0, G_MAXINT, 0, G_PARAM_READWRITE)
									);
	g_object_class_install_property (object_class,
	                                 PROP_FAILURES,
	                                 g_param_spec_int("failures", "Failures", "Nr. of failures that occured during backup", 0, G_MAXINT, 0, G_PARAM_READWRITE)
									);
	g_object_class_install_property (object_class,
	                                 PROP_N_JOBS,
	                                 g_param_spec_int("n_jobs", "n-jobs", "Nr. of running jobs for this backup", 0, G_MAXINT, 0, G_PARAM_READWRITE)
									);
	g_object_class_install_property (object_class,
	                                 PROP_MYSQL_ID,
	                                 g_param_spec_int("mysql_id", "MYSQL Id", "The MySQL ID", 0, G_MAXINT, 0, G_PARAM_READWRITE)
									);

	backup_signals[QUEUED_SIGNAL] =
		g_signal_new ("queued",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (BackupClass, queued),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__INT,
		              G_TYPE_NONE, 1, G_TYPE_INT
		             );
	backup_signals[STARTED_SIGNAL] =
		g_signal_new ("started",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (BackupClass, started),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__INT,
		              G_TYPE_NONE, 1, G_TYPE_INT
		             );
	backup_signals[FINISHED_SIGNAL] =
		g_signal_new ("finished",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (BackupClass, finished),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__INT,
		              G_TYPE_NONE, 1, G_TYPE_INT
		             );
	backup_signals[STATE_SIGNAL] =
		g_signal_new ("state_changed",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (BackupClass, state_changed),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__INT,
		              G_TYPE_NONE, 1, G_TYPE_INT
		             );
	backup_signals[FAILURE_SIGNAL] =
		g_signal_new ("failure",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (BackupClass, failure),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__INT,
		              G_TYPE_NONE, 1, G_TYPE_INT
		             );
	backup_signals[JOB_STARTED] =
		g_signal_new ("job_started",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (BackupClass, job_started),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__POINTER,
		              G_TYPE_NONE, 1, G_TYPE_POINTER
		             );
	backup_signals[JOB_FINISHED] =
		g_signal_new ("job_finished",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (BackupClass, job_finished),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__POINTER,
		              G_TYPE_NONE, 1, G_TYPE_POINTER
		             );
}

