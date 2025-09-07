#! /bin/bash


sudo /Users/clayton/Developer/Get-Weather-Data/weatherstation -n > /Users/clayton/Developer/Get-Weather-Data/weatherstation.log
/opt/homebrew/bin/terminal-notifier -message "weatherstation crashed"
