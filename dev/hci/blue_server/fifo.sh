#!/bin/bash

FIFO_NAME="bluetooth_fifo"

# 写入数据到管道
data=1234567890
echo "{\"topic\":\"response\",\"data\":\"$data\"}" > "$FIFO_NAME"