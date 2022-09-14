# YADRO Filesystem synchronization daemon

This daemon provides functional to synchronize RWFS from the main flash drive of
BMC onto alternate one (aka golden flash).

It uses `inotify` to detect the filesystem changes filtered by whitelist and
calls `rsync` in several seconds after the changes detected.

## Build
```
meson build
meson compile -Cbuild
```

