#!/bin/sh
# Mountless shared-core comparison against the independent NTFS-3G tools.
# Both implementations must return identical names and bytes before any
# timing starts. Neither filesystem is ever mounted; every timed command
# starts a fresh process and volume object against a warm host page cache.
set -eu

if test "$#" -ne 1; then
    echo "usage: benchmark_core.sh NTFSLIB_FUSE_BINARY" >&2
    exit 2
fi
frontend=$1

samples=${NTFSLIB_BENCH_SAMPLES:-5}
dir_iterations=${NTFSLIB_BENCH_DIR_ITERATIONS:-100}
read_iterations=${NTFSLIB_BENCH_READ_ITERATIONS:-3}
small_file_count=${NTFSLIB_BENCH_SMALL_FILES:-250}
read_size_mib=${NTFSLIB_BENCH_READ_SIZE_MIB:-64}
bench_cpu=${NTFSLIB_BENCH_CPU:-0}

for tool in mkntfs ntfscp ntfsls ntfscat taskset; do
    command -v "$tool" >/dev/null 2>&1 ||
        { echo "missing required tool: $tool" >&2; exit 2; }
done
test "$samples" -ge 5 ||
    { echo "at least five samples are required" >&2; exit 2; }

workdir=$(mktemp -d "${TMPDIR:-/tmp}/ntfslib-core-bench.XXXXXX")
image="$workdir/bench.ntfs"
source_file="$workdir/sequential.bin"
small_source="$workdir/small.txt"
csv="$workdir/samples.csv"
succeeded=0

cleanup()
{
    if test "$succeeded" -eq 1; then
        rm -r -- "$workdir"
    else
        echo "preserved failing fixture: $workdir" >&2
    fi
}
trap cleanup EXIT HUP INT TERM

now_ns()
{
    date +%s%N
}

# One timed burst of identically launched fresh processes, pinned to one
# CPU. Prints elapsed seconds.
time_burst()
{
    _iterations=$1
    shift
    _start=$(now_ns)
    _index=0
    while test "$_index" -lt "$_iterations"; do
        taskset -c "$bench_cpu" "$@" >/dev/null
        _index=$((_index + 1))
    done
    _stop=$(now_ns)
    awk -v a="$_start" -v b="$_stop" \
        'BEGIN { printf "%.6f", (b - a) / 1e9 }'
}

median_of()
{
    _metric_tool=$1
    awk -F, -v key="$_metric_tool" '
        $3 "," $4 == key { values[count++] = $6 }
        END {
            if (count == 0)
                exit 1
            for (i = 0; i < count; i++)
                for (j = i + 1; j < count; j++)
                    if (values[j] + 0 < values[i] + 0) {
                        swap = values[i]
                        values[i] = values[j]
                        values[j] = swap
                    }
            if (count % 2)
                printf "%.2f", values[(count - 1) / 2]
            else
                printf "%.2f", (values[count / 2 - 1] + values[count / 2]) / 2
        }' "$csv"
}

echo "# ntfslib mountless core benchmark $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "# kernel: $(uname -sr)"
echo "# cpu: $(awk -F: '/^model name/ { gsub(/^ /, "", $2); print $2; exit }' \
    /proc/cpuinfo), pinned to CPU $bench_cpu"
echo "# ntfs-3g tools: $(ntfsls --version 2>&1 | head -n 1)"
echo "# fixture: 384 MiB NTFS image, $small_file_count small files," \
    "one $read_size_mib MiB ordinary stream"
echo "# directory sample: $dir_iterations fresh enumerations;" \
    "read sample: $read_iterations fresh whole-stream reads;" \
    "$samples samples, alternating tool order"

truncate -s 384M "$image"
mkntfs -F -Q -q -L NTFSLIB_CORE "$image"
dd if=/dev/urandom of="$source_file" bs=1M count="$read_size_mib" \
    status=none
