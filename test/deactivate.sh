if [ "$#" -ne 2 ]; then
    echo "usage: $0 <path> <password>"
fi
PARENT=$(dirname "$0")
if [ "$PARENT" == "." ]; then
    EXE="./cli/cli.o"
else
    EXE="./$PARENT/cli/cli.o"
fi
"$EXE" deactivate --path $1 --password $2