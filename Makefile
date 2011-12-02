CC = gcc
OBJECTS = main.o host.o backup.o job.o db.o
LIBS = `pkg-config --libs libconfig glib-2.0 gio-2.0` -L/usr/lib/mysql -lmysqlclient
CFLAGS = `pkg-config --cflags libconfig glib-2.0` -I/usr/include/mysql

busyd: $(OBJECTS)
	$(CC) -Wall -g -o $@ $(OBJECTS) $(LIBS)

%.o: %.c %.h
	$(CC) -Wall -g -c $(CFLAGS) $<

install:
	mkdir -p /var/lock/busy
	mkdir -p /var/backups
	mkdir -p /etc/busy

	cp ./busyd /usr/local/bin/
	cp ./init.d/busy /etc/init.d/
	cp ./conf/busy.conf /etc/busy/
	chmod +x /etc/init.d/busy
	

clean:
	rm *.o
