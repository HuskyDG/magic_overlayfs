
loop_setup() {
  unset LOOPDEV
  local LOOP
  local MINORX=1
  [ -e /dev/block/loop1 ] && MINORX=$(stat -Lc '%T' /dev/block/loop1)
  local NUM=0
  while [ $NUM -lt 1024 ]; do
    LOOP=/dev/block/loop$NUM
    [ -e $LOOP ] || mknod $LOOP b 7 $((NUM * MINORX))
    if losetup $LOOP "$1" 2>/dev/null; then
      LOOPDEV=$LOOP
      break
    fi
    NUM=$((NUM + 1))
  done
}

sizeof(){
    EXTRA="$2"
    [ -z "$EXTRA" ] && EXTRA=0
    [ "$EXTRA" -gt 0 ] || EXTRA=0
    size="$(du -s "$1" | awk '{ print $1 }')"
    # append more 20Mb
    size="$((size + EXTRA))"
    echo -n "$((size + 20000))"
}


handle() {
  if [ ! -L "/$1" ] && [ -d "/$1" ] && [ -d "$MODPATH/system/$1" ]; then
    mv -f "$MODPATH/overlay/system/$1" "$MODPATH/overlay"
  fi
}

support_overlayfs() {

#OVERLAY_IMAGE_EXTRA - number of kb need to be added to overlay.img
#OVERLAY_IMAGE_SHRINK - shrink overlay.img or not?

if [ -d "$MODPATH/system" ]; then
  OVERLAY_IMAGE_SIZE="$(sizeof "$MODPATH/system" "$OVERLAY_IMAGE_EXTRA")"
  dd if=/dev/zero of="$MODPATH/overlay.img" bs=1024 count="$OVERLAY_IMAGE_SIZE"
  ui_print "- Created overlay image with size: $(du -shH "$MODPATH/overlay.img" | awk '{ print $1 }')"
  /system/bin/mkfs.ext4 "$MODPATH/overlay.img"
  loop_setup "$MODPATH/overlay.img"
  if [ ! -z "$LOOPDEV" ]; then
    rm -rf "$MODPATH/overlay"
    mkdir "$MODPATH/overlay"
    mount -t ext4 -o rw "$LOOPDEV" "$MODPATH/overlay"
    chcon u:object_r:system_file:s0 "$MODPATH/overlay"
    cp -afT "$MODPATH/system" "$MODPATH/overlay/system"
    # fix context
    ( cd "$MODPATH" || exit 
      find "system" | while read line; do
        chcon "$(ls -Zd "$line" | awk '{ print $1 }')" "$MODPATH/overlay/$line"
      done
    )
    
    # handle partition
    handle vendor
    handle product
    handle system_ext
    umount -l "$MODPATH/overlay"

    if [ "$OVERLAY_IMAGE_SHRINK" == "true" ] || [ -z "$OVERLAY_IMAGE_SHRINK" ]; then
      ui_print "- Shrink overlay image"
      e2fsck -pf "$MODPATH/overlay.img"
      resize2fs -M "$MODPATH/overlay.img"
      ui_print "- Overlay image new size: $(du -shH "$MODPATH/overlay.img" | awk '{ print $1 }')"
    fi
    rm -rf "$MODPATH/overlay"
  fi
fi

}