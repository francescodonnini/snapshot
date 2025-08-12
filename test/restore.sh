for f in $(ls /snapshots/$1); do
    if [[ "$f" =~ ^[0-9]+$ ]]; then
        dd if="/snapshots/$1/$f" of="$2" bs=512 seek="$f" conv=notrunc status=none
    fi
done