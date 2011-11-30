#include <sys/wait.h>
#include <string.h>
#include <ctype.h>
#include <glib/gstdio.h>
#include "host.h"

enum{
	PROP_0,
	PROP_HOSTNAME,
	PROP_NAME,
	PROP_IP,
	PROP_BACKUPDIR,
	PROP_RSYNC_OPTS,
	PROP_MAX_INCR,
	PROP_MAX_AGE_INCR,
	PROP_MAX_AGE_FULL,
	PROP_SRCDIRS,
	PROP_INCLUDES,
	PROP_EXCLUDES,
	PROP_IPS,
	PROP_SCHEDULE
};

enum{
	CREATED,
	LAST_SIGNAL
};

static guint host_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (Host, host, G_TYPE_OBJECT);

static void host_init (Host *object){
	Host *host = BUS_HOST(object);
	host->name = g_string_new(NULL);
	host->hostname = g_string_new(NULL);
	host->rsync_opts = g_string_new(NULL);
	host->ip = g_string_new(NULL);
	host->backupdir = g_string_new(NULL);
	host->srcdirs = NULL;
	host->excludes = NULL;
	host->includes = NULL;
	host->ips = NULL;
	host->schedule = NULL;
}

static void host_finalize (GObject *object) {
	Host *host = BUS_HOST(object);
	g_print("Finalizing host: %s\n", host->name->str);
	g_string_free(host->name, TRUE);
	g_string_free(host->hostname, TRUE);
	g_string_free(host->rsync_opts, TRUE);
	g_string_free(host->ip, TRUE);
	g_string_free(host->backupdir, TRUE);
	g_list_free(host->srcdirs);
	g_list_free(host->excludes);
	g_list_free(host->includes);
	g_list_free(host->ips);
	g_list_free(host->schedule);

	G_OBJECT_CLASS (host_parent_class)->finalize (object);
}

Host *host_new(void){
	return (g_object_new(BUS_TYPE_HOST, NULL));
}

gchar *rtrim (gchar *s){
	gint i;
	for (i = strlen(s) - 1; i > 0; i--){
		if (isspace(s[i]) || s[i] == '/'){
			s[i] = '\0';
		}
		else {
			break;
		}
	}
	return (s);
}

