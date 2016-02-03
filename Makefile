NDK_PROJECT_PATH=$(PWD)
export NDK_PROJECT_PATH

all:
	$(MAKE) -C rmidevice all
	$(MAKE) -C rmi4update all
	$(MAKE) -C rmi4tool all
	$(MAKE) -C f54test all

clean:
	$(MAKE) -C rmidevice clean
	$(MAKE) -C rmi4update clean
	$(MAKE) -C rmi4tool clean
	$(MAKE) -C f54test clean

android:
	ndk-build NDK_APPLICATION_MK=Application.mk

android-clean:
	ndk-build NDK_APPLICATION_MK=Application.mk clean
