#!/bin/bash
export PATH=$PATH:/opt/Catapult-10.0b/Mgc_home/bin
hostname=$(hostname)
hostname=$(echo "$hostname" | cut -d'.' -f 1)
if [ "$hostname" = "bindi" ]; then
    export LM_LICENSE_FILE=1717@license.irisa.fr
else
    export LM_LICENSE_FILE=1717@loth3.enssat.fr:1717@license.irisa.fr
fi
catapult -product university $@

