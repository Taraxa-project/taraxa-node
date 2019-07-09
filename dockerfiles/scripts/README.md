# SCRIPTS

## Env Setup

Creates `env_variables.sh` for use in other scripts.

```bash
$ cd dockerfiles/scripts
$ ./env-setup.sh
Creating env_variables.sh
```

## Update Base Image

```bash
$ cd dockerfiles/scripts
$ AWS_PROFILE=<profile name> \
  ./update_base_image.sh
```