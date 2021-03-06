#include <sys/wait.h>
#include <string.h>
#include <ctype.h>
#include <glib/gstdio.h>

#include "host.h"
#include "busy.h"
#include "db.h"

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
	PROP_SCHEDULE,
	PROP_USER,
	PROP_MYSQL_ID,
	PROP_MAX_AGE,
	PROP_ARCHIVEDIR,
	PROP_EMAIL
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
	host->user = g_string_new(NULL);
	host->rsync_opts = g_string_new(NULL);
	host->ip = g_string_new(NULL);
	host->backupdir = g_string_new(NULL);
	host->archivedir = g_string_new(NULL);
	host->srcdirs = NULL;
	host->excludes = NULL;
	host->includes = NULL;
	host->ips = NULL;
	host->schedule = NULL;
	host->email = g_string_new(NULL);
}

static void host_finalize (GObject *object) {
	Host *host = BUS_HOST(object);
	syslog(LOG_NOTICE, "Finalizing host: %s\n", host->name->str);
	g_string_free(host->name, TRUE);
	g_string_free(host->hostname, TRUE);
	g_string_free(host->user, TRUE);
	g_string_free(host->rsync_opts, TRUE);
	g_string_free(host->ip, TRUE);
	g_string_free(host->backupdir, TRUE);
	g_string_free(host->archivedir, TRUE);
	g_list_free(host->srcdirs);
	g_list_free(host->excludes);
	g_list_free(host->includes);
	g_list_free(host->ips);
	g_list_free(host->schedule);
	g_string_free(host->email, TRUE);

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
	const gchar *name, *hostname, *rsync_opts, *backupdir, *user, *archivedir, *email;
	long int mi;
	gint max_incr;
	gdouble max_age_incr, max_age_full, max_age;

	if ((host = host_new()) == NULL){
		return (NULL);
	}

	if (config_setting_lookup_string(cs, "name", &name) == CONFIG_FALSE){
		name = NULL;
	}
	if (config_setting_lookup_string(cs, "hostname", &hostname) == CONFIG_FALSE){
		hostname = NULL;
	}

	if (config_setting_lookup_string(cs, "backupdir", &backupdir) == CONFIG_FALSE){
		if ((config_lookup_string(config, "default.backupdir", &backupdir)) == CONFIG_FALSE){
			backupdir = NULL;
		}
	}
	if (config_setting_lookup_string(cs, "archivedir", &archivedir) == CONFIG_FALSE){
		if ((config_lookup_string(config, "default.archivedir", &archivedir)) == CONFIG_FALSE){
			archivedir = NULL;
		}
	}

	if (config_setting_lookup_string(cs, "user", &user) == CONFIG_FALSE){
		if ((config_lookup_string(config, "default.user", &user) == CONFIG_FALSE)){
			user = NULL;
		}
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
	if (config_setting_lookup_float(cs, "max_age", &max_age) == CONFIG_FALSE){
		if (config_lookup_float(config, "default.max_age", &max_age) == CONFIG_FALSE){
			max_age = 365;
		}
	}

	if (config_setting_lookup_string(cs, "email", &email) == CONFIG_FALSE){
		if (config_lookup_string(config, "default.email", &email) == CONFIG_FALSE){
			email = NULL;
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
		"user", user,
		"rsync_opts", rsync_opts,
		"max_incr", max_incr,
		"max_age_incr", max_age_incr,
		"max_age_full", max_age_full,
		"max_age", max_age,
		"archivedir", archivedir,
		"email", email,
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
				syslog(LOG_WARNING, "Invalid IP: %s, skipping...\n", s);
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
				syslog(LOG_WARNING, "Invalid Schedule Time: %s, skipping...\n", s);
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
	return (b->age < a->age);
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
				syslog(LOG_NOTICE, "Removing backup: %s\n", dpath);
				delete_folder_tree(dpath);
				g_free(dpath);
			}
		}
		g_dir_close(dir);
	}
	g_free(path);
}

static void on_sendmail_exit(GPid pid, gint status, gpointer udata){
	syslog(LOG_NOTICE, "sendmail exitet with code %u", WEXITSTATUS(status));
}


gint host_send_email(Host *self, const gchar *subject, const gchar *message){
	gchar *argv[5];
	GError *error = NULL;
	GPid pid;
	gint stdin;

	g_return_val_if_fail(BUS_IS_HOST(self), -1);
	g_return_val_if_fail(strlen(self->email->str) > 0, -1);

	syslog(LOG_NOTICE, "Sending email to %s", self->name->str);

	argv[0] = "/usr/bin/mail";
	argv[1] = g_strdup_printf("-s %s", subject);
	argv[2] = self->email->str;
	argv[3] = NULL;

	g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid, &stdin, NULL, NULL, &error);
	if (error){
		syslog(LOG_NOTICE, "Error: %s", error->message);
		g_error_free(error);
	}
	g_child_watch_add(pid, on_sendmail_exit, self);
	GIOChannel *in;
	error = NULL;
	in = g_io_channel_unix_new(stdin);
	g_io_channel_write_chars(in, message, -1, NULL, &error);
	if (error != NULL){
		syslog(LOG_WARNING, "g_io_channel_write_chars() failed");
		g_error_free(error);
	}

	g_io_channel_close(in);

	g_free(argv[1]);

	return (0);
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

