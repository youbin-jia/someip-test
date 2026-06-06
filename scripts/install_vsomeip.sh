#!/usr/bin/env bash
# install_vsomeip.sh — 一键编译并安装 vsomeip3 到 /usr/local
#
# 用法:
#   sudo bash scripts/install_vsomeip.sh           # 装最新 release tag
#   sudo VSOMEIP_REF=3.5.5 bash scripts/install_vsomeip.sh   # 装指定 tag
#
# 完成后:
#   /usr/local/include/vsomeip/...   <- 头文件
#   /usr/local/lib/libvsomeip3*.so   <- 运行库

set -euo pipefail

VSOMEIP_REF="${VSOMEIP_REF:-3.5.5}"           # 已发布的稳定 tag, 你也可以换成 master
BUILD_DIR="${BUILD_DIR:-/tmp/vsomeip-build}"

echo "==> 检查依赖 (apt)..."
APT_PKGS=(
    build-essential
    cmake
    git
    libboost-system-dev
    libboost-thread-dev
    libboost-log-dev
    libboost-filesystem-dev
    asciidoc
    source-highlight
    doxygen
    graphviz
)
if command -v apt-get >/dev/null 2>&1; then
    apt-get update
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "${APT_PKGS[@]}"
else
    echo "!! 非 Debian/Ubuntu 系统,请自行确认上述包已安装"
fi

echo "==> 克隆 vsomeip @ ${VSOMEIP_REF} 到 ${BUILD_DIR}"
rm -rf "${BUILD_DIR}"
git clone --depth 1 --branch "${VSOMEIP_REF}" https://github.com/COVESA/vsomeip.git "${BUILD_DIR}"

echo "==> CMake 配置 + 编译 + 安装"
mkdir -p "${BUILD_DIR}/build"
cd "${BUILD_DIR}/build"
cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DENABLE_SIGNAL_HANDLING=1 \
    ..
make -j"$(nproc)"
make install

echo "==> 刷新动态链接器缓存"
ldconfig

echo
echo "✅  vsomeip3 已安装。验证一下:"
ls -1 /usr/local/lib/libvsomeip3*.so 2>/dev/null || true
echo
echo "下一步: 在项目根目录运行  bazel build //examples/..."
