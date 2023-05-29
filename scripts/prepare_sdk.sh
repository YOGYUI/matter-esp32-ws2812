#! /usr/sh
# prepare_sdk.sh
# author: seung hee, lee
# purpose: prepare sdk environment (idf.py)

cur_path=${PWD}
if [[ "$OSTYPE" == "darwin"* ]]; then
    project_path=$(dirname $(dirname $(realpath $0)))
else 
    project_path=$(dirname $(dirname $(realpath $BASH_SOURCE)))
fi
# sdk_path=${project_path}/sdk
sdk_path=~/tools  # change to your own sdk path
esp_idf_path=${sdk_path}/esp-idf
esp_matter_path=${sdk_path}/esp-matter
chip_path=${esp_matter_path}/connectedhomeip/connectedhomeip
zap_path=${sdk_path}/esp-matter/.zap

source ${esp_idf_path}/export.sh
source ${esp_matter_path}/export.sh
export IDF_CCACHE_ENABLE=1

# (optional) print git commit id of repositories
echo "------------------------------------------------------"
echo "[esp-idf]"
cd ${esp_idf_path}
git rev-parse HEAD
git describe --tags
echo "[esp-matter]"
cd ${esp_matter_path}
git rev-parse HEAD
git describe --tags
echo "[connectedhomeip]"
cd ${chip_path}
git rev-parse HEAD
git describe --tags
echo "[zap-cli]"
cd ${zap_path}
./zap-cli --version
echo "------------------------------------------------------"

cd ${cur_path}