#!/bin/bash

PID=$1
OUTPUT=$2

if [ -z "$PID" ] || [ -z "$OUTPUT" ]; then
    echo "用法: $0 <PID> <输出文件>"
    exit 1
fi

if [ ! -r "/proc/$PID/status" ]; then
    echo "进程不存在: $PID"
    exit 1
fi

echo "time,VmSize_kB,VmRSS_kB,VmPeak_kB,VmHWM_kB" > "$OUTPUT"

while [ -r "/proc/$PID/status" ]; do
    TIME=$(date '+%H:%M:%S')
    VMSIZE=$(awk '/VmSize:/ {print $2}' "/proc/$PID/status")
    VMRSS=$(awk '/VmRSS:/ {print $2}' "/proc/$PID/status")
    VMPEAK=$(awk '/VmPeak:/ {print $2}' "/proc/$PID/status")
    VMHWM=$(awk '/VmHWM:/ {print $2}' "/proc/$PID/status")

    echo "$TIME,$VMSIZE,$VMRSS,$VMPEAK,$VMHWM" >> "$OUTPUT"
    sleep 1
done

echo
echo "===== 内存统计结果 ====="

awk -F, '
NR == 2 {
    start_vsize=$2
    start_rss=$3
}
NR > 1 {
    end_vsize=$2
    end_rss=$3

    if ($2 > max_vsize) max_vsize=$2
    if ($3 > max_rss) max_rss=$3
    if ($4 > vmpeak) vmpeak=$4
    if ($5 > vmhwm) vmhwm=$5
}
END {
    printf "开始虚拟内存: %.2f MB\n", start_vsize / 1024
    printf "开始物理内存: %.2f MB\n", start_rss / 1024
    printf "最大虚拟内存: %.2f MB\n", max_vsize / 1024
    printf "最大物理内存: %.2f MB\n", max_rss / 1024
    printf "VmPeak: %.2f MB\n", vmpeak / 1024
    printf "VmHWM: %.2f MB\n", vmhwm / 1024
    printf "结束虚拟内存: %.2f MB\n", end_vsize / 1024
    printf "结束物理内存: %.2f MB\n", end_rss / 1024
}' "$OUTPUT"