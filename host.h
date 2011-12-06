#ifndef _HOST_H_
#define _HOST_H_

#include <libconfig.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define BUS_TYPE_HOST             (host_get_type ())
#define BUS_HOST(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BUS_TYPE_HOST, Host))
#define BUS_HOST_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BUS_TYPE_HOST, HostClass))
#define BUS_IS_HOST(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BUS_TYPE_HOST))
#define BUS_IS_HOST_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BUS_TYPE_HOST))
#define BUS_HOST_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BUS_TYPE_HOST, HostClass))

typedef struct {
	GString *name;
	gint type;
	gdouble age;
} ExistingBackup;

typedef struct _HostClass HostClass;
typedef struct _Host Host;

#include "backup.h"
#include "job.h"

struct _HostClass
{
	GObjectClass parent_class;

	/* Signals */
	void (* created) (Host *self);
};

struct _Host
{
	GObject parent_instance;

	GString *name;
	GString *hostname;
	GString *user;
	GString *ip;
	GString *rsync_opts;
	GString *backupdir;
	GString *archivedir;
	GList *ips;
	GList *srcdirs;
	GList *includes;
	GList *excludes;
	GList *schedule;
	gint max_incr;
	gdouble max_age;
	gdouble max_age_full;
	gdouble max_age_incr;
	gint mysql_id;

};

GType host_get_type (void) G_GNUC_CONST;

Host *host_new(void);
Host *host_new_from_config_setting(config_t *config, config_setting_t *cs);

// Setters

void 		host_set_name(Host *self, const gchar *name);
void 		host_set_hostname(Host *self, const gchar *hostname);
void 		host_set_user(Host *self, const gchar *user);
void 		host_set_rsync_opts(Host *self, const gchar *rsync_opts);
void 		host_set_max_incr(Host *self, gint max_incr);
void 		host_set_max_age_incr(Host *self, gdouble max_age_incr);
void 		host_set_max_age_full(Host *self, gdouble max_age_full);
void 		host_set_max_age_full(Host *self, gdouble max_age);
void 		host_set_max_age(Host *self, gdouble max_age);
void 		host_set_mysql_id(Host *self, gint mysql_id);

// Getters

const gchar	*host_get_name(Host *self);
const gchar *host_get_hostname(Host *self);
const gchar *host_get_user(Host *self);
const gchar *host_get_rsync_opts(Host *self);
const gchar *host_get_ip(Host *self);
const gchar *host_get_backupdir(Host *self);
const gchar *host_get_archivedir(Host *self);
gint		host_get_max_incr(Host *host);
gdouble		host_get_max_age_incr(Host *host);
gdouble		host_get_max_age_full(Host *host);
gdouble		host_get_max_age(Host *host);
gint		host_get_n_incr(Host *host);
gint		host_get_n_full(Host *host);
gint		host_get_mysql_id(Host *self);
ExistingBackup *host_get_youngest_backup(Host *host, BusBackupType type);


// Doers

void 		host_add_exclude(Host *self, const gchar *str);
void 		host_add_include(Host *self, const gchar *str);
void 		host_add_srcdir(Host *self, const gchar *str);
void 		host_add_ip(Host *self, const gchar *str);
void 		host_add_schedule(Host *self, const gchar *str);
void		host_dump(Host *host);
GList		*host_read_backups(Host *host, BusBackupType type);
void		host_free_existing_backup(ExistingBackup *eb);

// Checkers

gboolean	host_is_online(Host *host);
gboolean	host_is_on_schedule(Host *host);
G_END_DECLS

#endif /* _HOST_H_ */
