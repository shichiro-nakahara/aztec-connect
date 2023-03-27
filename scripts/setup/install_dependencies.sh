#!/bin/bash

dnf install -y ninja-build clang clang-tools-extra binaryen socat tmux sqlite sqlite-devel

cp .tmux.conf ~/.tmux.conf

curl https://raw.githubusercontent.com/creationix/nvm/master/install.sh | bash

curl -L https://foundry.paradigm.xyz | bash

. ~/.bashrc

nvm install v18.8.0

foundryup