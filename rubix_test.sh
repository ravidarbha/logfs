i=0
while [ $i -lt 10 ]; 
do
   sudo touch  mount/ravi$i
   echo "created ravi"
   ls -la mount/
   sudo rm -rf mount/ravi$i
   echo "deleted ravi"
   ls -la mount/
   i=` expr $i + 1 `
done
