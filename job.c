#include <sys/wait.h>
#include "job.h"

enum{
	PROP_0,
	PROP_JOB_SRCDIR,
	PROP_JOB_RSYNC_CMD,
	PROP_JOB_PID,
	PROP_JOB_STARTED,
	PROP_JOB_FINISHED,
	PROP_JOB_STATE,
	PROP_JOB_BACKUP,
	PROP_JOB_MYSQL_ID,
	PROP_JOB_EXIT_CODE,
	PROP_JOB_DESTDIR
};

enum{
	STARTED,
	FINISHED,
	LAST_SIGNAL
};


static guint job_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (Job, job, G_TYPE_OBJECT);

static void job_init (Job *object){
	Job *job = BUS_JOB(object);
	job->srcdir = g_string_new(NULL);
	job->destdir = g_string_new(NULL);
	job->rsync_cmd = g_string_new(NULL);
	job->backup = NULL;
}

static void job_finalize (GObject *object) {
	Job *job = BUS_JOB(object);
	g_string_free(job->srcdir, TRUE);
	g_string_free(job->rsync_cmd, TRUE);
	g_string_free(job->destdir, TRUE);
	G_OBJECT_CLASS (job_parent_class)->finalize (object);
}

gchar *replace_slashes(gchar *str, gchar repl){
	gchar *ret, *s;

	ret = g_strdup(str);
	for (s = ret; *s; s++){
		if (*s == '/'){
			*s = repl;
		}
	}
	return (ret);
}

Job *job_new(Backup *backup, gchar *srcdir){
	Job *job;

	job = g_object_new(BUS_TYPE_JOB, "backup", backup, "srcdir", srcdir, NULL);
	job->state = BUS_JOB_UNDEFINED;
	job->started = 0;
	job->finished = 0;
	g_string_assign(job->destdir, replace_slashes(srcdir, 'B'));
	g_string_assign(job->srcdir, srcdir);

	return (job);
}


void watch_job(GPid pid, gint status, Job *job){
	g_object_set(G_OBJECT(job), "finished", time(NULL), "state", ((WEXITSTATUS(status) == 0) ? BUS_JOB_SUCCESS : BUS_JOB_FAILURE), NULL);
	g_signal_emit_by_name(G_OBJECT(job), "finished", WEXITSTATUS(status));
//	g_object_unref(job);
}

void job_run(Job *job){
	g_return_if_fail(BUS_IS_JOB(job));

	GList *lptr;
	gchar *excludes = NULL, *includes = NULL, *src, *linkdest = NULL, *argv[1024];
	gchar *logfile;
	gchar *logfileparam;
	gint argc = 0;
	Backup *backup;
	Host *host;

	backup = job_get_backup(job);
	host = backup_get_host(backup);

	argv[argc++] = "/usr/bin/rsync";
	argv[argc++] = "-e";
	argv[argc++] = "ssh";
	argv[argc++] = job->backup->host->rsync_opts->str;

	if (backup_get_backup_type(backup) == BUS_BACKUP_TYPE_INCREMENTAL){
		ExistingBackup *youngest;
		gchar *l;
		youngest = host_get_youngest_backup(host, BUS_BACKUP_TYPE_FULL);
		l = g_build_filename(backup_get_full_path(backup), youngest->name->str, job_get_destdir(job), NULL);
		linkdest = g_strdup_printf("--link-dest=%s", l);
		argv[argc++] = linkdest;
		g_slice_free(ExistingBackup, youngest);
		g_free(l);
	}

	if (g_list_length(job->backup->host->excludes) > 0){
		for (lptr = job->backup->host->excludes; lptr != NULL; lptr = lptr->next){
			gchar *s;
			s = g_strdup_printf("--exclude=\"%s\"", (gchar *)lptr->data);
			excludes = (excludes == NULL) ? g_strdup(s) : g_strjoin(" ", excludes, s, NULL);
			g_free(s);
		}
		argv[argc++] = excludes;
	}
	if (g_list_length(job->backup->host->includes) > 0){
		for (lptr = job->backup->host->includes; lptr != NULL; lptr = lptr->next){
			gchar *s;
			s = g_strdup_printf("--include=\"%s\"", (gchar *)lptr->data);
			includes = (includes == NULL) ? g_strdup(s) : g_strjoin(includes, s, NULL);
			g_free(s);
		}
		argv[argc++] = includes;
	}

	gchar *dest;
	const gchar *ip;
	ip = host_get_ip(host);
	if (g_strcmp0(ip, "127.0.0.1")){
		src = g_strdup_printf("root@%s:%s", ip, job_get_srcdir(job));
	}
	else{
		src = g_strdup(job_get_srcdir(job));
	}
	dest = g_build_filename(backup_get_full_path(backup), job_get_destdir(job), NULL);
	logfile = g_build_filename(backup_get_full_path(backup), job_get_destdir(job), "rsync.log", NULL);
	logfileparam = g_strdup_printf("--log-file=%s", logfile);
	argv[argc++] = logfileparam;

	if (g_mkdir_with_parents(dest, 0755) == -1){
		g_print("Couldn't create directory: %s %s\n", dest, g_strerror(errno));
		return;
	}

	src = g_strconcat(src, "/", NULL);
	dest = g_strconcat(dest, "/", NULL);

	argv[argc++] = src;
	argv[argc++] = dest;
	argv[argc++] = NULL;

	g_object_set(G_OBJECT(job), "rsync_cmd", g_strjoinv(" ", argv), NULL);

	g_print("RSYNC: %s\n", g_strjoinv(" ", argv));

	GError *error = NULL;
	GPid pid;
	gint stdin, stdout, stderr;
	if (!g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid, &stdin, &stdout, &stderr, &error)){
		g_print("Spawning process failed: %s\n", error->message);
	}
	else {
		g_object_set(G_OBJECT(job), "state", BUS_JOB_RUNNING, "started", time(NULL), "pid", pid, NULL);
		g_child_watch_add(pid, (GChildWatchFunc)watch_job, job);
		job_set_state(job, BUS_JOB_RUNNING);
		g_signal_emit_by_name(G_OBJECT(job), "started");
	}

	g_free(src);
	g_free(dest);
	g_free(excludes);
	g_free(includes);
	g_free(linkdest);
	g_free(logfile);
	g_free(logfileparam);
}

