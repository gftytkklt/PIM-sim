import pandas as pd
import re
import matplotlib.pyplot as plt
import numpy as np
from collections import defaultdict
def plot_simulator_stats(csv_file='completed_events.csv', output_image='module_timeline_heatmap.pdf'):
    # 读取并解析CSV数据
    data = []
    with open(csv_file, 'r') as f:
        for line in f:
            # 使用正则表达式提取所需信息
            module_match = re.search(r'Module: ([^,]+)', line)
            trigger_match = re.search(r'trigger_time=(\d+)', line)
            end_match = re.search(r'end_time=(\d+)', line)
            
            if module_match and trigger_match and end_match:
                module = module_match.group(1).strip()
                trigger_time = int(trigger_match.group(1))
                end_time = int(end_match.group(1))
                
                data.append({
                    'module': module,
                    'trigger_time': trigger_time,
                    'end_time': end_time
                })

    # 转换为DataFrame
    df = pd.DataFrame(data)
    print("解析的数据:")
    print(df)
    print(f"\n数据形状: {df.shape}")
    print(f"模块种类: {df['module'].unique()}")

    # 转换模块列为普通字符串数组
    modules = list(df['module'].unique())
    modules_sorted = sorted(modules)
    print(f"模块列表: {modules_sorted}")

    # 获取时间范围
    start_time = df['trigger_time'].min()
    end_time = df['end_time'].max()
    print(f"时间范围: {start_time} - {end_time}")

    # 创建图表
    fig, ax = plt.subplots(figsize=(12, 6))

    # 为每个模块分配一个Y坐标
    module_y_positions = {module: i for i, module in enumerate(modules_sorted)}

    # 选择颜色映射
    colors = plt.cm.tab20c(np.linspace(0, 1, len(modules_sorted)))
    module_colors = {module: colors[i] for i, module in enumerate(modules_sorted)}

    # 绘制每个时间区间
    legend_labels = set()
    for idx, row in df.iterrows():
        module = row['module']
        start = row['trigger_time']
        end = row['end_time']
        y_pos = module_y_positions[module]
        
        # 绘制矩形表示活动区间
        if module not in legend_labels:
            ax.barh(y_pos, end-start, left=start, height=0.5, color=module_colors[module], label=module)
            legend_labels.add(module)
        else:
            ax.barh(y_pos, end-start, left=start, height=0.5, color=module_colors[module])

    # 设置Y轴标签
    ax.set_yticks(range(len(modules_sorted)))
    ax.set_yticklabels(modules_sorted)

    # 设置轴标签
    ax.set_xlabel('time (cycles)')
    ax.set_ylabel('modules')
    ax.set_title('8x8 fmap inference timeline heatmap')

    # 添加图例
    ax.legend()

    # 设置网格
    ax.grid(True, axis='x', alpha=0.3)

    # 保存图像
    # heatmap_image = 'module_timeline_heatmap.pdf'
    plt.tight_layout()
    plt.savefig(output_image, dpi=300)

