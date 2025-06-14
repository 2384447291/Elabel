import shutil
import os

# 要复制的文件路径
files_to_copy = [
    'build/bootloader/bootloader.bin',
    'build/E_lable.bin',
    'build/partition_table/partition-table.bin',
    'build/ota_data_initial.bin'
]

# 目标目录（建议不要直接用系统根目录 /bin，改为项目内 bin 目录）
target_dir = 'bin'

# 创建目标目录（如果不存在）
os.makedirs(target_dir, exist_ok=True)

# 执行复制操作
for file_path in files_to_copy:
    if os.path.isfile(file_path):
        shutil.copy(file_path, target_dir)
        print(f"已复制 {file_path} 到 {target_dir}")
    else:
        print(f"文件未找到: {file_path}")