void job_dump(Job *job){
	g_return_if_fail(BUS_IS_JOB(job));

	g_print("******** Job Dump *******************\n");
	g_print("---------------------------------------\n");
}

GPid job_get_pid(Job *job){
	g_return_val_if_fail(BUS_IS_JOB(job), 0);
	return (job->pid);
}

gint job_get_mysql_id(Job *job){
	g_return_val_if_fail(BUS_IS_JOB(job), 0);
	return (job->mysql_id);
}

const gchar *job_get_srcdir(Job *job){
	g_return_val_if_fail(BUS_IS_JOB(job), NULL);
	return (job->srcdir->str);
}

const gchar *job_get_hostname(Job *job){
	g_return_val_if_fail(BUS_IS_JOB(job), NULL);
	return (job->backup->host->name->str);
}

gint job_get_state(Job *job){
	g_return_val_if_fail(BUS_IS_JOB(job), BUS_JOB_UNDEFINED);
	return (job->state);
}

const gchar *job_get_state_str(Job *job){
	g_return_val_if_fail(BUS_IS_JOB(job), NULL);
	static const gchar *strings[] = {
		"undefined",
		"running",
		"success",
		"failure"
	};
	return (strings[job->state]);
}

const gchar *job_get_destdir(Job *job){
	g_return_val_if_fail(BUS_IS_JOB(job), NULL);
	return (job->destdir->str);
}

const gchar *job_get_rsync_cmd(Job *job){
	g_return_val_if_fail(BUS_IS_JOB(job), NULL);
	return (job->rsync_cmd->str);
}

time_t job_get_started(Job *job){
	g_return_val_if_fail(BUS_IS_JOB(job), -1);
	return (job->started);
}

time_t job_get_finished(Job *job){
	g_return_val_if_fail(BUS_IS_JOB(job), -1);
	return (job->finished);
}

gint job_get_exit_code(Job *job){
	g_return_val_if_fail(BUS_IS_JOB(job), -1);
	return (job->exit_code);
}

Backup *job_get_backup(Job *job){
	g_return_val_if_fail(BUS_IS_JOB(job), NULL);
	return (job->backup);
}



void job_set_mysql_id(Job *job, gint id){
	g_return_if_fail(BUS_IS_JOB(job));
	job->mysql_id = id;
	g_object_notify(G_OBJECT(job), "mysql_id");
}

void job_set_state(Job *job, BusJobState state){
	g_return_if_fail(BUS_IS_JOB(job));
	job->state = state;
	g_object_notify(G_OBJECT(job), "state");
}

void job_set_exit_code(Job *job, gint code){
	g_return_if_fail(BUS_IS_JOB(job));
	job->exit_code = code;
	g_object_notify(G_OBJECT(job), "exit_code");
}

void job_set_rsync_cmd(Job *job, gchar *cmd){
	g_return_if_fail(BUS_IS_JOB(job));
	g_string_assign(job->rsync_cmd, cmd);
	g_object_notify(G_OBJECT(job), "rsync_cmd");
}

void job_set_started(Job *job, time_t t){
	g_return_if_fail(BUS_IS_JOB(job));
	job->started = t;
	g_object_notify(G_OBJECT(job), "started");
}

void job_set_finished(Job *job, time_t t){
	g_return_if_fail(BUS_IS_JOB(job));
	job->finished = t;
	g_object_notify(G_OBJECT(job), "finished");
}

void job_set_srcdir(Job *job, gchar *str){
	g_return_if_fail(BUS_IS_JOB(job));
	g_string_assign(job->srcdir, str);
	g_object_notify(G_OBJECT(job), "srcdir");
}