Host *host_new_from_config_setting(config_t *config, config_setting_t *cs){
	Host *host;
	const gchar *name, *hostname, *rsync_opts, *backupdir;
	long int mi;
	gint max_incr;
	gdouble max_age_incr, max_age_full;
	
	if ((host = host_new()) == NULL){
		return (NULL);
	}

	if (config_setting_lookup_string(cs, "name", &name) == CONFIG_FALSE){
		name = NULL;
	}
	if (config_setting_lookup_string(cs, "backupdir", &backupdir) == CONFIG_FALSE){
		if ((config_lookup_string(config, "default.backupdir", &backupdir)) == CONFIG_FALSE){
			backupdir = NULL;
		}
	}
	if (config_setting_lookup_string(cs, "hostname", &hostname) == CONFIG_FALSE){
		hostname = NULL;
	}
	if (config_setting_lookup_string(cs, "rsync_opts", &rsync_opts) == CONFIG_FALSE){
		if ((config_lookup_string(config, "default.rsync_opts", &rsync_opts)) == CONFIG_FALSE){
			rsync_opts = "";
		}
	}

	if (config_setting_lookup_int(cs, "max_incr", (long int*)&mi) == CONFIG_FALSE){
		if (config_lookup_int(config, "default.max_incr", (long int*)&mi) == CONFIG_FALSE){
			mi = 7;
		}
	}
	max_incr = (gint)mi;
	if (config_setting_lookup_float(cs, "max_age_incr", &max_age_incr) == CONFIG_FALSE){
		if (config_lookup_float(config, "default.max_age_incr", &max_age_incr) == CONFIG_FALSE){
			max_age_incr = 1.0;
		}
	}
	if (config_setting_lookup_float(cs, "max_age_full", &max_age_full) == CONFIG_FALSE){
		if (config_lookup_float(config, "default.max_age_full", &max_age_full) == CONFIG_FALSE){
			max_age_full = 7.0;
		}
	}

	if (name == NULL || hostname == NULL || backupdir == NULL){
		g_object_unref(host);
		return (NULL);
	}

	gchar *bdir;
	bdir = g_strdup(backupdir);
	bdir = rtrim(bdir);

	g_object_set(G_OBJECT(host),
		"backupdir", bdir,
		"name", name,
		"hostname", hostname,
		"rsync_opts", rsync_opts,
		"max_incr", max_incr,
		"max_age_incr", max_age_incr,
		"max_age_full", max_age_full,
		NULL
	);

	g_free(bdir);

	config_setting_t *cs2;
	cs2 = config_setting_get_member(cs, "excludes");
	if (!cs2){
		cs2 = config_lookup(config, "default.excludes");
	}
	if (cs2){
		gint i = 0;
		const gchar *s;
		while ((s = config_setting_get_string_elem(cs2, i++)) != NULL){
			host_add_exclude(host, s);
		}
	}
	cs2 = config_setting_get_member(cs, "includes");
	if (!cs2){
		cs2 = config_lookup(config, "default.includes");
	}
	if (cs2){
		gint i = 0;
		const gchar *s;
		while ((s = config_setting_get_string_elem(cs2, i++)) != NULL){
			host_add_include(host, s);
		}
	}
	cs2 = config_setting_get_member(cs, "srcdirs");
	if (!cs2){
		cs2 = config_lookup(config, "default.srcdirs");
	}
	if (cs2){
		gint i = 0;
		const gchar *s;
		while ((s = config_setting_get_string_elem(cs2, i++)) != NULL){
			host_add_srcdir(host, s);
		}
	}
	cs2 = config_setting_get_member(cs, "ips");
	if (!cs2){
		cs2 = config_lookup(config, "default.ips");
	}
	if (cs2){
		gint i = 0;
		const gchar *s;
		while ((s = config_setting_get_string_elem(cs2, i++)) != NULL){
			if (g_regex_match_simple("([0-9]{1,3})\\.([0-9]{1,3})\\.([0-9]{1,3})\\.([0-9]{1,3})", s, 0, 0)){
				host_add_ip(host, s);
			}
			else {
				g_printerr("Invalid IP: %s, skipping...\n", s);
			}
		}
	}
	cs2 = config_setting_get_member(cs, "schedule");
	if (!cs2){
		cs2 = config_lookup(config, "default.schedule");
	}
	if (cs2){
		gint i = 0;
		const gchar *s;
		while ((s = config_setting_get_string_elem(cs2, i++)) != NULL){
			if (g_regex_match_simple("([0-1][0-9])|(2[0-3]):[0-5][0-9]", s, 0, 0)){
				host_add_schedule(host, s);
			}
			else {
				g_printerr("Invalid Schedule Time: %s, skipping...\n", s);
			}
		}
	}

	if (g_list_length(host->ips) < 1){
		g_object_unref(host);
		return (NULL);
	}

	host->ip = g_string_new(g_list_nth_data(host->ips, 0));

	return (host);
}


static gint isofunc(ExistingBackup *a, ExistingBackup *b, gpointer udata){
	return (b->age - a->age);
}

