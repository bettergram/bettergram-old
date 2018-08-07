#!/bin/bash
# We have to do that in case we have installed Node js modules without sudo
# See https://github.com/sindresorhus/guides/blob/master/npm-global-without-sudo.md
[ -f ~/.bash_profile ] && source ~/.bash_profile && appdmg $1 $2