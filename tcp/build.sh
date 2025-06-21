#!/bin/bash

if [ $# -lt 2 ]; then
    echo "Usage: $0 build|run|buildRun server|client"
    exit 1
fi

if [ "$1" = "build" ]; then
    make
elif [ "$1" = "buildRun" ] || [ "$1" = "run" ]; then
    if [ "$1" = "buildRun" ]; then
        make
    fi

    if [ "$2" = "client" ]; then
        if [ $# -ne 5 ]; then
            echo "Usage: $0 buildRun client 127.0.0.1 8888 1"
            exit 1
        fi
        for ((i=1; i<=$5; i++))
        do
            nohup ./bin/appClient "$3" "$4" &
        done
    else
        nohup ./bin/appServer &
    fi
else
    echo "Invalid option: $1"
    exit 1
fi