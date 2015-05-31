#!/bin/sh

png="$1"
hdr="$2"
sym=`basename "$png"|tr . _`
mac=`echo "$sym"|tr '[a-z]' '[A-Z]'`
size=`stat -c%s "$png"`

cat > "$hdr" << EOF
#ifndef ${mac}_H
#define ${mac}_H
#pragma once

#define ${mac}_LEN ${size}
extern const unsigned char ${sym}[];
extern const unsigned int ${sym}_len;

#endif
EOF
