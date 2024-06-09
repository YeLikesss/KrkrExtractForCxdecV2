# KrkrExtractV2 (ForCxdecV2) 动态工具集
&emsp;&emsp;适用于Wamsoft(Hxv4 2021.11+)加密解包/字符串<->Hash提取

## 环境
&emsp;&emsp;系统: Windows 7 SP1 x64

&emsp;&emsp;IDE: Visual Sudio 2022

&emsp;&emsp;编译器: MSVC2022 x86

## 与老版本区别
&emsp;&emsp;1.重构代码, 稍微看着没那么屎山

&emsp;&emsp;2.功能模块拆分为不同Dll

&emsp;&emsp;3.修复许多bug

## 如何使用
&emsp;&emsp;1. `CxdecExtractorLoader.exe`, `CxdecExtractor.dll`, `CxdecExtractorUI.dll`, `CxdecStringDumper.dll`保持同一目录

&emsp;&emsp;2. 保证你的游戏是Wamsoft KrkrZ Hxv4加密类型且加密认证已移除

&emsp;&emsp;3. 拖拽游戏exe到`CxdecExtractorLoader.exe`启动, 弹出模块选择对话框

&emsp;&emsp;4. 选择`加载解包模块`, 弹出解包对话框, 拖拽`xxx.xp3`到框内解包

&emsp;&emsp;&emsp;4.1 `游戏目录\Extractor_Output\`为输出目录, 包含`xxx文件夹`的封包资源与`xxx.alst`的文件表

&emsp;&emsp;&emsp;4.2 `工具目录\Extractor.log`为日志信息

&emsp;&emsp;5. 选择`加载字符串Hash提取模块`, 自动提取游戏运行时的字符串Hash映射表

&emsp;&emsp;&emsp;5.1 `游戏目录\StringHashDumper_Output\`为输出目录

&emsp;&emsp;&emsp;5.2 `DirectoryHash.log`为文件夹路径Hash映射表

&emsp;&emsp;&emsp;5.3 `FileNameHash.log`为文件名Hash映射表

&emsp;&emsp;&emsp;5.4 `Universal.log`为通用信息(Hash加密参数)

&emsp;&emsp;6. 选择`加载Key提取模块`(功能暂未实现)

&emsp;&emsp;7. 工具不会申请管理员权限进行弹出UAC提权, 游戏与工具务必不要放在C盘

&emsp;&emsp;8. 如出现错误标题的弹窗报错, 请检查上述步骤

## 常见问题
&emsp;&emsp;Q: 为什么没有资源文件名

&emsp;&emsp;A: 封包里面本来就没有文件名

&emsp;&emsp;Q: 解包对话框支持批量拖拽解包吗

&emsp;&emsp;A: 不支持, 仅支持单个封包逐个拖拽提取

&emsp;&emsp;Q: 解包响应框解包时候无响应

&emsp;&emsp;A: 没做多线程支持, 等它慢慢解完就好

&emsp;&emsp;Q: Hash映射表能一次性提取所有吗

&emsp;&emsp;A: 不能, 名字在脚本里面散落到处都是, 且不全

&emsp;&emsp;Q: 兼容Win7以外的系统吗

&emsp;&emsp;A: 理论上兼容, 不过没有测试, 有问题我也不知道

## 同类工具推荐

 * KrkrExtractV2 (ForCxdecV2)(本工具)&emsp; 类型: 动态 &emsp; 解包: 一次性 &emsp;文件名: 运行时

 * [KrkrDump](https://github.com/crskycode/KrkrDump)&emsp; 类型: 动态 &emsp; 解包: 运行时 &emsp;文件名: 运行时

 * [GARBro](https://github.com/crskycode/GARbro)&emsp; 类型: 静态(需人工装填) &emsp; 解包: 一次性 &emsp;文件名: 无