IN_F=$1
OUT_F=${2:-$IN_F-flash.bin} 
echo "$OUT_F" 
selfDir=$(dirname "$0")
("$selfDir/iar-stm8_dump-section.sh" "$IN_F" 'PROGBITS' 'A2 rw'; "$selfDir/iar-stm8_dump-section.sh" "$IN_F" 'PROGBITS' 'P3-P5 ro') > "$OUT_F"
