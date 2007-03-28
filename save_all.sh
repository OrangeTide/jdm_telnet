#!/bin/sh
D=`basename \`pwd\``
T=`date +%Y%m%d`
if [ -r VERSION ]; then
	T=$(cat VERSION)-${T}
fi
cd ../
# pax -Lvwz -f ${D}-${T}.tar.gz -x ustar -s '/.*CVS.*//' ${D}/
rm "${D}-${T}.zip" 2>/dev/null
zip -Xoyr "${D}-${T}.zip" "${D}/" -x '*/CVS/*' -x '*.o' -x '.*swp' -x '*~' -x '*/.svn/*' -x '*/.*' -x '*/.*/*'
