#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
GNU_COMMANDS="[ base64 basename cat chcon chgrp chmod chown chroot \
cksum comm cp csplit cut date dd df dir dircolors dirname du echo env expand \
expr factor false fmt fold groups head hostid id install join kill link ln \
logname ls md5sum mkdir mkfifo mknod mktemp mv nice nl nohup nproc numfmt od \
paste pathchk pinky pr printenv printf ptx pwd readlink realpath rm rmdir \
runcon seq sha1sum sha224sum sha256sum sha384sum sha512sum shred shuf sleep \
sort split stat stty sum sync tac tail tee test timeout touch tr true \
truncate tsort tty uname unexpand uniq unlink uptime users vdir wc who \
whoami yes sed"

pushd "$DIR" > /dev/null
    if [ ! -d local ]; then
        mkdir local
    fi
    pushd local > /dev/null
        if [ ! -d bin ]; then
            mkdir bin
        fi
        pushd bin > /dev/null
            for COMMAND in $GNU_COMMANDS; do
                ln -fs $(which $(echo g${COMMAND})) $COMMAND
            done
        popd > /dev/null
    popd > /dev/null
popd > /dev/null

export PATH=$DIR/local/bin:$PATH
