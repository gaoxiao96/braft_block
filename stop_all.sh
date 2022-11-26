#!/bin/bash

## 清理可用

rm -rf ./runtime/*
ps -ef | grep "block_" | awk -F ' ' '{print $2}' | xargs kill