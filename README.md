# BUSY

BUSY is a backup system which runs on a Server and pulls silently backups from configured clients inside the LAN.

## How it works

BUSY is a daemon which runs in the background. It wakes up every minute and checks if there are any hosts on schedule to backup.
Backups are done via rsync __on the client__ over ssh.

## Prerequisites

In order to use BUSY, you will need the following prerequisites.
On the host that BUSY is running on (server), we need `netcat` and `ssh` installed
On the clients we need `ssh` and `hostname` installed. In case there is no `hostname` you can emulate it by dropping this file to `/bin`

~~~
#!/bin/sh
echo $HOSTNAME
~~~

The clients and server need to be configured that they have SSH access on each other via _public key authentication_ as user `root`. 
(Maybe this will change in the future and there will be a seperate user that BUSY runs as)

# Installation

Download the archive and extract it. Run 

~~~
$ make
# make install
~~~

to complie and install the daemon.
The init script is must be started manually by now by issuing

~~~
/etc/init.d/busyd start
~~~

# Configuration

Configuration of hosts is done by editing the file `/etc/busy/busy.conf`:


# Web interface

I have written a simple web interface for busy which shows nice formatted status and summaries and allows for running manual backups.
To use it, you will need a running web server (apache) and MySQL.
Create a table...
bla bla



