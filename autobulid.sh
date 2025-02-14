#!/bin/bash

set -e

#如果没有build目录，则创建
if [ ! -d `pwd`/build ];then
    mkdir `pwd`/build
fi

#删除build目录下的所有文件
rm -rf `pwd`/build/*

#进入build目录，执行cmake和make
cd `pwd`/build &&
    cmake .. &&
    make

cd ..

#把头文件拷贝到/usr/include/mymuduo so库拷贝到/usr/lib

if [ ! -d /usr/include/mymuduo ]; then
    sudo mkdir /usr/include/mymuduo
fi

for header in `ls ./include/*.hh`
do
    cp $header /usr/include/mymuduo
done

cp `pwd`/lib/libmymuduo.so /usr/lib

ldconfig