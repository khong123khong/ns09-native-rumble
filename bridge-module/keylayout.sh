#!/system/bin/sh
# Only a version-specific028e overlay is added when existing Xbox layouts are
# absent/incompatible. Generic.kl and the physical02e0 layout are untouched.
ns09_layout_valid() {
  [ -f "$1" ] || return 1
  awk '
  function num(s, i,n,d) { if (s ~ /^0[xX]/) { n=0; for(i=3;i<=length(s);i++){d=index("0123456789abcdef",tolower(substr(s,i,1)))-1;if(d<0)return -1;n=n*16+d} return n } return s+0 }
  BEGIN {
    k[304]="BUTTON_A";k[305]="BUTTON_B";k[307]="BUTTON_X";k[308]="BUTTON_Y";k[310]="BUTTON_L1";k[311]="BUTTON_R1";
    k[314]="BUTTON_SELECT";k[315]="BUTTON_START";k[316]="BUTTON_MODE";k[317]="BUTTON_THUMBL";k[318]="BUTTON_THUMBR";
    a[0]="X";a[1]="Y";a[3]="Z";a[4]="RZ";a[2]="LTRIGGER";a[5]="RTRIGGER";a[16]="HAT_X";a[17]="HAT_Y";
  }
  $1=="key" { gotk[num($2)]=$3 }
  $1=="axis" { gota[num($2)]=$3 }
  END { for(i in k)if(gotk[i]!=k[i])exit 1;for(i in a)if(gota[i]!=a[i])exit 1;exit 0 }
  ' "$1"
}
ns09_select_keylayout() {
  ROOTS=${NS09_KL_ROOTS:-'/odm/usr/keylayout /vendor/usr/keylayout /product/usr/keylayout /system_ext/usr/keylayout /system/usr/keylayout /data/system/devices/keylayout'}
  FOUND=0
  BAD=0
  printf '%s\n' 'Virtual045E:028E Version0001; physical045E:02E0 unchanged' > "$MODPATH/keylayout-selection.txt"
  for DIR in $ROOTS; do
    for FILE in "$DIR/Vendor_045e_Product_028e_Version_0001.kl" "$DIR/Vendor_045e_Product_028e.kl"; do
      [ -f "$FILE" ] || continue
      FOUND=1
      if ns09_layout_valid "$FILE"; then
        printf 'COMPATIBLE %s\n' "$FILE" >> "$MODPATH/keylayout-selection.txt"
      else
        BAD=1
        printf 'INCOMPATIBLE %s\n' "$FILE" >> "$MODPATH/keylayout-selection.txt"
      fi
    done
  done
  if [ "$FOUND" = 1 ] && [ "$BAD" = 0 ]; then
    printf '%s\n' 'Use existing ROM Xbox360 keylayout; no overlay' >> "$MODPATH/keylayout-selection.txt"
    return 0
  fi
  ns09_layout_valid "$MODPATH/assets/Xbox360.kl" || return 1
  mkdir -p "$MODPATH/system/usr/keylayout" || return 1
  cp "$MODPATH/assets/Xbox360.kl" "$MODPATH/system/usr/keylayout/Vendor_045e_Product_028e_Version_0001.kl" || return 1
  # An incompatible version-specific vendor file has higher priority than the
  # system file. Overlay that exact028e version file, never a Generic/02e0 file.
  for DIR in $ROOTS; do
    FILE="$DIR/Vendor_045e_Product_028e_Version_0001.kl"
    [ -f "$FILE" ] || continue
    ns09_layout_valid "$FILE" && continue
    case "$DIR" in
      /vendor/*|/odm/*|/product/*|/system_ext/*) DEST="$MODPATH/system$DIR" ;;
      /system/*) DEST="$MODPATH$DIR" ;;
      *) continue ;;
    esac
    mkdir -p "$DEST" || return 1
    cp "$MODPATH/assets/Xbox360.kl" "$DEST/Vendor_045e_Product_028e_Version_0001.kl" || return 1
  done
  rm -f "$MODPATH/skip_mount"
  printf '%s\n' 'Version-specific systemless Xbox360 fallback enabled; root manager mount support required' >> "$MODPATH/keylayout-selection.txt"
}
