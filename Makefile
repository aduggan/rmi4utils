all:
	make -C rmidevice all
	make -C rmi4update all
	make -C rmihidtool all

clean:
	make -C rmidevice clean
	make -C rmi4update clean
	make -C rmihidtool clean
