# BUSY

BUSY is a backup system which runs on a Server and pulls silently backups from configured clients inside the LAN.

BUSY runs as background daemon and checks periodically if there are any hosts on schedule to backup.

Backups are done via rsync __on the client__ over SSH.

BUSY does full and incremental backups in configurable intervals.

There is minimal client-side configuration - as long as clients are online and reachable by SSH they will be automatically backuped according to their individual schedule.

Manual backups can be triggered additionally.

BUSY will backup itself (the host it is running on) - just configure `localhost` as a client.

BUSY runs without user interaction (daemon) but there is a web frontend which can be used to monitor backups and trigger manual backups.

### How BUSY works

- BUSY daemon wakes up periodically and checks for clients that are online and on schedule

- if any, it checks, if this client(s) need a fresh backup and if yes, what tpye (full or incremental)

- Any necessary backups are queued

- In another process, BUSY looks up the queue and processes each queued backup

- For each backup's source directory, BUSY creates a JOB. A Job is indeed a Rsync call. BUSY logs in via SSH to the client that needs to be backuped and issues the desired Rsync command on the client where the rsync destination is locatet back on the server.

- Everything is (optionally) logged in a MySQL database where the web frontend reads out to monitor the whole backup status of all of your clients.


## Dependencies

In order to compile BUSY, you need `glib`, `libconfig` and `libmysql` libraries.
In order to run BUSY you need `SSH`, `netcat` and `hostname` installed

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

The daemon will be started during the next boot or you can manually fore it up by typing

~~~
#root@server# /etc/init.d/busyd start
~~~

## Prerequisites

In order to use BUSY, you will need to meet the following prerequisites:

- On the host that BUSY is running on (server), we need `netcat` and `ssh` installed

- On the clients we need `ssh` and `hostname` installed. In case there is no `hostname` you can emulate it by writing your own at `/bin/hostname`

When talking about `SSH` we always need both the client and server suite (e.g. `openssh-server`)

~~~
#!/bin/sh
echo $HOSTNAME
~~~

Make sure it is executable:

~~~
root@server# chmod +x /bin/hostname
~~~

__WARNING!__
All SSH connections are established between the root accounts of each machine! Please be aware that this will cause a __significant security risk__ if your LAN is reachable from outside! Please don't use BUSY if you are not sure, you can accept this risk!
I have also plans for letting BUSY and its child processes (ssh, rsync) run as non-priviledged user, but this might be in some far future - and it won't be easy because in order to do the backups BUSY needs (read) access to arbitrary files and directories, which would require non-trivial client-side setup which is exactly what i want to avoid.


## Setting up SSH

BUSY needs to be able to connect from and to client/ server through SSH as `root`. This means that on each client you need to have SSH installed and the ssh server running (sshd);
then you need public key authentication between the roots of every machine.

On each client to create a public key do (as root):

~~~
root@client# ssh-keygen -t rsa
~~~

Enter __no__ passphrase and confirm all defaults. Then copy the key to the server:

~~~
root@client# ssh-copy-id -i ~/.ssh/id_rsa.pub root@server
~~~

where `server` is the Servers IP or hostname (the host where BUSY is running on)

After you have setup your clients, do the same vice versa on the server (again as root):

~~~
root@server# ssh-keygen -t rsa
root@server# ssh-copy-id -i ~/.ssh/id_rsa.pub root@client
~~~

where `client` is the client's ip or hostname. Repeat the last step for each client.


To test if everything is setup correctly, try to login from client to server and then back from server to client:

~~~
root@client# ssh root@server
root@server# ssh root@client hostname
~~~

If you are able to do this "double login" (without entering passwords or passphrases) and the last command prints out the correct hostname for your client, everything is setup fine and you are done.

__To backup itself, be sure you have the same access on the server's localhost__


# Configuration

Configuration of hosts is done by editing the file `/etc/busy/busy.conf`:

There are three sections. First is the MySQL setup. This is only needed if you want to use the web frontend. In this case enter the MySQL username and password to connect to MySQL. (Make sure you have created the database `busy` and the tables found in `webfrontend/busy.sql`).

Next section defines default settings that are valid for all clients. The available settings are as follows:

~~~
backupdir :		specify the directory where the backup shall be stored to.
name : 			leave as 'default' in the default section
hostname : 		leave as 'None' in the default section
srcdirs : 		specify a list of source directories that shall be backed up
excludes : 		List of patterns to exclude (passed to Rsync)
schedule :		Specify a list of times, when to look for backups
max_incr : 		Give the number of maximum incremental backups before a new full backup is done
max_age_full : 	give the max age in days (decimals allowed) that a full backup may be old.
max_age_incr : 	give the max age in days (decimals allowed) that an incremental backup may be old. If the newest incremental backup is older than specified here or there are more than max_incr incremental backups, then a full backup will be triggered.
rsync_opts : 	you can specify the options passed to rsync here. The defaults are quiet sane and should rarely be modified...
~~~

Next follows a list of clients, where all of the above settings can be overriden per host.
In this section, give each host a name (identifier) and the hostname. This must be the hostname that is printed out by the `hostname` command on that client (see above).

# Web interface

I have written a simple web interface for busy which shows nice formatted status and summaries and allows for running manual backups.
To use it, you will need a running web server (apache) and MySQL.
PHP must be able to issue the `exec` command (safe mode off)

The web frontend is __not yet__ included in the repository. Please stay tuned....

