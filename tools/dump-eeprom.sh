IN_F=$1
OUT_F=${2:-$IN_F-eeprom.bin} 
echo "$OUT_F" 
SECTION='P6-P8 ro'

#  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
# ...
#  [ 7] P6-P8 ro          PROGBITS        00004000 000034 000008 01   A  0   0  1
readelf -S "$IN_F" |
	grep "$SECTION" |
	awk -F 'PROGBITS' '{print $2}' |
	awk '{print "dd if='$IN_F' of='$OUT_F' bs=1 skip=$[0x" $2 "] count=$[0x" $3 "]"}' |
	bash
