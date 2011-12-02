CC = gcc
OBJECTS = main.o host.o backup.o job.o db.o
LIBS = `pkg-config --libs libconfig glib-2.0 gio-2.0` -L/usr/lib/mysql -lmysqlclient
CFLAGS = `pkg-config --cflags libconfig glib-2.0` -I/usr/include/mysql

autobus: $(OBJECTS)
	$(CC) -Wall -g -o $@ $(OBJECTS) $(LIBS)

%.o: %.c %.h
	$(CC) -Wall -g -c $(CFLAGS) $<

install:
	cp ./autobus /usr/local/bin/
	cp init.d/autobus /etc/init.d/
#	ln -s /etc/rc0.2/K02 /etc/init.d/autobus		!FIXMEq
	chmod +x /etc/init.d/autobus
	mkdir -p /var/lock/autobus

clean:
	rm *.o
