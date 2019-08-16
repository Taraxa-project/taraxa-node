### Purpose
This is a stub for taraxa utils python lib.
Please put here all generic utils that may be useful for users or 
reusable across our python projects e.g. integration tests.

### Local dev environment
*According to the best practices, use `virtualenv` or a similar tool 
to create an isolated local python environment*

Install:
```
pip install .
```

### Installing as a dependency
use the following `pip` dependency descriptor:
```
git+https://github.com/Taraxa-project/taraxa-node@{GIT_REVISION}#subdirectory=sdk_py
```
Example:
```
pip install git+https://github.com/Taraxa-project/taraxa-node@{GIT_REVISION}#subdirectory=sdk_py
```
The root python module installed is `taraxa`