GList *host_read_backups(Host *host, BusBackupType type){
	GDir *dir;
	const gchar *name;
	gchar *path;
	GList *backups;

	g_return_val_if_fail(BUS_IS_HOST(host), NULL);

	backups = NULL;

	path = g_build_filename(host->backupdir->str, host->name->str, NULL);
	if ((dir = g_dir_open(path, 0, NULL))){
		while ((name = g_dir_read_name(dir)) != NULL){
			struct tm ti;
			gchar tyc;
			gint n;
			BusBackupType ty;
				
			n = sscanf(name, "%4u-%2u-%2u_%2u-%2u-%2u%c", &ti.tm_year, &ti.tm_mon, &ti.tm_mday, &ti.tm_hour, &ti.tm_min, &ti.tm_sec, &tyc);
			if (n == 7){
				ti.tm_year -= 1900;
				ti.tm_mon -= 1;
				switch (tyc){
					case 'f':
						ty = BUS_BACKUP_TYPE_FULL;
						break;
					case 'i':
						ty = BUS_BACKUP_TYPE_INCREMENTAL;
						break;
					default:
						continue;
				}
				if (ty == type || type == BUS_BACKUP_TYPE_ANY){
					ExistingBackup *eback;
					eback = g_slice_new(ExistingBackup);
					eback->name = g_string_new(name);
					eback->type = ty;
					eback->age = difftime(time(NULL), mktime(&ti)) / 86400;
					backups = g_list_insert_sorted_with_data(backups, eback, (GCompareDataFunc)isofunc, NULL);
				}
			}
		}
		g_dir_close(dir);
	}
	g_free(path);
	return (backups);
}

void host_free_existing_backup(ExistingBackup *eb){
	if (eb != NULL){
		g_string_free(eb->name, TRUE);
		g_slice_free(ExistingBackup, eb);
	}
}

gint host_get_n_incr(Host *host){
	GList *backups;
	gint n;

	g_return_val_if_fail(BUS_IS_HOST(host), -1);
	
	backups = host_read_backups(host, BUS_BACKUP_TYPE_INCREMENTAL);
	n = g_list_length(backups);
	g_list_foreach(backups, (GFunc)host_free_existing_backup, NULL);
	g_list_free(backups);
	return (n);
}

gint host_get_n_full(Host *host){
	GList *backups;
	gint n;
	
	g_return_val_if_fail(BUS_IS_HOST(host), -1);

	backups = host_read_backups(host, BUS_BACKUP_TYPE_FULL);
	n = g_list_length(backups);
	g_list_foreach(backups, (GFunc)host_free_existing_backup, NULL);
	g_list_free(backups);
	return (n);
}

ExistingBackup *host_get_youngest_backup(Host *host, BusBackupType type){
	ExistingBackup *youngest;
	GList *backups, *b;

	g_return_val_if_fail(BUS_IS_HOST(host), NULL);

	youngest = NULL;
	backups = host_read_backups(host, type);
	if (backups != NULL){
		b = backups;
		youngest = backups->data;
		backups = backups->next;
		g_list_free1(b);
		g_list_foreach(backups, (GFunc)host_free_existing_backup, NULL);
		g_list_free(backups);
	}
	return (youngest);
}


void delete_folder_tree (const gchar* directory_name) {
	GDir *dp;
	const gchar *name;
	gchar p_buf[512] = {0};

	dp = g_dir_open(directory_name, 0, NULL);

	while ((name = g_dir_read_name(dp)) != NULL) {
		g_snprintf(p_buf, sizeof(p_buf), "%s/%s", directory_name, name);
		if (g_file_test(p_buf, G_FILE_TEST_IS_DIR))
			delete_folder_tree(p_buf);
		else
			g_remove(p_buf);
    }
    g_dir_close(dp);
    g_rmdir(directory_name);
}

void host_remove_incr_backups(Host *host){
	GDir *dir;
	const gchar *name;
	gchar *path;

	g_return_if_fail(BUS_IS_HOST(host));
	
	path = g_build_filename(host->backupdir->str, host->name->str, NULL);
	if ((dir = g_dir_open(path, 0, NULL))){
		while ((name = g_dir_read_name(dir)) != NULL){
			if (g_str_has_suffix(name, "i")){
				gchar *dpath;
				dpath = g_build_filename(path, name, NULL);
				g_print("Removing backup: %s\n", dpath);
				delete_folder_tree(dpath);
				g_free(dpath);
			}
		}
		g_dir_close(dir);
	}
	g_free(path);
}

