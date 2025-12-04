import subprocess
import re
import os
import shutil

# 读取版本信息
def read_version_info():
    """从 global_version.h 读取版本信息"""
    version_file = 'main/global_message/global_version.h'
    
    if not os.path.isfile(version_file):
        raise FileNotFoundError(f"未找到版本文件: {version_file}")
    
    with open(version_file, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # 提取宏定义的值
    firmware_version = re.search(r'#define\s+FIRMWARE_VERSION\s+"([^"]+)"', content)
    device_model = re.search(r'#define\s+DEVICE_MODEL\s+"([^"]+)"', content)
    language = re.search(r'#define\s+LANGUAGE\s+"([^"]+)"', content)
    
    if not firmware_version or not device_model or not language:
        raise ValueError("无法从版本文件中提取完整的版本信息")
    
    version = firmware_version.group(1)
    model = device_model.group(1)
    lang = language.group(1)
    
    return version, model, lang

# 执行命令
def run_command(cmd, check=True):
    """执行命令并打印输出"""
    print(f"\n执行命令: {cmd}")
    print("-" * 60)
    result = subprocess.run(cmd, shell=True, check=check, 
                           capture_output=False, text=True)
    if result.returncode != 0 and check:
        raise RuntimeError(f"命令执行失败: {cmd}")
    return result

def main():
    try:
        # 读取版本信息
        print("正在读取版本信息...")
        version, model, lang = read_version_info()
        print(f"版本: {version}, 型号: {model}, 语言: {lang}")
        
        # 生成文件名
        filename = f"{version}_{model}_{lang}.bin"
        bin_dir = "bin"
        bin_filepath = os.path.join(bin_dir, filename)
        print(f"\n目标文件名: {bin_filepath}")
        
        # 创建bin文件夹（如果不存在）
        os.makedirs(bin_dir, exist_ok=True)
        
        # 执行构建
        print("\n开始构建项目...")
        run_command("idf.py build")
        
        # 执行合并bin文件（idf.py merge-bin 在 build 目录下执行，所以先合并到 build 目录）
        print("\n开始合并bin文件...")
        temp_filename = filename  # 临时文件名（在 build 目录下）
        run_command(f"idf.py merge-bin -o {temp_filename} -f raw")
        
        # 检查 build 目录下的文件是否存在
        build_filepath = os.path.join("build", temp_filename)
        if not os.path.isfile(build_filepath):
            raise FileNotFoundError(f"合并后的文件不存在: {build_filepath}")
        
        # 移动文件到项目根目录的 bin 文件夹
        if os.path.isfile(bin_filepath):
            print(f"\n文件 {bin_filepath} 已存在，将被覆盖")
            os.remove(bin_filepath)
        
        shutil.move(build_filepath, bin_filepath)
        print(f"\n已生成合并文件: {bin_filepath}")
        
        # 复制 E_lable.bin 作为 OTA 文件
        ota_filename = f"{version}_{model}_{lang}_ota.bin"
        ota_filepath = os.path.join(bin_dir, ota_filename)
        elable_bin_path = os.path.join("build", "E_lable.bin")
        
        if not os.path.isfile(elable_bin_path):
            raise FileNotFoundError(f"E_lable.bin 文件不存在: {elable_bin_path}")
        
        if os.path.isfile(ota_filepath):
            print(f"\nOTA 文件 {ota_filepath} 已存在，将被覆盖")
            os.remove(ota_filepath)
        
        shutil.copy2(elable_bin_path, ota_filepath)
        print(f"\n已生成 OTA 文件: {ota_filepath}")
        
        # 执行烧录
        print("\n开始烧录到设备...")
        flash_cmd = (
            "python -m esptool -p COM3 -b 2000000 "
            "--before default_reset --after hard_reset "
            "--chip esp32c6 "
            "write_flash "
            "--flash_mode dio --flash_freq 80m --flash_size 16MB "
            f"0x0 {bin_filepath}"
        )
        run_command(flash_cmd)
        
        print("\n" + "=" * 60)
        print("烧录完成！")
        print(f"完整固件文件: {bin_filepath}")
        print(f"OTA 固件文件: {ota_filepath}")
        
    except Exception as e:
        print(f"\n错误: {e}")
        return 1
    
    return 0

if __name__ == "__main__":
    exit(main())

