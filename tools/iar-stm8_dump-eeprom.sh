IN_F=$1
OUT_F=${2:-$IN_F-eeprom.bin} 
echo "$OUT_F" 
selfDir=$(dirname "$0")
("$selfDir/iar-stm8_dump-section.sh" "$IN_F" 'PROGBITS' 'P6-P8 ro') > "$OUT_F"
