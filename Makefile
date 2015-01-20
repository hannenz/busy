CC = gcc
OBJECTS = main.o host.o backup.o job.o db.o
LIBS = `pkg-config --libs libconfig glib-2.0 gio-2.0` -L/usr/lib/mysql -lmysqlclient
CFLAGS = `pkg-config --cflags libconfig glib-2.0` -I/usr/include/mysql
PREFIX=/usr/sbin
CONFDIR=/etc/busy
BACKUPDIR=/var/backups


busyd: $(OBJECTS)
	$(CC) -Wall -g -o $@ $(OBJECTS) $(LIBS)

%.o: %.c %.h
	$(CC) -Wall -g -c $(CFLAGS) $<

reinstall:
	cp ./busyd ${PREFIX}

install:
	test -d "${BACKUPDIR}" || mkdir -p "${BACKUPDIR}"
	test -d "${CONFDIR}" || mkdir -p "${CONFDIR}"

	cp ./busyd ${PREFIX}
	cp ./init.d/busy /etc/init.d/
	cp ./conf/busy.conf ${CONFDIR}
	cp ./conf/pre-backup ${CONFDIR}
	cp ./conf/post-backup ${CONFDIR}
	chmod +x ${CONFDIR}/pre-backup
	chmod +x ${CONFDIR}/post-backup
	chmod +x /etc/init.d/busy

	# Make links in rc.d
	[ -e /etc/rc2.d/S23busy ] || ln -s /etc/init.d/busy /etc/rc2.d/S23busy
	[ -e /etc/rc0.d/k23busy ] || ln -s /etc/init.d/busy /etc/rc0.d/k23busy
	[ -e /etc/rc6.d/k23busy ] || ln -s /etc/init.d/busy /etc/rc6.d/k23busy

installwebfrontend:
	mkdir -p /var/www/busy
	cp -Ra webfrontend/busy/* /var/www/busy/
	cp webfrontend/busy/.htaccess /var/www/busy/

	#FIXME!
	# Create MySQL Databes and tables...


uninstall:

	#kill any running daemons
	killall -9 busyd || echo "Nothing to kill"

	rm -rf "${CONFDIR}"
	rm -f ${PREFIX}/busyd
	rm -f /etc/init.d/busy
	rm -f /etc/rc2.d/S23busy
	rm -f /etc/rc6.d/k23busy
	rm -f /etc/rc0.d/k23busy

clean:
	rm ./*.o
	rm ./busyd


resetall:
	make uninstall
	./rmall.sh