void host_set_name(Host *self, const gchar *name){
	g_return_if_fail(BUS_IS_HOST(self));
	g_string_assign(self->name, name);
	g_object_notify(G_OBJECT(self), "name");
}

void host_set_hostname(Host *self, const gchar *hostname){
	g_return_if_fail(BUS_IS_HOST(self));
	g_string_assign(self->hostname, hostname);
	g_object_notify(G_OBJECT(self), "hostname");
}

void host_set_rsync_opts(Host *self, const gchar *rsync_opts){
	g_return_if_fail(BUS_IS_HOST(self));
	g_string_assign(self->rsync_opts, rsync_opts);
	g_object_notify(G_OBJECT(self), "rsync_opts");
}

void host_set_ip(Host *self, const gchar *ip){
	g_return_if_fail(BUS_IS_HOST(self));
	g_string_assign(self->ip, ip);
	g_object_notify(G_OBJECT(self), "ip");
}

void host_set_backupdir(Host *self, const gchar *backupdir){
	g_return_if_fail(BUS_IS_HOST(self));
	g_string_assign(self->backupdir, backupdir);
	g_object_notify(G_OBJECT(self), "backupdir");
}

void host_set_max_incr(Host *self, gint max_incr){
	g_return_if_fail(BUS_IS_HOST(self));
	self->max_incr = max_incr;
	g_object_notify(G_OBJECT(self), "max_incr");
}

void host_set_max_age_incr(Host *self, gdouble max_age_incr){
	g_return_if_fail(BUS_IS_HOST(self));
	self->max_age_incr = max_age_incr;
	g_object_notify(G_OBJECT(self), "max_age_incr");
	
}

void host_set_max_age_full(Host *self, gdouble max_age_full){
	g_return_if_fail(BUS_IS_HOST(self));
	self->max_age_full = max_age_full;
	g_object_notify(G_OBJECT(self), "max_age_full");
}

void host_add_exclude(Host *self, const gchar *exclude){
	g_return_if_fail(BUS_IS_HOST(self));
	self->excludes = g_list_append(self->excludes, g_strdup(exclude));
	g_object_notify(G_OBJECT(self), "excludes");
}

void host_add_include(Host *self, const gchar *include){
	g_return_if_fail(BUS_IS_HOST(self));
	self->includes = g_list_append(self->includes, g_strdup(include));
	g_object_notify(G_OBJECT(self), "includes");
}

void host_add_srcdir(Host *self, const gchar *srcdir){
	g_return_if_fail(BUS_IS_HOST(self));
	self->srcdirs = g_list_append(self->srcdirs, rtrim(g_strdup(srcdir)));
	g_object_notify(G_OBJECT(self), "srcdirs");
}

void host_add_ip(Host *self, const gchar *ip){
	g_return_if_fail(BUS_IS_HOST(self));
	self->ips = g_list_append(self->ips, g_strdup(ip));
	g_object_notify(G_OBJECT(self), "ips");
}

void host_add_schedule(Host *self, const gchar *schedule){
	g_return_if_fail(BUS_IS_HOST(self));
	self->schedule = g_list_append(self->schedule, g_strdup(schedule));
	g_object_notify(G_OBJECT(self), "schedule");
}

const gchar *host_get_name(Host *self){
	g_return_val_if_fail(BUS_IS_HOST(self), NULL);
	return (self->name->str);
}

const gchar *host_get_hostname(Host *self){
	g_return_val_if_fail(BUS_IS_HOST(self), NULL);
	return (self->hostname->str);
}

const gchar *host_get_rsync_opts(Host *self){
	g_return_val_if_fail(BUS_IS_HOST(self), NULL);
	return (self->rsync_opts->str);
}

const gchar *host_get_backupdir(Host *self){
	g_return_val_if_fail(BUS_IS_HOST(self), NULL);
	return (self->backupdir->str);
}

