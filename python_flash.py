#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
烧录脚本 - 通过命令行参数指定端口和固件文件
用法: python python_flash.py <端口> <固件文件路径>
示例: python python_flash.py COM3 bin/3.0.39_R01C_EN.bin
"""

import subprocess
import sys
import os
import argparse

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
    烧录固件到设备
    
    参数:
        port: 串口端口（如 COM3）
        firmware_path: 固件文件路径（如 bin/3.0.38_R01C_ZH.bin）
        baudrate: 波特率（默认 2000000）
    """
    # 检查固件文件是否存在
    if not os.path.isfile(firmware_path):
        raise FileNotFoundError(f"固件文件不存在: {firmware_path}")
    
    print(f"端口: {port}")
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
    print("烧录完成！")
    print(f"固件文件: {firmware_path}")

def main():
    parser = argparse.ArgumentParser(
        description='ESP32C6 固件烧录工具',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  python python_flash.py COM3 bin/3.0.38_R01C_ZH.bin
  python python_flash.py COM3 bin/3.0.38_R01C_ZH.bin --baudrate 460800
        """
    )
    
    parser.add_argument(
        'port',
        help='串口端口（如 COM3）'
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
        flash_firmware(args.port, args.firmware, args.baudrate)
        return 0
    except Exception as e:
        print(f"\n错误: {e}")
        return 1

if __name__ == "__main__":
    exit(main())

