import os
import struct

def find_non_silent(data, threshold=10):
    """查找非静音部分的起始和结束位置"""
    # 找到第一个非静音位置
    start = 0
    while start < len(data) and data[start] < threshold:
        start += 1
    
    # 找到最后一个非静音位置
    end = len(data) - 1
    while end > start and data[end] < threshold:
        end -= 1
    
    return start, end

def pcm_to_array(pcm_file):
    """将PCM文件转换为C++数组"""
    array_name = os.path.splitext(os.path.basename(pcm_file))[0]
    array_content = []
    
    with open(pcm_file, 'rb') as f:
        # 读取PCM数据
        data = f.read()
        
        # 找到非静音部分
        start, end = find_non_silent(data)
        
        # 只处理非静音部分
        data = data[start:end+1]
        
        # 每个字节转换为一个uint8_t
        for i in range(0, len(data)):
            value = struct.unpack('B', data[i:i+1])[0]
            array_content.append(str(value))
    
    # 生成C++数组声明
    array_declaration = f"const uint8_t {array_name}[] = {{\n"
    
    # 每行显示16个元素
    ELEMENTS_PER_LINE = 16
    for i in range(0, len(array_content), ELEMENTS_PER_LINE):
        line = array_content[i:i + ELEMENTS_PER_LINE]
        array_declaration += "    " + ", ".join(line) + ",\n"
    
    array_declaration += f"}};\nconst size_t {array_name}_size = sizeof({array_name});\n"
    
    return array_declaration

def main():
    # 获取当前目录下所有的PCM文件
    pcm_files = [f for f in os.listdir('.') if f.endswith('.pcm')]
    
    # 生成music.hpp的内容
    hpp_content = """#ifndef MUSIC_HPP
#define MUSIC_HPP

#include <cstdint>
#include <cstddef>

"""
    
    # 处理每个PCM文件
    for pcm_file in pcm_files:
        hpp_content += pcm_to_array(pcm_file) + "\n"
    
    hpp_content += "\n#endif // MUSIC_HPP"
    
    # 确保include目录存在
    if not os.path.exists('include'):
        os.makedirs('include')
    
    # 写入music.hpp文件到include目录
    with open('include/music.hpp', 'w') as f:
        f.write(hpp_content)
    
    print("已生成 include/music.hpp 文件")

if __name__ == "__main__":
    main()
