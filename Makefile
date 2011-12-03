CC = gcc
OBJECTS = main.o host.o backup.o job.o db.o
LIBS = `pkg-config --libs libconfig glib-2.0 gio-2.0` -L/usr/lib/mysql -lmysqlclient
CFLAGS = `pkg-config --cflags libconfig glib-2.0` -I/usr/include/mysql
PREFIX=/usr/local/bin/

busyd: $(OBJECTS)
	$(CC) -Wall -g -o $@ $(OBJECTS) $(LIBS)

%.o: %.c %.h
	$(CC) -Wall -g -c $(CFLAGS) $<

install:
	mkdir -p /var/lock/busy
	mkdir -p /var/backups
	mkdir -p /etc/busy

	cp ./busyd ${PREFIX}
	cp ./init.d/busy /etc/init.d/
	cp ./conf/busy.conf /etc/busy/
	chmod +x /etc/init.d/busy

	[ -e /etc/rc2.d/S23busy ] || ln -s /etc/init.d/busy /etc/rc2.d/S23busy
	[ -e /etc/rc0.d/k23busy ] || ln -s /etc/init.d/busy /etc/rc0.d/k23busy
	[ -e /etc/rc6.d/k23busy ] || ln -s /etc/init.d/busy /etc/rc6.d/k23busy
	
uninstall:
	killall -9 busyd || echo "Nothing to kill"
	rm -rf /var/lock/busy
	rm -rf /etc/busy
	rm -f ${PREFIX}/busyd
	rm -f /etc/init.d/busy
	rm -f /etc/rc2.d/S23busy
	rm -f /etc/rc6.d/k23busy
	rm -f /etc/rc0.d/k23busy
		
clean:
	rm ./*.o
	rm ./busyd