void host_set_archivedir(Host *self, const gchar *archivedir){
	g_return_if_fail(BUS_IS_HOST(self));
	g_string_assign(self->archivedir, archivedir);
	g_object_notify(G_OBJECT(self), "archivedir");
}

void host_set_user(Host *self, const gchar *user){
	g_return_if_fail(BUS_IS_HOST(self));
	g_string_assign(self->user, user),
	g_object_notify(G_OBJECT(self), "user");
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

void host_set_max_age(Host *self, gdouble max_age){
	g_return_if_fail(BUS_IS_HOST(self));
	self->max_age = max_age;
	g_object_notify(G_OBJECT(self), "max_age");
}

void host_add_exclude(Host *self, const gchar *exclude){
	g_return_if_fail(BUS_IS_HOST(self));
	gchar *e = g_strdup(exclude);
	if (!g_list_find_custom(self->excludes, e, (GCompareFunc)g_strcmp0)){
		self->excludes = g_list_append(self->excludes, e);
		g_object_notify(G_OBJECT(self), "excludes");
	}
	else {
		g_free(e);
	}
}

void host_add_include(Host *self, const gchar *include){
	g_return_if_fail(BUS_IS_HOST(self));
	self->includes = g_list_append(self->includes, g_strdup(include));
	g_object_notify(G_OBJECT(self), "includes");
}

void host_add_srcdir(Host *self, const gchar *srcdir){
	g_return_if_fail(BUS_IS_HOST(self));

	gchar *s = rtrim(g_strdup(srcdir));
	if (!g_list_find_custom(self->srcdirs, s, (GCompareFunc)g_strcmp0)){
		self->srcdirs = g_list_append(self->srcdirs, s);
		g_object_notify(G_OBJECT(self), "srcdirs");
	}
	else {
		g_free(s);
	}
}

void host_add_ip(Host *self, const gchar *ip){
	g_return_if_fail(BUS_IS_HOST(self));

	gchar *i = g_strdup(ip);
	if (!g_list_find_custom(self->ips, i, (GCompareFunc)g_strcmp0)){
		self->ips = g_list_append(self->ips, i);
		g_object_notify(G_OBJECT(self), "ips");
	}
	else {
		g_free(i);
	}
}

void host_add_schedule(Host *self, const gchar *schedule){
	g_return_if_fail(BUS_IS_HOST(self));

	gchar *s = g_strdup(schedule);
	if (!g_list_find_custom(self->schedule, s, (GCompareFunc)g_strcmp0)){
		self->schedule = g_list_append(self->schedule, s);
		g_object_notify(G_OBJECT(self), "schedule");
	}
	else {
		g_free(s);
	}
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

const gchar *host_get_archivedir(Host *self){
	g_return_val_if_fail(BUS_IS_HOST(self), NULL);
	return (self->archivedir->str);
}

const gchar *host_get_user(Host *self){
	g_return_val_if_fail(BUS_IS_HOST(self), NULL);
	return (self->user->str);
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

gdouble host_get_max_age(Host *self){
	g_return_val_if_fail(BUS_IS_HOST(self), -1.0);
	return (self->max_age);
}

gint host_get_mysql_id(Host *self){
	g_return_val_if_fail(BUS_IS_HOST(self), -1);
	return (self->mysql_id);
}

void host_set_mysql_id(Host *self, gint mysql_id){
	g_return_if_fail(BUS_IS_HOST(self));
	self->mysql_id = mysql_id;
	g_object_notify(G_OBJECT(self), "mysql_id");
}

void host_set_email(Host *self, const gchar *email){
	g_return_if_fail(BUS_IS_HOST(self));
	g_string_assign(self->email, email);
	g_object_notify(G_OBJECT(self), "email");
}

const gchar *host_get_email(Host *self){
	g_return_val_if_fail(BUS_IS_HOST(self), NULL);
	return (self->email->str);
}

gboolean host_ping(Host *host, gchar *ip){
	gint status;
	gchar *argv[4];
	GError *error = NULL;

	g_return_val_if_fail(BUS_IS_HOST(host), FALSE);

	if (ip == NULL){
		ip = host->ip->str;
	}
	g_return_val_if_fail(is_valid_ip(ip), FALSE);

	argv[0] = "/bin/ping";
	argv[1] = "-c 1";
	argv[2] = ip;
	argv[3] = NULL;

	if (g_spawn_sync(NULL, argv, NULL, G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, NULL, NULL, &status, &error)){
		return (WEXITSTATUS(status) == 0);
	}
	else {
		syslog(LOG_ERR, "spawn_async() failed: %s", error->message);
		g_error_free(error);
	}
	return (FALSE);
}

gchar *host_retrieve_hostname(Host *host, gchar *ip){
	g_return_val_if_fail(BUS_IS_HOST(host), NULL);

	if (ip == NULL){
		ip = host->ip->str;
	}

	if (g_strcmp0(host->ip->str, "127.0.0.1") == 0){
		return "localhost";
	}

	gchar *argv[6];
	argv[0] = "/usr/bin/ssh";
	argv[1] = "-l";
	argv[2] = DEFAULT_USER;
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
		syslog(LOG_ERR, "spawn_async() failed: %s", error->message);
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
		syslog(LOG_DEBUG, "ping has been positive");
		h = host_retrieve_hostname(self, NULL);
		syslog(LOG_DEBUG, "host_retrieve_hostname() returned %s, self->hostname is %s", h, self->hostname->str);
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

	syslog(LOG_NOTICE, "******** Host Dump *******************\n");
	syslog(LOG_NOTICE, "Name:         %s\n", host_get_name(host));
	syslog(LOG_NOTICE, "Hostname:     %s\n", host_get_hostname(host));
	syslog(LOG_NOTICE, "Rsync Opts:   %s\n", host_get_rsync_opts(host));
	syslog(LOG_NOTICE, "Max Incr:     %d\n", host_get_max_incr(host));
	syslog(LOG_NOTICE, "Max Age:      %f\n", host_get_max_age(host));
	syslog(LOG_NOTICE, "Max Age Incr: %f\n", host_get_max_age_incr(host));
	syslog(LOG_NOTICE, "Max Age Full: %f\n", host_get_max_age_full(host));
	syslog(LOG_NOTICE, "Backup Dir:   %s\n", host->backupdir->str);
	syslog(LOG_NOTICE, "Archive Dir:   %s\n", host->archivedir->str);

	g_object_get(G_OBJECT(host), "excludes", &excludes, "includes", &includes, "srcdirs", &srcdirs, "ips", &ips, "schedule", &schedule, NULL);

	str = string_list_concat(excludes, ", ");	syslog(LOG_NOTICE, "Excludes:   %s\n", str);	g_free(str);
	str = string_list_concat(includes, ", ");	syslog(LOG_NOTICE, "Includes:   %s\n", str);	g_free(str);
	str = string_list_concat(srcdirs, ", ");	syslog(LOG_NOTICE, "Srcdirs:    %s\n", str);	g_free(str);
	str = string_list_concat(ips, ", ");		syslog(LOG_NOTICE, "Ips:        %s\n", str);	g_free(str);
	str = string_list_concat(schedule, ", ");	syslog(LOG_NOTICE, "Schedule:   %s\n", str);	g_free(str);
	syslog(LOG_NOTICE, "---------------------------------------\n");
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
	case PROP_ARCHIVEDIR:
		host_set_archivedir(host, g_value_get_string(value));
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
	case PROP_MAX_AGE:
		host_set_max_age(host, g_value_get_double(value));
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
	case PROP_USER:
		host_set_user(host, g_value_get_string(value));
		break;
	case PROP_MYSQL_ID:
		host_set_mysql_id(host, g_value_get_int(value));
		break;
	case PROP_EMAIL:
		host_set_email(host, g_value_get_string(value));
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
	case PROP_ARCHIVEDIR:
		g_value_set_string(value, host->archivedir->str);
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
	case PROP_MAX_AGE:
		g_value_set_double(value, host->max_age);
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
	case PROP_USER:
		g_value_set_string(value, host->user->str);
		break;
	case PROP_MYSQL_ID:
		g_value_set_int(value, host->mysql_id);
		break;
	case PROP_EMAIL:
		g_value_set_string(value, host->email->str);
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
	                                 PROP_ARCHIVEDIR,
	                                 g_param_spec_string("archivedir", "ArchiveDir", "Archive Destination", "", G_PARAM_READWRITE)
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
	                                 PROP_MAX_AGE,
	                                 g_param_spec_double("max_age", "max-age", "Max Age of full backup before it gets archived", 0.0, 3560.0, 265, G_PARAM_READWRITE)
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
	g_object_class_install_property (object_class,
	                                 PROP_USER,
	                                 g_param_spec_string("user", "user", "User", "root", G_PARAM_READWRITE)
									);
	g_object_class_install_property (object_class,
	                                 PROP_MYSQL_ID,
	                                 g_param_spec_int("mysql_id", "mysql-id", "MySQL Id", 0, G_MAXINT, 0, G_PARAM_READWRITE)
									);
	g_object_class_install_property(object_class,
									PROP_EMAIL,
									g_param_spec_string("email", "email", "E-Mail", "", G_PARAM_READWRITE)
									);

	host_signals[CREATED] = g_signal_new ("created", G_OBJECT_CLASS_TYPE (klass), G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (HostClass, created), NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