const gchar *host_get_ip(Host *self){
	g_return_val_if_fail(BUS_IS_HOST(self), NULL);
	return (self->ip->str);
}

gint host_get_max_incr(Host *self){
	g_return_val_if_fail(BUS_IS_HOST(self), -1);
	return (self->max_incr);
}

gdouble host_get_max_age_incr(Host *self){
	g_return_val_if_fail(BUS_IS_HOST(self), -1.0);
	return (self->max_age_incr);
}

gdouble host_get_max_age_full(Host *self){
	g_return_val_if_fail(BUS_IS_HOST(self), -1.0);
	return (self->max_age_full);
}


gboolean host_ping(Host *host, gchar *ip){
	g_return_val_if_fail(BUS_IS_HOST(host), FALSE);

	if (ip == NULL){
		ip = host->ip->str;
	}

	gint status;
	gchar *argv[4];
	argv[0] = "/bin/ping";
	argv[1] = "-c 1";
	argv[2] = ip;
	argv[3] = NULL;
	GError *error = NULL;

	if (g_spawn_sync(NULL, argv, NULL, G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, NULL, NULL, &status, &error)){
		return (WEXITSTATUS(status) == 0);
	}
	else {
		g_printerr("spawn_async() failed: %s", error->message);
		g_error_free(error);
	}
	return (FALSE);
}

gchar *host_retrieve_hostname(Host *host, gchar *ip){
	g_return_val_if_fail(BUS_IS_HOST(host), NULL);

	if (ip == NULL){
		ip = host->ip->str;
	}

	gchar *argv[6];
	argv[0] = "/usr/bin/ssh";
	argv[1] = "-l";
	argv[2] = "root";
	argv[3] = ip;
	argv[4] = "hostname";
	argv[5] = NULL;
	GError *error = NULL;
	gint status;
	gchar *stdout;

	if (g_spawn_sync(NULL, argv, NULL, G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, &stdout, NULL, &status, &error)){
		if (WEXITSTATUS(status) == 0){
			return (g_strchomp(stdout));
		}
	}
	else {
		g_printerr("spawn_async() failed: %s", error->message);
		g_error_free(error);
	}
	return (NULL);
}

gboolean host_is_on_schedule(Host *self){
	GList *s;
	time_t now;
	gchar tstr[10];

	g_return_val_if_fail(BUS_IS_HOST(self), FALSE);
	if (self->schedule == NULL){
		return (TRUE);
	}
	
	now = time(NULL);
	strftime(tstr, sizeof(tstr), "%H:%M", localtime(&now));
	for (s = self->schedule; s != NULL; s = s->next){
		if (g_strcmp0(s->data, tstr) == 0){
			return (TRUE);
		}
	}
	return (FALSE);
}

gboolean host_is_online(Host *self){
	g_return_val_if_fail(BUS_IS_HOST(self), FALSE);

	gchar *h;
	GList *ipptr;

	if (host_ping(self, NULL)){
		h = host_retrieve_hostname(self, NULL);
		if (g_strcmp0(h, self->hostname->str) == 0){
			return (TRUE);
		}
	}

	for (ipptr = self->ips; ipptr != NULL; ipptr = ipptr->next){
		if (host_ping(self, (gchar*)ipptr->data)){
			h = host_retrieve_hostname(self, ipptr->data);
			if (g_strcmp0(h, self->hostname->str) == 0){
				g_string_assign(self->ip, ipptr->data);
				return (TRUE);
			}
		}
	}
	return (FALSE);
}

static gchar *string_list_concat(GList *list, const gchar *delim){
	gchar *s = NULL, *s1;
	GList *e;
	for (e = list; e != NULL; e = e->next){
		if (s == NULL){
			s = g_strdup(e->data);
		}
		else {
			s1 = s;
			s = g_strjoin(delim, s, e->data, NULL);
			g_free(s1);
		}
	}
	return (s);
}

