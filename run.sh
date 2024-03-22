sshpass -p "passpass" scp -r * pi@192.168.1.110:~/zic/fbcp-ili9341/
sshpass -p "passpass" ssh pi@192.168.1.110 "cd ~/zic/fbcp-ili9341 && make -j && sudo ./fbcp-ili9341"
