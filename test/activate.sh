if [ "$#" -ne 2 ]; then
    echo "usage: $0 <path> <password>"
    exit -1
fi
PARENT=$(dirname "$0")
if [ "$PARENT" == "." ]; then
    EXE="./cli/cli.o"
else
    EXE="./$PARENT/cli/cli.o"
fi
"$EXE" activate --path $1 --password $2