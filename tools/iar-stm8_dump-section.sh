IN_F=$1
TYPE=$2
SECTION=$3

#  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
# ...
#  [ 7] P6-P8 ro          PROGBITS        00004000 000034 000008 01   A  0   0  1
readelf -S "$IN_F" |
	grep "$SECTION" |
	awk -F "$TYPE" '{print $2}' |
	awk '{print "dd if='$IN_F' bs=1 skip=$[0x" $2 "] count=$[0x" $3 "]"}' |
	bash
