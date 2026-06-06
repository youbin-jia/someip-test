#!/usr/bin/env bash
# run.sh — 一键启动某个示例的某一端
#
# 用法:
#   bash scripts/run.sh <例子编号> <角色>
#
#   编号:  01 | 02 | 03 | 04
#   角色:  service | client | client1 | client2  (取决于具体示例)
#
# 示例:
#   bash scripts/run.sh 01 service
#   bash scripts/run.sh 04 client2

set -euo pipefail

if [[ $# -lt 2 ]]; then
    echo "用法: $0 <01|02|03|04> <service|client|client1|client2>"
    exit 1
fi

NUM="$1"
ROLE="$2"

# 找到示例目录 (例如 01 -> 01_request_response)
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
EX_DIR=$(ls -d "${ROOT}/examples/${NUM}"_* 2>/dev/null | head -n1 || true)
CFG_DIR=$(ls -d "${ROOT}/config/${NUM}"_* 2>/dev/null | head -n1 || true)

if [[ -z "${EX_DIR}" || -z "${CFG_DIR}" ]]; then
    echo "找不到示例 ${NUM} (examples/${NUM}_* 或 config/${NUM}_*)"; exit 1
fi

CFG_FILE="${CFG_DIR}/${ROLE}.json"
if [[ ! -f "${CFG_FILE}" ]]; then
    echo "找不到配置 ${CFG_FILE}"; exit 1
fi

# bazel-bin 路径: examples/01_request_response/<role>
BAZEL_BIN="${ROOT}/bazel-bin/examples/$(basename "${EX_DIR}")/${ROLE}"
if [[ ! -x "${BAZEL_BIN}" ]]; then
    echo "未找到可执行文件 ${BAZEL_BIN}"
    echo "先运行: bazel build //examples/$(basename "${EX_DIR}"):${ROLE}"
    exit 1
fi

# vsomeip 通过两个环境变量找配置 / 找自己叫什么名字
export VSOMEIP_CONFIGURATION="${CFG_FILE}"
export VSOMEIP_APPLICATION_NAME="${ROLE}"

echo "==> VSOMEIP_CONFIGURATION=${VSOMEIP_CONFIGURATION}"
echo "==> VSOMEIP_APPLICATION_NAME=${VSOMEIP_APPLICATION_NAME}"
echo "==> 启动 ${BAZEL_BIN}"
echo "----"
exec "${BAZEL_BIN}"
