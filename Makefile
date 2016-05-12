obj-m += rubix_fs.o 

rubix_fs-objs :=  rubix_test.o rubix_file_system.o rubix_dir_ops.o rubix_file_ops.o worker.o bitmaps.o btree.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
