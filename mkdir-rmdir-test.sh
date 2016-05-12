########## This script verifies cleanup is working fine. The data block and the inode cleanup code is exercised. ######################
i=0
while [ $i -lt 10 ];
do
   sudo rm -rf mount/*
   sudo mkdir  mount/ravi
   echo "created ravi dir"
   sudo mkdir mount/adi
   sudo ls -la mount/
   sudo touch mount/ravi/ravi.txt
   sudo echo "This is ravi" >> mount/ravi/ravi.txt
   sudo cat mount/ravi/ravi.txt
   sudo rm -rf mount/ravi
   echo "deleted ravi"
   sudo ls -la mount/
   sudo touch mount/adi/adi.txt
   sudo echo "This is adi" >> mount/adi/adi.txt
   str1=`sudo cat mount/adi/adi.txt`
   sudo ls -la mount/
   sudo umount /dev/fioa
   sudo rmmod rubix_fs.ko
   sudo insmod ./rubix_fs.ko
   sudo mount -t rubix /dev/fioa mount
   echo "After mount"
   sudo ls -la  mount/adi
   str2=`sudo cat mount/adi/adi.txt`
   echo $str1
   echo $str2
   if [ "$str1" = "$str2" ];
 
  then
     echo "Success"
   else
     echo "Failure"
   fi
   i=` expr $i + 1 `
done
