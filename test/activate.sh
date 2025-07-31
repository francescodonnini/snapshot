if [ "$#" -ne 2 ]; then
    echo "usage: $0 <path> <password>"
    exit -1
fi
PARENT=$(dirname "$0")
./$PARENT/cli.o activate --path $1 --password $2