if [ "$#" -ne 2 ]; then
    echo "usage: $0 <path> <password>"
fi
./cli.o deactivate --path $1 --password $2