def create_core_clustered_timeline(csv_file_path="banking_completed_events_YX.csv", output_file='core_timeline_YX.pdf',title = 'module timeline heatmap'):
    """
    为包含模块和核心编号的CSV数据创建时间轴热力图（无Y轴标签版本）
    
    参数:
    csv_file_path: CSV文件路径
    output_file: 输出图片文件名
    
    返回:
    DataFrame: 包含解析后数据的DataFrame
    """
    # 读取并解析CSV数据
    data = []
    
    with open(csv_file_path, 'r') as f:
        for line_num, line in enumerate(f, 1):
            line = line.strip()
            if not line:
                continue
                
            try:
                # 使用正则表达式提取所需信息
                module_match = re.search(r'Module:\s*([^,]+)', line)
                trigger_match = re.search(r'trigger_time\s*=\s*(\d+)', line)
                end_match = re.search(r'end_time\s*=\s*(\d+)', line)
                
                if module_match and trigger_match and end_match:
                    module = module_match.group(1).strip()
                    trigger_time = int(trigger_match.group(1))
                    end_time = int(end_match.group(1))
                    
                    # 提取core编号（假设模块名称以数字结尾）
                    core_match = re.search(r'(\d+)$', module)
                    if core_match:
                        core_num = int(core_match.group(1))
                    else:
                        core_num = -1  # 没有数字的情况
                    
                    data.append({
                        'module': module,
                        'core': core_num,
                        'trigger_time': trigger_time,
                        'end_time': end_time,
                        'duration': end_time - trigger_time
                    })
                else:
                    print(f"警告: 第{line_num}行格式不正确: {line}")
            except Exception as e:
                print(f"错误: 解析第{line_num}行时出错: {e}")
    
    if not data:
        print("错误: 没有解析到有效数据")
        return None
    
    # 转换为DataFrame
    df = pd.DataFrame(data)
    
    print(f"成功解析 {len(df)} 行数据")
    print(f"发现 {df['core'].nunique()} 个不同的core")
    print(f"发现 {df['module'].nunique()} 个不同的模块")
    
    # 按core编号和模块名称排序
    df = df.sort_values(['core', 'module', 'trigger_time']).reset_index(drop=True)
    
    # 创建图表
    fig, ax = plt.subplots(figsize=(16, 10))
    
    # 获取核心和模块信息
    cores = sorted(df['core'].unique())
    core_modules = {}
    for core in cores:
        core_modules[core] = sorted(df[df['core'] == core]['module'].unique())
    
    # 创建模块到y位置的映射
    module_to_y = {}
    y_pos = 0
    core_separator_positions = []
    
    for core in cores:
        if core >= 0:  # 只有有编号的核心才添加分隔线
            core_separator_positions.append(y_pos - 0.5)
        
        for module in core_modules[core]:
            module_to_y[module] = y_pos
            y_pos += 1
        
        y_pos += 0.5  # 添加间隔
    
    # 设置颜色
    modules = list(module_to_y.keys())
    colors = plt.cm.tab20(np.linspace(0, 1, len(modules)))
    module_colors = {module: colors[i] for i, module in enumerate(modules)}
    
    # 绘制每个时间区间
    for idx, row in df.iterrows():
        module = row['module']
        start = row['trigger_time']
        end = row['end_time']
        y = module_to_y[module]
        
        # 绘制矩形
        rect = plt.Rectangle((start, y-0.4), end-start, 0.8,
                           color=module_colors[module], alpha=0.7, edgecolor='black')
        ax.add_patch(rect)
        
        # 添加持续时间标签（如果区间足够长）
        duration = end - start
        if duration >= 10:
            ax.text(start + duration/2, y, str(duration),
                   ha='center', va='center', fontsize=8, color='white', fontweight='bold')
    
    # 添加核心分隔线
    for sep_pos in core_separator_positions:
        if sep_pos > 0:  # 跳过第一个
            ax.axhline(y=sep_pos, color='gray', linestyle='--', alpha=0.5, linewidth=1)
    
    # 设置Y轴 - 移除模块名称标签
    y_ticks = [module_to_y[module] for module in modules]
    ax.set_yticks(y_ticks)
    ax.set_yticklabels([])  # 移除Y轴标签
    
    # 设置X轴
    min_time = df['trigger_time'].min()
    max_time = df['end_time'].max()
    time_padding = (max_time - min_time) * 0.05
    ax.set_xlim(min_time - time_padding, max_time + time_padding)
    ax.set_ylim(-0.5, max(y_ticks) + 0.5)
    
    # 添加核心标签
    for core in cores:
        if core_modules[core]:
            first_module = core_modules[core][0]
            y_pos_label = module_to_y[first_module]
            if core >= 0:
                label = f'Core {core}'
            else:
                label = '无核心'
            ax.text(min_time - time_padding, y_pos_label, label,
                   ha='right', va='center', fontsize=11, fontweight='bold',
                   bbox=dict(boxstyle='round,pad=0.3', facecolor='lightblue', alpha=0.7))
    
    # 设置X轴标签和标题
    ax.set_xlabel('time', fontsize=12)
    ax.set_ylabel('')  # 清空Y轴标签
    ax.set_title(title, fontsize=16, fontweight='bold')
    
    # 添加网格
    ax.grid(True, axis='x', alpha=0.3, linestyle=':')
    
    # 添加图例 - 保留图例
    from matplotlib.patches import Patch
    legend_patches = [Patch(facecolor=module_colors[mod], label=mod, alpha=0.7) for mod in modules]
    ax.legend(handles=legend_patches, title='modules', bbox_to_anchor=(1.05, 1), loc='upper left')
    
    # 调整布局并保存
    plt.tight_layout()
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    
    print(f"图表已保存为: {output_file}")
    print(f"时间范围: {min_time} 到 {max_time}")
    print(f"模块布局:")
    for core in cores:
        print(f"  Core {core}: {', '.join(core_modules[core])}")
    
    return df


# 使用示例
# print("正在创建时间轴热力图...")
# result_df = create_core_clustered_timeline('timing_data_core.csv', 'final_core_timeline.png')
# print("完成!")

if __name__ == "__main__":
    # plot_simulator_stats()
    plot_simulator_stats()
    create_core_clustered_timeline('banking_completed_events_XY.csv', 'core_timeline_XY.pdf', title='XY tiling timeline heatmap')
    create_core_clustered_timeline('banking_completed_events_YX.csv', 'core_timeline_YX.pdf', title='YX tiling timeline heatmap')
    create_core_clustered_timeline('banking_completed_events_16x4.csv', 'core_timeline_16x4.pdf', title='16x4 fmap tiling timeline heatmap')
    create_core_clustered_timeline('multicore_completed_events.csv', 'multicore_completed_events.pdf', title='AlexNet inference timeline heatmap')