#!/bin/bash
# We have to do that in case we have installed Node js modules without sudo
# See https://github.com/sindresorhus/guides/blob/master/npm-global-without-sudo.md

if [ -f ~/.bash_profile ]; then
  source ~/.bash_profile
elif [ -f ~/.profile ]; then
  source ~/.profile
fi

appdmg $1 $2