export ARCH := riscv
export CROSS_COMPILE := ${RISCV}/buildroot/output/host/bin/riscv64-buildroot-linux-gnu-
obj-m += ssd_driver.o
KDIR := /opt/riscv/buildroot/output/build/linux-6.12.8
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
