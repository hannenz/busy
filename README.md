# BUSY

- BUSY is a backup system which runs on a Server and silently pulls backups from configured clients inside a local network (or on the host itself).

- BUSY runs as background daemon and checks periodically if there are any hosts on schedule to backup.

- Backups are done via `rsync` __on the client__ over `SSH`.

- BUSY does full and incremental backups in configurable intervals as well as archiving backups.

- Client-side configuration is kept to a minimum: As long as clients are online and reachable by SSH they will be automatically backuped according to their individual schedule.

- Manual backups can be triggered additionally.

- BUSY will backup itself (the host it is running on) - just configure `localhost` as a client.

- BUSY runs without user interaction (daemon) but there is a web frontend which can be used to monitor backups and trigger manual backups.

- It comes with a web frontend to monitor and control backups, setup client configuration etc. Usage is optional, so it works perfectly without if you don't want or have a web server installed and running.

- BUSY is written in plain C (using Glib) and relies on standard UNIX/ Linux tools. It should run on any linux box out there without issues.

### How BUSY works

- BUSY daemon wakes up periodically and checks for clients that are online and on schedule

- if any, it checks, if this client(s) need a fresh backup and if yes, what type (full or incremental)

- Any necessary backups are queued

- In another process, BUSY looks up the queue and processes each queued backup

- If present, a pre backup script is run, e.g. to mount a backup disk, prepare sql dumps etc. 

- For each backup's source directory, BUSY creates a "job", which is a `Rsync` call. BUSY logs in to the client that needs to be backuped via `SSH` and issues the necessary Rsync command on the client with the rsync destination located back on the server.

- If all jobs for a backup have been finished (successful or not), a post-backup script is run to do any cleaning up, unmounting etc.

- Logfiles of the backup process and each rsync call are stored in the backup directory.

- Everything is (optionally) logged in a MySQL database where the web frontend reads out to monitor the whole backup status of all of your clients.

- If anything goes wrong, an Email is sent to a specified address, informing about the failure

- In another configurable interval, busy (optionally) archives backups that exceed a specified age. Archiving means, busy creates a gzipped tarball (`.tar.gz`)of the backup and moves it to a specified location. 

## Dependencies

In order to compile BUSY, you need `glib`, `libconfig` and `libmysql` libraries.

In order to run BUSY you need `SSH`, `rsync`, `netcat`, `hostname`, `ping`, `sendmail` installed. On almost every linux box, these should be installed, except for SSH. Make sure you have installed SSH server (`openssh`)

`sendmail` and general mail setup (postfix or what have you) must be ready configured. BUSY just calls the `sendmail` command and relies on it to deliver properly.

I develop and run BUSY on a Debian Lenny system and init scripts are written for debain distributions. If you need other init scripts, please feel free to adapt to your distro.

## Installation

Download the archive and extract it. Run 

~~~
$ make
~~~

to compile the daemon. Then install it by issuing

~~~
# make install
~~~

This will install the daemon to `/usr/sbin`, the initscript to `/etc/init.d/busy` and the config file to `/etc/busy/busy.conf`.
The initscript is symlinked to `/etc/rc0.d`, `/etc/rc2.d` and `/etc/rc6.d`, so it will be started at boot and stopped at shutdown or reboot (on a Debian system; for other LINUX distributions, please adjust manually).

The daemon will be started during the next boot. To fire it up immediately type

~~~
#root@server# /etc/init.d/busyd start
~~~

## Prerequisites

In order to use BUSY, you will need to meet the following prerequisites:

On the host that BUSY is running on (server), we need `netcat`, `ssh` and `sendmail` installed. (Do we still need netcat???)

On the clients we need `ssh`, `rsync` and `hostname` installed. In case there is no `hostname` you can emulate it by writing your own at `/bin/hostname`

~~~
#!/bin/sh
echo $HOSTNAME
~~~

Make sure it is executable:

~~~
root@server# chmod +x /bin/hostname
~~~

When talking about `SSH` we always need both the client and server suite (e.g. `openssh-server`)


## Setting up SSH

BUSY needs to be able to connect from and to client/ server through SSH. This means that on each client you need to have SSH installed and the ssh server running (sshd);
then you need public key authentication between the `root` and `client-user` of every machine.

On each client to create a public key do (as the user you want the rsync command to be run):

~~~
user@client# ssh-keygen -t rsa
~~~

Enter __no__ passphrase and confirm all defaults. Then copy the key to the server:

~~~
root@client# ssh-copy-id -i ~/.ssh/id_rsa.pub root@server
~~~

where `server` is the Servers IP or hostname (the host where BUSY is running on)

After you have setup your clients, do the same vice versa on the server (again as root):

~~~
root@server# ssh-keygen -t rsa
root@server# ssh-copy-id -i ~/.ssh/id_rsa.pub user@client
~~~

