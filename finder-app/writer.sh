#!/bin/sh
# File writer script for assignment 1
# Author: Cedric Camacho


# check number of arguments
if [ $# -lt 2 ]

then  

  echo "\nERROR: Incorrect number of arguments\n"\
       "\rMust have 2 arguments:\n"\
       "\rArgument 1: full path to a file (including filename) on the filesystem\n"\
       "\rArgument 2: text string which will be written within the file\n"
  exit 1

fi

writefile=$1
writestr=$2

# get directory and create if needed
directory=$( dirname $writefile )
mkdir -p $directory # -p creates directory or does nothing if existing

# create or overwrite file 
echo $writestr > $writefile

if [ -e $writefile ]

then
  
  #echo "\n$writefile successfully written\n"
  exit 0

else

  echo "\n$writefile could not be created\n"
  exit 1
  
fi
  