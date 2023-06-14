cp hello kshram.ko /home/alison/Lab5/dist/dist/rootfs
cd /home/alison/Lab5/dist/dist/rootfs
find . | cpio -H newc -o | bzip2 > ../newrootfs.cpio.bz2
cd /home/alison/Lab5/dist
./qemu.sh