void job_set_destdir(Job *job, gchar *str){
	g_return_if_fail(BUS_IS_JOB(job));
	g_string_assign(job->destdir, str);
	g_object_notify(G_OBJECT(job), "destdir");
}

void job_set_pid(Job *job, GPid pid){
	g_return_if_fail(BUS_IS_JOB(job));
	job->pid = pid;
	g_object_notify(G_OBJECT(job), "pid");
}

void job_set_backup(Job *job, Backup *backup){
	g_return_if_fail(BUS_IS_JOB(job));
	job->backup = backup;
	g_object_notify(G_OBJECT(job), "backup");
}

static void job_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec){
	g_return_if_fail (BUS_IS_JOB (object));

	Job *job = BUS_JOB(object);

	switch (prop_id){
		case PROP_JOB_SRCDIR:
			g_string_assign(job->srcdir, g_value_get_string(value));
			break;
		case PROP_JOB_RSYNC_CMD:
			g_string_assign(job->rsync_cmd, g_value_get_string(value));
			break;
		case PROP_JOB_PID:
			job->pid = g_value_get_int(value);
			break;
		case PROP_JOB_STARTED:
			job->started = g_value_get_int(value);
			break;
		case PROP_JOB_FINISHED:
			job->finished = g_value_get_int(value);
			break;
		case PROP_JOB_STATE:
			job->state = g_value_get_int(value);
			break;
		case PROP_JOB_BACKUP:
			job->backup = g_value_get_pointer(value);
			break;
		case PROP_JOB_MYSQL_ID:
			job->mysql_id = g_value_get_int(value);
			break;
		case PROP_JOB_EXIT_CODE:
			job->exit_code = g_value_get_int(value);
			break;
		case PROP_JOB_DESTDIR:
			g_string_assign(job->destdir, g_value_get_string(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void job_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec){
	g_return_if_fail (BUS_IS_JOB (object));

	Job *job = BUS_JOB(object);

	switch (prop_id){
		case PROP_JOB_SRCDIR:
			g_value_set_string(value, job->srcdir->str);
			break;
		case PROP_JOB_RSYNC_CMD:
			g_value_set_string(value, job->rsync_cmd->str);
			break;
		case PROP_JOB_PID:
			g_value_set_int(value, job->pid);
			break;
		case PROP_JOB_STARTED:
			g_value_set_int(value, job->started);
			break;
		case PROP_JOB_FINISHED:
			g_value_set_int(value, job->finished);
			break;
		case PROP_JOB_STATE:
			g_value_set_int(value, job->state);
			break;
		case PROP_JOB_BACKUP:
			g_value_set_pointer(value, job->backup);
			break;
		case PROP_JOB_MYSQL_ID:
			g_value_set_int(value, job->mysql_id);
			break;
		case PROP_JOB_EXIT_CODE:
			g_value_set_int(value, job->exit_code);
			break;
		case PROP_JOB_DESTDIR:
			g_value_set_string(value, job->destdir->str);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void job_started (Job *self){
	/* TODO: Add default signal handler implementation here */
}

static void job_finished (Job *self){
}

static void job_class_init (JobClass *klass){
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
/*
	GObjectClass* parent_class = G_OBJECT_CLASS (klass);
*/

	object_class->finalize = job_finalize;
	object_class->set_property = job_set_property;
	object_class->get_property = job_get_property;

	klass->started = job_started;
	klass->finished = job_finished;

	g_object_class_install_property(object_class, PROP_JOB_PID, g_param_spec_int("pid", "Pid", "The job's pid", 0, G_MAXINT, 0, G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_JOB_STARTED, g_param_spec_int("started", "Started", "", 0, G_MAXINT, 0, G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_JOB_FINISHED, g_param_spec_int("finished", "Finished", "", 0, G_MAXINT, 0, G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_JOB_STATE, g_param_spec_int("state", "State", "", 0, 99, 0, G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_JOB_SRCDIR, g_param_spec_string("srcdir", "Srcdir", "", "", G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_JOB_RSYNC_CMD, g_param_spec_string("rsync_cmd", "Rsync-cmd", "", "", G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_JOB_BACKUP, g_param_spec_pointer("backup", "Backup", "", G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_JOB_MYSQL_ID, g_param_spec_int("mysql_id", "MySQL Id", "", 0, G_MAXINT, 0, G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_JOB_EXIT_CODE, g_param_spec_int("exit_code", "Exit Code", "", 0, G_MAXINT, 0, G_PARAM_READWRITE));
	g_object_class_install_property(object_class, PROP_JOB_DESTDIR, g_param_spec_string("destdir", "Destination directory", "", "", G_PARAM_READWRITE));

	job_signals[STARTED] = g_signal_new ("started", G_OBJECT_CLASS_TYPE (klass), G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (JobClass, started), NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
	job_signals[FINISHED] = g_signal_new ("finished", G_OBJECT_CLASS_TYPE (klass), G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (JobClass, finished), NULL, NULL, g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);
}

