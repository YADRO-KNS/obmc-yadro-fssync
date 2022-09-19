# YADRO Filesystem synchronization daemon

This daemon provides synchronization of RWFS from the BMC's main flash drive
onto the alternate one (aka golden flash).

It uses `inotify` to detect the filesystem changes filtered by whitelist and
calls `rsync` a few minutes after the changes are detected.

## Build
```
meson build
meson compile -Cbuild
```

