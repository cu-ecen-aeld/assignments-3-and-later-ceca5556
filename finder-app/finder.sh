#!/bin/sh
# File find script for assignment 1
# Author: Cedric Camacho

set -e
set -u

# check number of arguments
if [ $# -lt 2 ]

then  

  echo "\nERROR: Incorrect number of arguments\n"\
       "\rMust have 2 arguments:\n"\
       "\rArgument 1: path to a directory on the filesystem\n"\
       "\rArgument 2: text string which will be searched within these files\n"
  exit 1

fi
  
# save arguments
filedir=$1
searchstr=$2
  
# check if directory exists
if [ -d $filedir ]
then
  
  num_files=$( find $filedir -type f | wc -l )
  num_match=$( grep -r $searchstr $filedir | wc -l ) 
  
  echo "\nThe number of files are $num_files and the number of matching lines are $num_match\n"
  
  exit 0
  
else
  
  echo "\nERROR: $filedir is not a directory"
  exit 1
    
fi