#!/bin/sh
killall autobus
killall rsync
echo "DELETE FROM jobs WHERE 1; DELETE FROM backups WHERE 1" | mysql -u root --password='jmp$fce2' buddy
rm -rf /root/test/*