printf 'ntfslib core benchmark payload\n' >"$small_source"
ntfscp -f -q "$image" "$source_file" /sequential.bin
file_index=1
while test "$file_index" -le "$small_file_count"; do
    ntfscp -f -q "$image" "$small_source" \
        "$(printf '/small-%06d.txt' "$file_index")"
    file_index=$((file_index + 1))
done
"$frontend" --probe "$image" | sed 's/^/# /'

# Correctness gate: identical visible names and identical stream bytes
# through both implementations, before any timing.
"$frontend" --list "$image" / | awk '{ print $NF }' | sort \
    >"$workdir/names.ntfslib"
ntfsls "$image" | sort >"$workdir/names.ntfs3g"
cmp "$workdir/names.ntfslib" "$workdir/names.ntfs3g"
name_count=$(wc -l <"$workdir/names.ntfslib")
test "$name_count" -eq $((small_file_count + 1))
"$frontend" --cat "$image" /sequential.bin | cmp - "$source_file"
ntfscat "$image" /sequential.bin | cmp - "$source_file"
echo "# validation: $name_count identical names, identical stream bytes"

# One untimed warm-up per tool and workload.
taskset -c "$bench_cpu" "$frontend" --list "$image" / >/dev/null
taskset -c "$bench_cpu" ntfsls "$image" >/dev/null
taskset -c "$bench_cpu" "$frontend" --cat "$image" /sequential.bin \
    >/dev/null
taskset -c "$bench_cpu" ntfscat "$image" /sequential.bin >/dev/null

echo "sample,order,tool,metric,seconds,rate,unit" | tee "$csv"
sample=1
while test "$sample" -le "$samples"; do
    if test $((sample % 2)) -eq 1; then
        first=ntfslib
        second=ntfs3g
    else
        first=ntfs3g
        second=ntfslib
    fi
    for tool in "$first" "$second"; do
        if test "$tool" = ntfslib; then
            dir_seconds=$(time_burst "$dir_iterations" \
                "$frontend" --list "$image" /)
            read_seconds=$(time_burst "$read_iterations" \
                "$frontend" --cat "$image" /sequential.bin)
        else
            dir_seconds=$(time_burst "$dir_iterations" \
                ntfsls "$image")
            read_seconds=$(time_burst "$read_iterations" \
                ntfscat "$image" /sequential.bin)
        fi
        dir_rate=$(awk -v i="$dir_iterations" -v s="$dir_seconds" \
            'BEGIN { printf "%.2f", i / s }')
        read_rate=$(awk -v i="$read_iterations" -v m="$read_size_mib" \
            -v s="$read_seconds" 'BEGIN { printf "%.2f", i * m / s }')
        printf '%s,%s,%s,%s,%s,%s,%s\n' \
            "$sample" "$first-first" "$tool" directory \
            "$dir_seconds" "$dir_rate" ops/s | tee -a "$csv"
        printf '%s,%s,%s,%s,%s,%s,%s\n' \
            "$sample" "$first-first" "$tool" read \
            "$read_seconds" "$read_rate" MiB/s | tee -a "$csv"
    done
    sample=$((sample + 1))
done

dir_ntfslib=$(median_of "ntfslib,directory")
dir_ntfs3g=$(median_of "ntfs3g,directory")
read_ntfslib=$(median_of "ntfslib,read")
read_ntfs3g=$(median_of "ntfs3g,read")
echo "median fresh open + directory enumeration:" \
    "ntfslib $dir_ntfslib ops/s, ntfsls $dir_ntfs3g ops/s," \
    "ratio $(awk -v a="$dir_ntfslib" -v b="$dir_ntfs3g" \
        'BEGIN { printf "%.2f", a / b }')x"
echo "median fresh open + cached sequential read:" \
    "ntfslib $read_ntfslib MiB/s, ntfscat $read_ntfs3g MiB/s," \
    "ratio $(awk -v a="$read_ntfslib" -v b="$read_ntfs3g" \
        'BEGIN { printf "%.2f", a / b }')x"

succeeded=1
