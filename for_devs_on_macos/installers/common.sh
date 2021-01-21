basedir=$(realpath $(dirname "$0")/../../local)
cpu_cnt=$(${basedir}/../scripts/cpu_count.sh)
dst=${basedir}/system_home_override

mkdir -p ${basedir}/downloads
cd ${basedir}/downloads
