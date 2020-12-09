sudo sh -c "echo 0  > /proc/sys/kernel/kptr_restrict"
script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
alias apib="$script_dir""/../garbage/apib/bin/apib"
#sudo sh -c "echo /proc/sys/kernel/perf_event_max_sample_rate"
