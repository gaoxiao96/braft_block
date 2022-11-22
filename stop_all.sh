#!/bin/bash

rm -rf ./runtime/*
ps -ef | grep "block_" | awk -F ' ' '{print $2}' | xargs kill