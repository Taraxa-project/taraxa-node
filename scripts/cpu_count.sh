python -c "
import multiprocessing
print(multiprocessing.cpu_count())
" 2>/dev/null ||
  getconf _NPROCESSORS_ONLN 2>/dev/null ||
  getconf NPROCESSORS_ONLN 2>/dev/null ||
  echo 1
