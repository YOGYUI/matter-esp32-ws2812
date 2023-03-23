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
esp_idf_path=${project_path}/sdk/esp-idf
esp_matter_path=${project_path}/sdk/esp-matter
chip_path=${esp_matter_path}/connectedhomeip/connectedhomeip

if [ -z "$IDF_PATH" ]; then
  source ${esp_idf_path}/export.sh
fi
source ${esp_matter_path}/export.sh
export IDF_CCACHE_ENABLE=1

# (optional) print git commit id of repositories
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

cd ${cur_path}