void host_dump(Host *host){
	GList *excludes, *includes, *srcdirs, *ips, *schedule;
	gchar *str;

	g_return_if_fail(BUS_IS_HOST(host));
	
	g_print("******** Host Dump *******************\n");
	g_print("Name:         %s\n", host_get_name(host));
	g_print("Hostname:     %s\n", host_get_hostname(host));
	g_print("Rsync Opts:   %s\n", host_get_rsync_opts(host));
	g_print("Max Incr:     %d\n", host_get_max_incr(host));
	g_print("Max Age Incr: %f\n", host_get_max_age_incr(host));
	g_print("Max Age Full: %f\n", host_get_max_age_full(host));
	g_print("Backup Dir:   %s\n", host->backupdir->str);

	g_object_get(G_OBJECT(host), "excludes", &excludes, "includes", &includes, "srcdirs", &srcdirs, "ips", &ips, "schedule", &schedule, NULL);
	
	str = string_list_concat(excludes, ", ");	g_print("Excludes:   %s\n", str);	g_free(str);
	str = string_list_concat(includes, ", ");	g_print("Includes:   %s\n", str);	g_free(str);
	str = string_list_concat(srcdirs, ", ");	g_print("Srcdirs:    %s\n", str);	g_free(str);
	str = string_list_concat(ips, ", ");		g_print("Ips:        %s\n", str);	g_free(str);
	str = string_list_concat(schedule, ", ");	g_print("Schedule:   %s\n", str);	g_free(str);
	g_print("---------------------------------------\n");
}

