# BUSY

BUSY is a backup system which runs on a Server and pulls silently backups from configured clients inside the LAN.

## How it works

BUSY runs as background daemon and wakes up every minute to check if there are any hosts on schedule to backup.

Backups are done via rsync __on the client__ over SSH.

BUSY does full and incremental backups in configurable intervals.

There is minimal client-side configuration - as long as clients are online and reachable by SSH they will be automatically backuped according to their individual schedule.

Manual backups can be triggered additionally.

BUSY will backup itself (the host it is running on) - just configure `localhost` as a client.

## Dependencies

In order to compile BUSY, you need `glib` and `libmysql` libraries.
In order to run BUSY you need `SSH`, `netcat` and `hostname` installed

## Installation

Download the archive and extract it. Run 

~~~
$ make
# make install
~~~

to compile and install the daemon.
For now the init script is must be started manually by issuing

~~~
#root@server# /etc/init.d/busyd start
~~~


## Prerequisites

In order to use BUSY, you will need the following prerequisites.
On the host that BUSY is running on (server), we need `netcat` and `ssh` installed
On the clients we need `ssh` and `hostname` installed. In case there is no `hostname` you can emulate it by writing your own at `/bin/hostname`

~~~
#!/bin/sh
echo $HOSTNAME
~~~

Make sure it is executable:
~~~
root@server# chmod +x /bin/hostname
~~~

## Setting up SSH

BUSY needs to be able to connect from and to client/ server through SSH as `root`. This means that on each client you need to have SSH installed and the ssh server running (sshd);
then you need private key authentication between the roots of every machine.

For each client do (as root):

~~~
root@client# ssh-keygen -t rsa
~~~

Enter __no__ passphrase and confirm all defaults

~~~
root@client# ssh-copy-id -i ~/.ssh/id_rsa.pub root@server
~~~

where `server` is the Servers IP or hostname (the host where BUSY is running on)

And on the server (as root):

~~~
root@server# ssh-keygen -t rsa
root@server# ssh-copy-id -i ~/.ssh/id_rsa.pub root@client
~~~
where `client` is the client's ip or hostname


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
schedule :		Specify a list of times, when to look for backups
max_incr : 		Give the number of maximum incremental backups before a new full backup is done
max_age_full : 	give the max age in days (decimals allowed) that a full backup may be old.
max_age_incr : 	give the max age in days (decimals allowed) that an incremental backup may be old. If the newest incremental backup is older than specified here or there are more than max_incr incremental backups, then a full backup will be triggered.
rsync_opts : 	you can specify the options passed to rsync here. The defaults are quiet sane and should rarely be modified...
~~~

Next follows a list of clients, where all of the above settings can be overriden per host.
In this section, give each host a name (identifier) and the hostname. This must be the hostname that is printed out by the hostname command on that client.

# Web interface

I have written a simple web interface for busy which shows nice formatted status and summaries and allows for running manual backups.
To use it, you will need a running web server (apache) and MySQL.
PHP must be able to issue the `exec` command (safe mode off)

Create a table...

bla bla



