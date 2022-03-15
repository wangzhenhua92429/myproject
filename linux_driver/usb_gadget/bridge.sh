#!/bin/sh

do_start() {
	mount -t configfs none /sys/kernel/config
	mkdir /sys/kernel/config/usb_gadget/g1
	cd /sys/kernel/config/usb_gadget/g1
	echo "0x200" > bcdUSB
	echo "0x100" > bcdDevice
	echo "0x03FD" > idVendor
	echo "0x0500" > idProduct
	
	mkdir -p strings/0x409
	echo "0" > strings/0x409/serialnumber
	echo `uname -r` > strings/0x409/manufacturer
	echo `hostname -s` > strings/0x409/product
	
	mkdir -p functions/Loopback.0
	
	mkdir -p configs/c.1
	echo 120 > configs/c.1/MaxPower
	ln -s functions/Loopback.0 configs/c.1/
	echo "fe200000.dwc3" > UDC
}

do_stop() {
	# 卸载USB�
	cd /sys/kernel/config/usb_gadget/g1
	echo "" > UDC
	rm configs/c.1/acm.gs0/
	rmdir configs/c.1/
	rmdir functions/acm.gs0/
	rmdir strings/0x409/
	cd ..
	rmdir g1/
	
	rmmod u_serial.ko
	rmmod usb_f_serial.ko
	rmmod usb_f_acm.ko 
	rmmod libcomposite.ko
}

case $1 in
    start)
        echo "Start usb gadget "
        do_start 
        ;;
    stop)
        echo "Stop usb gadget"
        do_stop
        ;;
    *)
        echo "Usage: $0 (stop | start)"
        ;;
esac

