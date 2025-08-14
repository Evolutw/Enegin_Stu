#!/bin/bash

# 获取当前月份和日期
COMMIT_MESSAGE=$(date +"%m-%d")

# 执行 Git 操作
git add .
git commit -m "$COMMIT_MESSAGE"
git push

# 提示完成
echo "Git 上传完成，Commit 信息: $COMMIT_MESSAGE"
