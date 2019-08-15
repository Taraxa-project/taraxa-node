from pathlib import Path
from tempfile import gettempdir

_this_dir = Path(__file__).parent

base_dir = _this_dir.parent.parent.parent.parent
core_tests_dir = base_dir.joinpath('core_tests')
core_tests_conf_dir = core_tests_dir.joinpath('conf')
bin_dir = base_dir.joinpath('build')
node_exe = bin_dir.joinpath('main')
bin_dir_debug = base_dir.joinpath('build-d')
tmpdir = Path(gettempdir()).joinpath('taraxa')