where `user` is the user on the client that shall run `rsync` and `client` is the client's ip or hostname. Repeat the last step for each client.

To test if everything is setup correctly, try to login from client to server and then back from server to client:

~~~
root@client# ssh root@server
root@server# ssh user@client hostname
~~~

If you are able to do this "double login" (without entering passwords or passphrases) and the last command prints out the correct hostname for your client, everything is setup fine and you are done.

__To backup itself, be sure you have the same access on the server's localhost__


__Attention__

To be sure that rsync can read all files you want to backup, it might be tempting to use `root` as client user. Be aware that this might be a __security risk__!


# Configuration

Configuration of hosts is done either through a config file located at `/etc/busy/busy.conf` or by using the web frontend.

Busy tries first to read the config file and if it finds no host configurations in there, tries to read from MySQL database (web frontend configuration).

The config file must be present in both cases since it must provide the MySQL user and password in order to connect to the databse.

That means, whether you want or want not to use the web frontend, do the following

<dl>
<dt>No Web frontend</dt>
<dd>Do all configuration in /etc/busy/busy.conf. Omit the settings for `mysql_login` amd `mysql_password`</dd>
<dt>Use Web frontend</dt>
<dd>Specify `mysql_login` and `mysql_password` in `/etc/busy/busy.conf` and leave the rest of the file empty (or comment out)</dd>
</dl>

In case you give mysql credentials in /etc/busy/busy.conf AND host(s) configurations, the hosts configuratiosn are taken from there and you will be able to use the web frontend for monitoring, but any changes you will do to client setup in the web frontend won't be effective!! You better avoid this scenario for clearance and disambigouity.

## Config file /etc/bus/busy.conf

There are three sections. First is the MySQL setup. This is only needed if you want to use the web frontend. In this case enter the MySQL username and password to connect to MySQL. (Make sure you have created the database `busy` and the tables found in `webfrontend/busy.sql`).

Next section defines default settings that are valid for all clients. The available settings are as follows:

~~~
backupdir :		specify the directory where the backup shall be stored to.
name : 			leave as 'default' in the default section
hostname : 		leave as 'None' in the default section
user : 			specify the user as which the rsync command will be running on the client
email : 		optonally give a ameila address to send notificatin on failures
srcdirs : 		specify a list of source directories that shall be backed up
excludes : 		List of patterns to exclude (passed to Rsync)
schedule :		Specify a list of times, when to look for backups
max_incr : 		Give the number of maximum incremental backups before a new full backup is done
max_age_full : 	give the max age in days (decimals allowed) that a full backup may be old.
max_age_incr : 	give the max age in days (decimals allowed) that an incremental backup may be old. If the newest incremental backup is older than specified here or there are more than max_incr incremental backups, then a full backup will be triggered.
rsync_opts : 	you can specify the options passed to rsync here. The defaults are quiet sane and should rarely be modified...
max_age : 		Max age of backups before getting archived
archivedir :	Destination directory to store the archives
~~~

Next follows a list of clients, where all of the above settings can be overriden per host.
In this section, give each host a name (identifier) and the hostname. This must be the hostname that is printed out by the `hostname` command on that client (see above).

## Web interface

I have written a simple web interface for busy which shows nice formatted status and summaries and allows for running manual backups.
To use it, you will need a running web server (apache) and MySQL.

The web frontend is now included in the repository, but you have to install it manually until now. Copy the files in `webfrontend/busy` to a location where your web server can serve it and prepare the database by creating the necessary tables found in the SQL file in `webfrontend/busy.sql`.
For now i hope you know how to set up your web server and all; In some time i will write a seperate installer for the web frontend and a better documentation; please stay tunde...

The web frontend is in a very "raw state" until now but it is usable.

# Pre-/Post backup scripts

In order to do anything before and/ or after each backup, the scripts `/etc/busy/pre-backup` and `/etc/busy/post-backup` can be used. They are called before/ after each backup if present and executable. (Shell scripts called with `/bin/sh`).

## pre-backup

The script `/etc/busy/pre-backup` is executed before the rsync command is executed. It is a handy place to mount a seperate backup disk, stop a mysql server and pull a sql dump or anything else you want to be sure to be done before backing up.
Be aware that the backup will be started only if this script exits with return code 0, otherwise the backup will be aborted and marked as "failure".

The script is executed with the parameters

~~~
$1	#Type of the backup that will be run ("full" or "incr")
$2 	#Name of the host that will be backuped
$3	#Destination path that the backup will be stored at
~~~

## post-backup

The script `/etc/busy/post-backup` is executed after a backup has been finished with the parameters:

~~~
$1	#Name of the host
$2	#State: "success" or "failure"
$3	#Full path to the backup
~~~

# Pre-/Post archive scripts

Are not implemented yet, but will be soon ;)


