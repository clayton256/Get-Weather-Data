#! /bin/bash

source $HOME/.prowlrc

sudo $HOME/Developer/Get-Weather-Data/weatherstation -n > $HOME/Developer/Get-Weather-Data/weatherstation.log
#/opt/homebrew/bin/terminal-notifier -message "weatherstation crashed"
$HOME/bin/cprowl -a $PROWLKEY -n weatherstation -e ended -d "weatherstation terminated"

