#!/bin/bash

## Wrap Macports command (any executables installed by Macports).

    if [[ -z $MACPORTS_PREFIX ]]; then
	      MACPORTS_PREFIX='/opt/local'
      fi

      export PATH="$MACPORTS_PREFIX/bin:$MACPORTS_PREFIX/sbin:$PATH"
      export CPATH="$MACPORTS_PREFIX/include:$CPATH"

      command=$1

      shift

      exec port $command $*
