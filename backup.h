#ifndef _BACKUP_H_
#define _BACKUP_H_

#include <libconfig.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define BUS_TYPE_BACKUP             (backup_get_type ())
#define BUS_BACKUP(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BUS_TYPE_BACKUP, Backup))
#define BUS_BACKUP_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BUS_TYPE_BACKUP, BackupClass))
#define BUS_IS_BACKUP(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BUS_TYPE_BACKUP))
#define BUS_IS_BACKUP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BUS_TYPE_BACKUP))
#define BUS_BACKUP_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BUS_TYPE_BACKUP, BackupClass))

typedef struct _BackupClass BackupClass;
typedef struct _Backup Backup;

typedef enum {
	BUS_BACKUP_TYPE_NONE,
	BUS_BACKUP_TYPE_FULL,
	BUS_BACKUP_TYPE_INCREMENTAL,
	BUS_BACKUP_TYPE_ANY
} BusBackupType;

typedef enum {
	BUS_BACKUP_STATE_UNDEFINED,
	BUS_BACKUP_STATE_QUEUED,
	BUS_BACKUP_STATE_IN_PROGRESS,
	BUS_BACKUP_STATE_SUCCESS,
	BUS_BACKUP_STATE_FAILED,
	BUS_BACKUP_STATE_FAILED_PARTIALLY
} BusBackupState;

#include "host.h"
#include "job.h"

struct _BackupClass
{
	GObjectClass parent_class;

	/* Signals */
	void (* queued) (Backup *self);
	void (* started) (Backup *self);
	void (* finished) (Backup *self);
	void (* failure) (Backup *self);
	void (* state_changed) (Backup *self);
	void (* job_started) (Backup *self);
	void (* job_finished) (Backup *self);
};

struct _Backup
{
	GObject parent_instance;

	BusBackupType type;
	BusBackupState state;
	Host *host;
	GString *name;
	GString *path;
	time_t queued;
	time_t started;
	time_t finished;
	gint mysql_id;
	gint n_jobs;
	gint failures;
	GList *jobs;
	gint logfile_fd;
	GIOChannel *logfile_ch;
	GString *logfile_name;
};

GType backup_get_type (void) G_GNUC_CONST;



Backup *backup_new(void);
Backup *backup_new_for_host(Host *host);
void backup_dump(Backup *backup);


Host *backup_get_host(Backup *backup);
gint backup_get_failures(Backup *backup);
time_t backup_get_started(Backup *backup);
time_t backup_get_queued(Backup *backup);
time_t backup_get_finished(Backup *backup);
gint backup_get_mysql_id(Backup *backup);
BusBackupType backup_get_backup_type(Backup *backup);
BusBackupState backup_get_state(Backup *backup);
const gchar *backup_get_backup_type_str(Backup *backup);
const gchar *backup_get_state_str(Backup *backup);
const gchar *backup_get_path(Backup *backup);
const gchar *backup_get_name(Backup *backup);
const gchar *backup_get_full_path(Backup *backup);

void backup_set_backup_type(Backup *backup, BusBackupType type);
void backup_set_state(Backup *backup, BusBackupState state);
void backup_set_queued(Backup *backup, time_t t);
void backup_set_started(Backup *backup, time_t t);
void backup_set_finished(Backup *backup, time_t t);
void backup_set_mysql_id(Backup *backup, gint id);

void backup_add_job(Backup *backup, Job *job);
void backup_run(Backup *backup);

G_END_DECLS

#endif /* _BACKUP_H_ */
