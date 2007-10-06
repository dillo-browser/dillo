#!/bin/sh
#
# Script to generate configure&make stuff
#

#-----------------------------------------------------
# If defined, get these programs from the environment
#
: ${ACLOCAL:=aclocal}
: ${AUTOHEADER:=autoheader}
: ${AUTOCONF:=autoconf}
: ${AUTOMAKE:=automake}

#-------------------------
# Required binaries check
#          
check_bin_file(){
   which $1 > /dev/null 2>&1
   if [ $? = 0 ]; then
      return 0
   else
      return 1
   fi
}

#------
# Main
#

#clear
ERR="no"
for cmd in "$ACLOCAL" "$AUTOHEADER" "$AUTOCONF" "$AUTOMAKE"
do
   if check_bin_file "$cmd"
   then
      echo -e "$cmd   \tfound"
   else
      echo -e "$cmd   \tNOT found"
      ERR="yes"
   fi
done

if test $ERR = "yes"
then
   echo
   echo "ERROR: to run this program you need the following installed"
   echo "       $ACLOCAL $AUTOHEADER $AUTOCONF $AUTOMAKE"
   echo
   exit 1
fi

echo "[Checks passed]"
echo "Generating..."

"$ACLOCAL"
"$AUTOHEADER"
"$AUTOCONF"
"$AUTOMAKE" -a

