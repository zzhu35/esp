#!/bin/sh

# Author Zeran Zhu
# Copyright University of Illinois 2020

# server side script

# upstream sets UP_STREAM_DIR, MAKE_TARGET

UPSTREAM="dholak.cs.illinois.edu"

WORK_DIR="/tmp/$USER/"

if [ -d $WORK_DIR/socs ]
then
    echo "Already mounted upstream workspace."
else
    echo "Creating remote HLS working directory..."
    mkdir -p $WORK_DIR
    echo "Mounting upstream working directory..."
    sshfs -o default_permissions $USER@$UPSTREAM:$UP_STREAM_DIR $WORK_DIR
fi


echo "Setting ESP root directory..."
export ESP_ROOT=$WORK_DIR
echo "Setting up downstream dependencies..."
module unload cadence
module load cadence/Feb2019
echo "Launching downstream MAKE command..."
cd $WORK_DIR/socs/xilinx-vc707-xc7vx485t
echo "make $MAKE_TARGET"
make $MAKE_TARGET

echo "Worker left workspace gracefully."
