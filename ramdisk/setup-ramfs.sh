#!/bin/bash

# Crea un RAMDISK, montalo e setta i giusti permessi, copia queste righe in /etc/rc.local
# 
#   /etc/openhab2/ramdisk/setup-ramfs.sh
#   mkdir /var/log/openhab2
#   chmod 777 /var/log/openhab2
#   mkdir /var/log/mosquitto
#   touch /var/log/mosquitto/mosquitto.log
#   chmod 777 /var/log/mosquitto/mosquitto.log
######
#copia questo script nella dir /etc/openhab2/ramdisk/setup-ramfs.sh

RAM_DISK="/ramfs"
RAM_DISK_SIZE=64M

# Create RAM Disk ##########################
if [ ! -z "$RAM_DISK" ]; then
echo "Creating RAM Disk... $RAM_DISK"
mkdir -p $RAM_DISK
chmod 777 $RAM_DISK
mount -t tmpfs -o size=$RAM_DISK_SIZE tmpts $RAM_DISK/
echo "RAM Disk created at $RAM_DISK" 
 fi
 ############################################
 ## copia la seguente righa in /etc/fstab
 #/ramfs /var/log/ tmpfs defaults,noatime,nosuid,mode=0755,size=64m    0 0
 #cancella la dir /var/log/openhab2 presente dopo l'installazione
