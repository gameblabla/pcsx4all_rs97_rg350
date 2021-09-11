#!/bin/sh

OPK_NAME=pcsx4all_"$1".opk

echo ${OPK_NAME}

# create default.gcw0.desktop
cat > default."$1".desktop <<EOF
[Desktop Entry]
Name=Pcsx4all
Comment=PS1 emulator (Gameblabla version)
Exec=pcsx4all -iso %f
Terminal=false
Type=Application
StartupNotify=true
Icon=pcsxr-icon-small
Categories=emulators;
X-OD-NeedsDownscaling=true
EOF

# create default.gcw0.desktop
cat > normal."$1".desktop <<EOF
[Desktop Entry]
Name=Pcsx4all menu
Comment=PS1 menu
Exec=pcsx4all
Terminal=false
Type=Application
StartupNotify=true
Icon=pcsxr-icon-small
Categories=emulators;
X-OD-NeedsDownscaling=true
EOF

# create opk
FLIST="pcsx4all"
FLIST="${FLIST} default.$1.desktop"
FLIST="${FLIST} normal.$1.desktop"
FLIST="${FLIST} src/port/sdl/pcsxr-icon-small.png"

rm -f ${OPK_NAME}
mksquashfs ${FLIST} ${OPK_NAME} -all-root -no-xattrs -noappend -no-exports

cat default."$1".desktop
cat normal."$1".desktop
rm -f default."$1".desktop
rm -f normal."$1".desktop
