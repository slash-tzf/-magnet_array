#!/usr/bin/env bash
# download.sh — 用 OpenOCD + DAPLink(CMSIS-DAP) 烧录 STM32G031
# 用法:
#   ./download.sh build/Debug/test.elf
#   ./download.sh build/Debug/test.bin 0x08000000
# 诊断模式:
#   ./download.sh --check        # 自检环境
#   ./download.sh --verbose ...  # 更详细日志

set -euo pipefail

# ---- 可调参数 ----
OPENOCD_CFG="${OPENOCD_CFG:-openocd_g031.cfg}"   # 默认使用工程内的 cfg
ADAPTER_SPEED="${ADAPTER_SPEED:-1000}"           # kHz - STM32G0系列建议较低速度
DEFAULT_BIN_ADDR="${DEFAULT_BIN_ADDR:-0x08000000}"
BUILD_PRESET="${BUILD_PRESET:-${CMAKE_BUILD_PRESET:-}}"  # CMake build 预设名，空则自动推断/默认 Debug
# ------------------

log()  { echo -e "[INFO] $*"; }
warn() { echo -e "[WARN] $*" >&2; }
err()  { echo -e "[ERR ] $*"  >&2; exit 1; }

is_cmd() { command -v "$1" >/dev/null 2>&1; }

usage() {
  cat <<EOF
用法:
  $0 <firmware.elf|.hex|.bin> [bin起始地址]
  $0 --check
  $0 --verbose <固件...>

示例:
  $0 build/Debug/test.elf
  $0 build/Debug/test.bin 0x08000000
环境变量:
  OPENOCD_CFG=openocd_g031.cfg   # 指定 cfg
  ADAPTER_SPEED=1000             # 降速
  DEFAULT_BIN_ADDR=0x08000000    # bin 默认起始
  BUILD_PRESET=Release           # 下载前自动: cmake --build --preset <该值> (默认自动推断/Debug)
EOF
}

VERBOSE=0
if [[ $# -eq 0 ]]; then usage; exit 1; fi
if [[ "${1:-}" == "--check" ]]; then
  echo "===== 自检 ====="
  echo "- 当前目录: $(pwd)"
  echo "- Shell: $SHELL"
  echo "- 文件: "; ls -lah
  echo "- openocd: $(is_cmd openocd && openocd -v | head -n1 || echo '未找到')"
  echo "- gdb: $(is_cmd arm-none-eabi-gdb && arm-none-eabi-gdb --version | head -n1 || is_cmd gdb-multiarch && gdb-multiarch --version | head -n1 || echo '未找到')"
  echo "- cmake: $(is_cmd cmake && cmake --version | head -n1 || echo '未找到')"
  echo "- cfg 存在: ${OPENOCD_CFG} $( [[ -f "$OPENOCD_CFG" ]] && echo '[OK]' || echo '[不存在]')"
  echo "- DAPLink 检测: $(lsusb | grep -i '0d28:0204' && echo '[OK]' || echo '[未检测到]')"
  echo "==============="
  exit 0
fi
if [[ "${1:-}" == "--verbose" ]]; then VERBOSE=1; shift; [[ $# -ge 1 ]] || { usage; exit 1; }; fi

FW="$1"; shift || true
FW_EXT="${FW##*.}"

# 纠正常见路径误用：/build/*** 应改为 ./build/***
if [[ "$FW" == /build/* ]]; then
  warn "你传入了绝对路径 $FW，看起来像是误写。你工程里的文件在 ./build/ 下。"
  ALT="./${FW#/}"    # 去掉前导 / 得到相对路径
  if [[ -f "$ALT" ]]; then
    warn "自动改用 $ALT"
    FW="$ALT"
  fi
fi

# 在下载前先尝试编译（使用 CMake Presets）
if is_cmd cmake; then
  # 推断/选择 build preset
  PRESET="$BUILD_PRESET"
  if [[ -z "${PRESET}" ]]; then
    # 去掉可能的前导 ./ 后匹配 build/<preset>/...
    FW_NOPFX="${FW#./}"
    if [[ "$FW_NOPFX" =~ ^build/([^/]+)/ ]]; then
      PRESET="${BASH_REMATCH[1]}"
    else
      PRESET="Debug"
    fi
  fi
  log "开始编译 (cmake preset: ${PRESET})"
  # 详细模式传递给 cmake
  CMAKE_BUILD_ARGS=()
  if [[ $VERBOSE -eq 1 ]]; then CMAKE_BUILD_ARGS+=(--verbose); fi
  # 使用 build preset 会在未配置时自动配置，并于相应 build 目录中使用 Ninja 构建
  cmake --build --preset "${PRESET}" --parallel "${CMAKE_BUILD_ARGS[@]}" || err "编译失败"
else
  warn "未找到 cmake，跳过自动编译"
fi

# 检查固件文件（若编译后路径发生变化，请传入 build/<Preset>/xxx）
[[ -f "$FW" ]] || err "找不到固件文件: $FW"

# 检查 openocd
is_cmd openocd || err "找不到 openocd，请先安装：sudo apt install openocd"

# 组装 OpenOCD 参数
if [[ -f "$OPENOCD_CFG" ]]; then
  OCMD=(-f "$OPENOCD_CFG")
  log "使用工程配置: $OPENOCD_CFG"
else
  warn "未找到 $OPENOCD_CFG，改用默认 cmsis-dap + stm32g0x"
  OCMD=(-f interface/cmsis-dap.cfg -f target/stm32g0x.cfg -c "adapter speed ${ADAPTER_SPEED}")
fi

# 生成 program 子命令
if [[ "$FW_EXT" == "bin" ]]; then
  ADDR="${1:-$DEFAULT_BIN_ADDR}"
  [[ "$ADDR" =~ ^0x[0-9a-fA-F]+$ ]] || err "BIN 需要起始地址，例如 0x08000000"
  PROGRAM_CMD="program $FW $ADDR verify; reset run"
else
  PROGRAM_CMD="program $FW verify; reset run"
fi

# 输出环境信息
log "固件: $FW"
[[ "$FW_EXT" == "bin" ]] && log "起始地址: ${ADDR}"
log "适配器速度: ${ADAPTER_SPEED} kHz"
if [[ $VERBOSE -eq 1 ]]; then
  set -x
fi

# 执行
openocd "${OCMD[@]}" -c "init; reset halt; $PROGRAM_CMD; exit"