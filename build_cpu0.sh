#!/usr/bin/env bash

TUTORIAL_DIR=$(pwd)
LLVM_PROJECT_DIR=${TUTORIAL_DIR}/llvm-project
CHAPTER=$1
echo "-----select chapter: $1 -------"
if [ -z "$CHAPTER" ]; then
  echo "chapter is null"
  exit 1
fi

if [ ! -d ${LLVM_PROJECT_DIR} ]; then
  echo -e "git clone git@github.com:llvm/llvm-project.git --recursiv"
  git clone git@github.com:llvm/llvm-project.git --recursive
  cd ${LLVM_PROJECT_DIR}
  git checkout -b cpu0 origin/release/17.x
  cp ${TUTORIAL_DIR}/build_llvm.sh ./
fi
cd ${LLVM_PROJECT_DIR}
# 打上llvm公共部分修改的补丁
git apply ${TUTORIAL_DIR}/llvm17.diff
# cp相应的章节
cp -rf ${TUTORIAL_DIR}/chapters/${CHAPTER} ${LLVM_PROJECT_DIR}/llvm/lib/Target/Cpu0
#cp -rf regression-test/Cpu0 ${LLVM_TEST_DIR}/llvm/test/CodeGen/.
OS=`uname -s`
echo "OS =" ${OS}
bash build_llvm.sh