static void host_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec){
	g_return_if_fail (BUS_IS_HOST (object));

	Host *host = BUS_HOST(object);

	switch (prop_id)
	{
	case PROP_HOSTNAME:
		host_set_hostname(host, g_value_get_string(value));
		break;
	case PROP_NAME:
		host_set_name(host, g_value_get_string(value));
		break;
	case PROP_RSYNC_OPTS:
		host_set_rsync_opts(host, g_value_get_string(value));
		break;
	case PROP_IP:
		host_set_ip(host, g_value_get_string(value));
		break;
	case PROP_BACKUPDIR:
		host_set_backupdir(host, g_value_get_string(value));
		break;
	case PROP_MAX_INCR:
		host_set_max_incr(host, g_value_get_int(value));
		break;
	case PROP_MAX_AGE_INCR:
		host_set_max_age_incr(host, g_value_get_double(value));
		break;
	case PROP_MAX_AGE_FULL:
		host_set_max_age_full(host, g_value_get_double(value));
		break;
	case PROP_EXCLUDES:
		host->excludes = g_value_get_pointer(value);
		break;
	case PROP_INCLUDES:
		host->includes = g_value_get_pointer(value);
		break;
	case PROP_SRCDIRS:
		host->srcdirs = g_value_get_pointer(value);
		break;
	case PROP_IPS:
		host->ips = g_value_get_pointer(value);
		break;
	case PROP_SCHEDULE:
		host->schedule = g_value_get_pointer(value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void host_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec){
	g_return_if_fail (BUS_IS_HOST (object));

	Host *host = BUS_HOST(object);

	switch (prop_id)
	{
	case PROP_HOSTNAME:
		g_value_set_string(value, host->hostname->str);
		break;
	case PROP_NAME:
		g_value_set_string(value, host->name->str);
		break;
	case PROP_RSYNC_OPTS:
		g_value_set_string(value, host->rsync_opts->str);
		break;
	case PROP_IP:
		g_value_set_string(value, host->ip->str);
		break;
	case PROP_BACKUPDIR:
		g_value_set_string(value, host->backupdir->str);
		break;
	case PROP_MAX_INCR:
		g_value_set_int(value, host->max_incr);
		break;
	case PROP_MAX_AGE_INCR:
		g_value_set_double(value, host->max_age_incr);
		break;
	case PROP_MAX_AGE_FULL:
		g_value_set_double(value, host->max_age_full);
		break;
	case PROP_EXCLUDES:
		g_value_set_pointer(value, host->excludes);
		break;
	case PROP_INCLUDES:
		g_value_set_pointer(value, host->includes);
		break;
	case PROP_SRCDIRS:
		g_value_set_pointer(value, host->srcdirs);
		break;
	case PROP_IPS:
		g_value_set_pointer(value, host->ips);
		break;
	case PROP_SCHEDULE:
		g_value_set_pointer(value, host->schedule);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void host_created (Host *self){
	/* TODO: Add default signal handler implementation here */
}


static void host_class_init (HostClass *klass){
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
/*
	GObjectClass* parent_class = G_OBJECT_CLASS (klass);
*/

	object_class->finalize = host_finalize;
	object_class->set_property = host_set_property;
	object_class->get_property = host_get_property;

	klass->created = host_created;

	g_object_class_install_property (object_class,
	                                 PROP_HOSTNAME,
	                                 g_param_spec_string("hostname", "Hostname", "The computer's hostname", "NoHostname", G_PARAM_READWRITE)
									);
	g_object_class_install_property (object_class,
	                                 PROP_NAME,
	                                 g_param_spec_string("name", "Name", "Name to idenftify host", "NoName", G_PARAM_READWRITE)
									);
	g_object_class_install_property (object_class,
	                                 PROP_RSYNC_OPTS,
	                                 g_param_spec_string("rsync_opts", "rsync-opts", "Options passed to Rsync", "-ax", G_PARAM_READWRITE)
									);
	g_object_class_install_property (object_class,
	                                 PROP_IP,
	                                 g_param_spec_string("ip", "ip", "IP Adress", "", G_PARAM_READWRITE)
									);
	g_object_class_install_property (object_class,
	                                 PROP_BACKUPDIR,
	                                 g_param_spec_string("backupdir", "BackupDir", "Backup Destination", "", G_PARAM_READWRITE)
									);
	g_object_class_install_property (object_class,
	                                 PROP_MAX_INCR,
	                                 g_param_spec_int("max_incr", "max-incr", "Max nr. of incremental backups", 0, 100, 7, G_PARAM_READWRITE)
									);
	g_object_class_install_property (object_class,
	                                 PROP_MAX_AGE_INCR,
	                                 g_param_spec_double("max_age_incr", "max-age-incr", "Max Age of incremental backups in days", 0.0, 3560.0, 1.0, G_PARAM_READWRITE)
									);
	g_object_class_install_property (object_class,
	                                 PROP_MAX_AGE_FULL,
	                                 g_param_spec_double("max_age_full", "max-age-full", "Max Age of incremental backups in days", 0.0, 3560.0, 7.0, G_PARAM_READWRITE)
									);
	g_object_class_install_property (object_class,
	                                 PROP_EXCLUDES,
	                                 g_param_spec_pointer("excludes", "excludes", "Excludes", G_PARAM_READWRITE)
									);
	g_object_class_install_property (object_class,
	                                 PROP_INCLUDES,
	                                 g_param_spec_pointer("includes", "includes", "Includes", G_PARAM_READWRITE)
									);
	g_object_class_install_property (object_class,
	                                 PROP_SRCDIRS,
	                                 g_param_spec_pointer("srcdirs", "srcdirs", "Source Directories", G_PARAM_READWRITE)
									);
	g_object_class_install_property (object_class,
	                                 PROP_IPS,
	                                 g_param_spec_pointer("ips", "ips", "Ip Pool", G_PARAM_READWRITE)
									);
	g_object_class_install_property (object_class,
	                                 PROP_SCHEDULE,
	                                 g_param_spec_pointer("schedule", "schedule", "Schedule", G_PARAM_READWRITE)
									);

	host_signals[CREATED] = g_signal_new ("created", G_OBJECT_CLASS_TYPE (klass), G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (HostClass, created), NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

