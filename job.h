#ifndef _JOB_H_
#define _JOB_H_

#include <mysql.h>
#include <glib-object.h>
#include <errno.h>

G_BEGIN_DECLS

void on_notify(GObject *object, GParamSpec *pspec, MYSQL *mysql);

#define BUS_TYPE_JOB             (job_get_type ())
#define BUS_JOB(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BUS_TYPE_JOB, Job))
#define BUS_JOB_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BUS_TYPE_JOB, JobClass))
#define BUS_IS_JOB(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BUS_TYPE_JOB))
#define BUS_IS_JOB_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BUS_TYPE_JOB))
#define BUS_JOB_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BUS_TYPE_JOB, JobClass))

typedef struct _JobClass JobClass;
typedef struct _Job Job;

typedef enum {
	BUS_JOB_UNDEFINED,
	BUS_JOB_RUNNING,
	BUS_JOB_SUCCESS,
	BUS_JOB_FAILURE
} BusJobState;

#include "host.h"
#include "backup.h"

struct _JobClass
{
	GObjectClass parent_class;

	/* Signals */
	void (* started) (Job *self);
	void (* finished) (Job *self);
};

struct _Job
{
	GObject parent_instance;

	GPid pid;
	GString *srcdir;
	GString *destdir;
	GString *rsync_cmd;
	time_t started;
	time_t finished;
	gint exit_code;
	gint mysql_id;
	BusJobState state;
	Backup *backup;

};

GType job_get_type (void) G_GNUC_CONST;

Job *job_new(Backup *backup, gchar *srcdir);
void job_run(Job *job);

GPid job_get_pid(Job *job);
gint job_get_mysql_id(Job *job);
gint job_get_state(Job *job);
const gchar *job_get_srcdir(Job *job);
const gchar *job_get_hostname(Job *job);
const gchar *job_get_state_str(Job *job);
const gchar *job_get_destdir(Job *job);
const gchar *job_get_rsync_cmd(Job *job);
time_t job_get_started(Job *job);
time_t job_get_finished(Job *job);
gint job_get_exit_code(Job *job);
Backup *job_get_backup(Job *job);


void job_set_mysql_id(Job *job, gint id);
void job_set_state(Job *job, BusJobState state);
void job_set_pid(Job *job, GPid pid);
void job_set_rsync_cmd(Job *job, gchar *str);
void job_set_srcdir(Job *job, gchar *str);
void job_set_destdir(Job *job, gchar *str);
void job_set_started(Job *job, time_t t);
void job_set_finished(Job *job, time_t t);
void job_set_exit_code(Job *job, gint code);
void job_set_backup(Job *job, Backup *backup);


G_END_DECLS

#endif
