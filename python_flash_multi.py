#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
多端口烧录脚本 - 支持同时向多个串口设备烧录同一个固件

用法:
  python python_flash_multi.py bin/3.0.39_R01C_EN.bin -p COM3 COM10 COM19

说明:
- 使用 -p / --ports 指定一个或多个串口端口
- 多个端口之间用空格分隔，脚本会并行烧录
"""

import subprocess
import os
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed


def run_command(cmd, check=True):
    """执行命令并打印输出"""
    print(f"\n执行命令: {cmd}")
    print("-" * 60)
    result = subprocess.run(cmd, shell=True, check=check,
                           capture_output=False, text=True)
    if result.returncode != 0 and check:
        raise RuntimeError(f"命令执行失败: {cmd}")
    return result


def flash_firmware(port, firmware_path, baudrate=2000000):
    """
    烧录固件到单个设备

    参数:
        port: 串口端口（如 COM3）
        firmware_path: 固件文件路径（如 bin/3.0.38_R01C_ZH.bin）
        baudrate: 波特率（默认 2000000）
    """
    # 检查固件文件是否存在
    if not os.path.isfile(firmware_path):
        raise FileNotFoundError(f"固件文件不存在: {firmware_path}")

    print(f"\n=== 端口 {port} ===")
    print(f"固件文件: {firmware_path}")
    print(f"波特率: {baudrate}")

    # 执行烧录
    print("\n开始烧录到设备...")
    flash_cmd = (
        f"python -m esptool -p {port} -b {baudrate} "
        "--before default_reset --after hard_reset "
        "--chip esp32c6 "
        "write_flash "
        "--flash_mode dio --flash_freq 80m --flash_size 16MB "
        f"0x0 {firmware_path}"
    )
    run_command(flash_cmd)

    print("\n" + "=" * 60)
    print(f"[{port}] 烧录完成！")
    print(f"固件文件: {firmware_path}")


def flash_firmware_multi(ports, firmware_path, baudrate=2000000):
    """
    同时烧录多个设备

    参数:
        ports: 串口端口列表（如 ["COM3", "COM4"]）
        firmware_path: 固件文件路径
        baudrate: 波特率
    """
    # 去重并保持顺序
    seen = set()
    unique_ports = []
    for p in ports:
        if p not in seen:
            seen.add(p)
            unique_ports.append(p)

    if not unique_ports:
        raise ValueError("未指定任何有效端口")

    print("=" * 60)
    print(f"准备同时烧录 {len(unique_ports)} 个设备: {', '.join(unique_ports)}")
    print(f"固件文件: {firmware_path}")
    print(f"波特率: {baudrate}")
    print("=" * 60)

    errors = []

    with ThreadPoolExecutor(max_workers=len(unique_ports)) as executor:
        future_to_port = {
            executor.submit(flash_firmware, port, firmware_path, baudrate): port
            for port in unique_ports
        }

        for future in as_completed(future_to_port):
            port = future_to_port[future]
            try:
                future.result()
                print(f"\n[{port}] 烧录成功")
            except Exception as e:
                print(f"\n[{port}] 烧录失败: {e}")
                errors.append(port)

    if errors:
        raise RuntimeError(f"以下端口烧录失败: {', '.join(errors)}")
    else:
        print("\n所有端口烧录成功！")


def main():
    parser = argparse.ArgumentParser(
        description='ESP32C6 多端口并行烧录工具',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  单端口:
    python python_flash_multi.py -p COM3 bin/3.0.38_R01C_ZH.bin

  多端口（空格分隔）:
    python python_flash_multi.py -p COM3 COM4 bin/3.0.38_R01C_ZH.bin

  指定波特率:
    python python_flash_multi.py -p COM3 COM4 bin/3.0.38_R01C_ZH.bin --baudrate 460800
        """
    )

    parser.add_argument(
        '-p', '--ports',
        nargs='+',
        required=True,
        help='一个或多个串口端口（如 COM3 COM4 COM5）'
    )

    parser.add_argument(
        'firmware',
        help='固件文件路径（如 bin/3.0.38_R01C_ZH.bin）'
    )

    parser.add_argument(
        '--baudrate', '-b',
        type=int,
        default=2000000,
        help='波特率（默认: 2000000）'
    )

    args = parser.parse_args()

    try:
        flash_firmware_multi(args.ports, args.firmware, args.baudrate)
        return 0
    except Exception as e:
        print(f"\n错误: {e}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())


